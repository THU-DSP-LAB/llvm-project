//===-- RISCVISelDAGToDAG.cpp - A dag to dag inst selector for RISCV ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the RISCV target.
//
//===----------------------------------------------------------------------===//

#include "RISCVISelDAGToDAG.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "MCTargetDesc/RISCVMatInt.h"
#include "RISCVISelLowering.h"
#include "RISCVMachineFunctionInfo.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-isel"

static unsigned getLastNonGlueOrChainOpIdx(const SDNode *Node) {
  assert(Node->getNumOperands() > 0 && "Node with no operands");
  unsigned LastOpIdx = Node->getNumOperands() - 1;
  if (Node->getOperand(LastOpIdx).getValueType() == MVT::Glue)
    --LastOpIdx;
  if (Node->getOperand(LastOpIdx).getValueType() == MVT::Other)
    --LastOpIdx;
  return LastOpIdx;
}

void RISCVDAGToDAGISel::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LegacyDivergenceAnalysis>();
  SelectionDAGISel::getAnalysisUsage(AU);
}

void RISCVDAGToDAGISel::PostprocessISelDAG() {
  HandleSDNode Dummy(CurDAG->getRoot());
  SelectionDAG::allnodes_iterator Position = CurDAG->allnodes_end();

  bool MadeChange = false;
  while (Position != CurDAG->allnodes_begin()) {
    SDNode *N = &*--Position;
    // Skip dead nodes and any non-machine opcodes.
    if (N->use_empty() || !N->isMachineOpcode())
      continue;

    MadeChange |= doPeepholeSExtW(N);
  }

  CurDAG->setRoot(Dummy.getValue());

  if (MadeChange)
    CurDAG->RemoveDeadNodes();
}

static SDNode *selectImmSeq(SelectionDAG *CurDAG, const SDLoc &DL, const MVT VT,
                            RISCVMatInt::InstSeq &Seq) {
  SDNode *Result = nullptr;
  SDValue SrcReg = CurDAG->getRegister(RISCV::X0, VT);
  for (RISCVMatInt::Inst &Inst : Seq) {
    SDValue SDImm = CurDAG->getTargetConstant(Inst.Imm, DL, VT);
    switch (Inst.getOpndKind()) {
    case RISCVMatInt::Imm:
      Result = CurDAG->getMachineNode(Inst.Opc, DL, VT, SDImm);
      break;
    case RISCVMatInt::RegX0:
      Result = CurDAG->getMachineNode(Inst.Opc, DL, VT, SrcReg,
                                      CurDAG->getRegister(RISCV::X0, VT));
      break;
    case RISCVMatInt::RegReg:
      Result = CurDAG->getMachineNode(Inst.Opc, DL, VT, SrcReg, SrcReg);
      break;
    case RISCVMatInt::RegImm:
      Result = CurDAG->getMachineNode(Inst.Opc, DL, VT, SrcReg, SDImm);
      break;
    }

    // Only the first instruction has X0 as its source.
    SrcReg = SDValue(Result, 0);
  }

  return Result;
}

static SDNode *selectImm(SelectionDAG *CurDAG, const SDLoc &DL, const MVT VT,
                         int64_t Imm, const RISCVSubtarget &Subtarget) {
  RISCVMatInt::InstSeq Seq =
      RISCVMatInt::generateInstSeq(Imm, Subtarget.getFeatureBits());

  return selectImmSeq(CurDAG, DL, VT, Seq);
}

bool RISCVDAGToDAGISel::tryShrinkShlLogicImm(SDNode *Node) {
  MVT VT = Node->getSimpleValueType(0);
  unsigned Opcode = Node->getOpcode();
  assert((Opcode == ISD::AND || Opcode == ISD::OR || Opcode == ISD::XOR) &&
         "Unexpected opcode");
  SDLoc DL(Node);

  // For operations of the form (x << C1) op C2, check if we can use
  // ANDI/ORI/XORI by transforming it into (x op (C2>>C1)) << C1.
  SDValue N0 = Node->getOperand(0);
  SDValue N1 = Node->getOperand(1);

  ConstantSDNode *Cst = dyn_cast<ConstantSDNode>(N1);
  if (!Cst)
    return false;

  int64_t Val = Cst->getSExtValue();

  // Check if immediate can already use ANDI/ORI/XORI.
  if (isInt<12>(Val))
    return false;

  SDValue Shift = N0;

  // If Val is simm32 and we have a sext_inreg from i32, then the binop
  // produces at least 33 sign bits. We can peek through the sext_inreg and use
  // a SLLIW at the end.
  bool SignExt = false;
  if (isInt<32>(Val) && N0.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      N0.hasOneUse() && cast<VTSDNode>(N0.getOperand(1))->getVT() == MVT::i32) {
    SignExt = true;
    Shift = N0.getOperand(0);
  }

  if (Shift.getOpcode() != ISD::SHL || !Shift.hasOneUse())
    return false;

  ConstantSDNode *ShlCst = dyn_cast<ConstantSDNode>(Shift.getOperand(1));
  if (!ShlCst)
    return false;

  uint64_t ShAmt = ShlCst->getZExtValue();

  // Make sure that we don't change the operation by removing bits.
  // This only matters for OR and XOR, AND is unaffected.
  uint64_t RemovedBitsMask = maskTrailingOnes<uint64_t>(ShAmt);
  if (Opcode != ISD::AND && (Val & RemovedBitsMask) != 0)
    return false;

  int64_t ShiftedVal = Val >> ShAmt;
  if (!isInt<12>(ShiftedVal))
    return false;

  // If we peeked through a sext_inreg, make sure the shift is valid for SLLIW.
  if (SignExt && ShAmt >= 32)
    return false;

  // Ok, we can reorder to get a smaller immediate.
  unsigned BinOpc;
  switch (Opcode) {
  default: llvm_unreachable("Unexpected opcode");
  case ISD::AND: BinOpc = RISCV::ANDI; break;
  case ISD::OR:  BinOpc = RISCV::ORI;  break;
  case ISD::XOR: BinOpc = RISCV::XORI; break;
  }

  unsigned ShOpc = SignExt ? RISCV::SLLIW : RISCV::SLLI;

  SDNode *BinOp =
      CurDAG->getMachineNode(BinOpc, DL, VT, Shift.getOperand(0),
                             CurDAG->getTargetConstant(ShiftedVal, DL, VT));
  SDNode *SLLI =
      CurDAG->getMachineNode(ShOpc, DL, VT, SDValue(BinOp, 0),
                             CurDAG->getTargetConstant(ShAmt, DL, VT));
  ReplaceNode(Node, SLLI);
  return true;
}

void RISCVDAGToDAGISel::Select(SDNode *Node) {
  // If we have a custom node, we have already selected.
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(dbgs() << "== "; Node->dump(CurDAG); dbgs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // Instruction Selection not handled by the auto-generated tablegen selection
  // should be handled here.
  unsigned Opcode = Node->getOpcode();
  MVT XLenVT = Subtarget->getXLenVT();
  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);
  switch (Opcode) {
  case ISD::Constant: {
    auto *ConstNode = cast<ConstantSDNode>(Node);
    if (VT == XLenVT && ConstNode->isZero()) {
      SDValue New =
          CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, RISCV::X0, XLenVT);
      ReplaceNode(Node, New.getNode());
      return;
    }
    int64_t Imm = ConstNode->getSExtValue();
    // If the upper XLen-16 bits are not used, try to convert this to a simm12
    // by sign extending bit 15.
    if (isUInt<16>(Imm) && isInt<12>(SignExtend64<16>(Imm)) &&
        hasAllHUsers(Node))
      Imm = SignExtend64<16>(Imm);
    // If the upper 32-bits are not used try to convert this into a simm32 by
    // sign extending bit 32.
    if (!isInt<32>(Imm) && isUInt<32>(Imm) && hasAllWUsers(Node))
      Imm = SignExtend64<32>(Imm);

    ReplaceNode(Node, selectImm(CurDAG, DL, VT, Imm, *Subtarget));
    return;
  }
  case ISD::SHL: {
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::AND || !N0.hasOneUse() ||
        !isa<ConstantSDNode>(N0.getOperand(1)))
      break;
    unsigned ShAmt = N1C->getZExtValue();
    uint64_t Mask = N0.getConstantOperandVal(1);

    // Optimize (shl (and X, C2), C) -> (slli (srliw X, C3), C3+C) where C2 has
    // 32 leading zeros and C3 trailing zeros.
    if (ShAmt <= 32 && isShiftedMask_64(Mask)) {
      unsigned XLen = Subtarget->getXLen();
      unsigned LeadingZeros = XLen - (64 - countLeadingZeros(Mask));
      unsigned TrailingZeros = countTrailingZeros(Mask);
      if (TrailingZeros > 0 && LeadingZeros == 32) {
        SDNode *SRLIW = CurDAG->getMachineNode(
            RISCV::SRLIW, DL, VT, N0->getOperand(0),
            CurDAG->getTargetConstant(TrailingZeros, DL, VT));
        SDNode *SLLI = CurDAG->getMachineNode(
            RISCV::SLLI, DL, VT, SDValue(SRLIW, 0),
            CurDAG->getTargetConstant(TrailingZeros + ShAmt, DL, VT));
        ReplaceNode(Node, SLLI);
        return;
      }
    }
    break;
  }
//   case ISD::SRL: {
//     auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
//     if (!N1C)
//       break;
//     SDValue N0 = Node->getOperand(0);
//     if (N0.getOpcode() != ISD::AND || !isa<ConstantSDNode>(N0.getOperand(1)))
//       break;
//     unsigned ShAmt = N1C->getZExtValue();
//     uint64_t Mask = N0.getConstantOperandVal(1);

//     // Optimize (srl (and X, C2), C) -> (slli (srliw X, C3), C3-C) where C2 has
//     // 32 leading zeros and C3 trailing zeros.
//     if (isShiftedMask_64(Mask) && N0.hasOneUse()) {
//       unsigned XLen = Subtarget->getXLen();
//       unsigned LeadingZeros = XLen - (64 - countLeadingZeros(Mask));
//       unsigned TrailingZeros = countTrailingZeros(Mask);
//       if (LeadingZeros == 32 && TrailingZeros > ShAmt) {
//         SDNode *SRLIW = CurDAG->getMachineNode(
//             RISCV::SRLIW, DL, VT, N0->getOperand(0),
//             CurDAG->getTargetConstant(TrailingZeros, DL, VT));
//         SDNode *SLLI = CurDAG->getMachineNode(
//             RISCV::SLLI, DL, VT, SDValue(SRLIW, 0),
//             CurDAG->getTargetConstant(TrailingZeros - ShAmt, DL, VT));
//         ReplaceNode(Node, SLLI);
//         return;
//       }
//     }

//     // Optimize (srl (and X, C2), C) ->
//     //          (srli (slli X, (XLen-C3), (XLen-C3) + C)
//     // Where C2 is a mask with C3 trailing ones.
//     // Taking into account that the C2 may have had lower bits unset by
//     // SimplifyDemandedBits. This avoids materializing the C2 immediate.
//     // This pattern occurs when type legalizing right shifts for types with
//     // less than XLen bits.
//     Mask |= maskTrailingOnes<uint64_t>(ShAmt);
//     if (!isMask_64(Mask))
//       break;
//     unsigned TrailingOnes = countTrailingOnes(Mask);
//     if (ShAmt >= TrailingOnes)
//       break;
//     // If the mask has 32 trailing ones, use SRLIW.
//     if (TrailingOnes == 32) {
//       SDNode *SRLIW =
//           CurDAG->getMachineNode(RISCV::SRLIW, DL, VT, N0->getOperand(0),
//                                  CurDAG->getTargetConstant(ShAmt, DL, VT));
//       ReplaceNode(Node, SRLIW);
//       return;
//     }

//     // Only do the remaining transforms if the shift has one use.
//     if (!N0.hasOneUse())
//       break;

//     // If C2 is (1 << ShAmt) use bexti if possible.
//     if (Subtarget->hasStdExtZbs() && ShAmt + 1 == TrailingOnes) {
//       SDNode *BEXTI =
//           CurDAG->getMachineNode(RISCV::BEXTI, DL, VT, N0->getOperand(0),
//                                  CurDAG->getTargetConstant(ShAmt, DL, VT));
//       ReplaceNode(Node, BEXTI);
//       return;
//     }
//     unsigned LShAmt = Subtarget->getXLen() - TrailingOnes;
//     SDNode *SLLI =
//         CurDAG->getMachineNode(RISCV::SLLI, DL, VT, N0->getOperand(0),
//                                CurDAG->getTargetConstant(LShAmt, DL, VT));
//     SDNode *SRLI = CurDAG->getMachineNode(
//         RISCV::SRLI, DL, VT, SDValue(SLLI, 0),
//         CurDAG->getTargetConstant(LShAmt + ShAmt, DL, VT));
//     ReplaceNode(Node, SRLI);
//     return;
//   }
  case ISD::SRA: {
    // Optimize (sra (sext_inreg X, i16), C) ->
    //          (srai (slli X, (XLen-16), (XLen-16) + C)
    // And      (sra (sext_inreg X, i8), C) ->
    //          (srai (slli X, (XLen-8), (XLen-8) + C)
    // This can occur when Zbb is enabled, which makes sext_inreg i16/i8 legal.
    // This transform matches the code we get without Zbb. The shifts are more
    // compressible, and this can help expose CSE opportunities in the sdiv by
    // constant optimization.
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::SIGN_EXTEND_INREG || !N0.hasOneUse())
      break;
    unsigned ShAmt = N1C->getZExtValue();
    unsigned ExtSize =
        cast<VTSDNode>(N0.getOperand(1))->getVT().getSizeInBits();
    // ExtSize of 32 should use sraiw via tablegen pattern.
    if (ExtSize >= 32 || ShAmt >= ExtSize)
      break;
    unsigned LShAmt = Subtarget->getXLen() - ExtSize;
    SDNode *SLLI =
        CurDAG->getMachineNode(RISCV::SLLI, DL, VT, N0->getOperand(0),
                               CurDAG->getTargetConstant(LShAmt, DL, VT));
    SDNode *SRAI = CurDAG->getMachineNode(
        RISCV::SRAI, DL, VT, SDValue(SLLI, 0),
        CurDAG->getTargetConstant(LShAmt + ShAmt, DL, VT));
    ReplaceNode(Node, SRAI);
    return;
  }
  case ISD::OR:
  case ISD::XOR:
    if (tryShrinkShlLogicImm(Node))
      return;

    break;
  // case ISD::AND: {
  //   auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
  //   if (!N1C)
  //     break;

  //   SDValue N0 = Node->getOperand(0);

  //   bool LeftShift = N0.getOpcode() == ISD::SHL;
  //   if (LeftShift || N0.getOpcode() == ISD::SRL) {
  //     auto *C = dyn_cast<ConstantSDNode>(N0.getOperand(1));
  //     if (!C)
  //       break;
  //     unsigned C2 = C->getZExtValue();
  //     unsigned XLen = Subtarget->getXLen();
  //     assert((C2 > 0 && C2 < XLen) && "Unexpected shift amount!");

  //     uint64_t C1 = N1C->getZExtValue();

  //     // Keep track of whether this is a c.andi. If we can't use c.andi, the
  //     // shift pair might offer more compression opportunities.
  //     // TODO: We could check for C extension here, but we don't have many lit
  //     // tests with the C extension enabled so not checking gets better
  //     // coverage.
  //     // TODO: What if ANDI faster than shift?
  //     bool IsCANDI = isInt<6>(N1C->getSExtValue());

  //     // Clear irrelevant bits in the mask.
  //     if (LeftShift)
  //       C1 &= maskTrailingZeros<uint64_t>(C2);
  //     else
  //       C1 &= maskTrailingOnes<uint64_t>(XLen - C2);

  //     // Some transforms should only be done if the shift has a single use or
  //     // the AND would become (srli (slli X, 32), 32)
  //     bool OneUseOrZExtW = N0.hasOneUse() || C1 == UINT64_C(0xFFFFFFFF);

  //     SDValue X = N0.getOperand(0);

  //     // Turn (and (srl x, c2) c1) -> (srli (slli x, c3-c2), c3) if c1 is a mask
  //     // with c3 leading zeros.
  //     if (!LeftShift && isMask_64(C1)) {
  //       unsigned Leading = XLen - (64 - countLeadingZeros(C1));
  //       if (C2 < Leading) {
  //         // If the number of leading zeros is C2+32 this can be SRLIW.
  //         if (C2 + 32 == Leading) {
  //           SDNode *SRLIW = CurDAG->getMachineNode(
  //               RISCV::SRLIW, DL, VT, X, CurDAG->getTargetConstant(C2, DL, VT));
  //           ReplaceNode(Node, SRLIW);
  //           return;
  //         }

  //         // (and (srl (sexti32 Y), c2), c1) -> (srliw (sraiw Y, 31), c3 - 32)
  //         // if c1 is a mask with c3 leading zeros and c2 >= 32 and c3-c2==1.
  //         //
  //         // This pattern occurs when (i32 (srl (sra 31), c3 - 32)) is type
  //         // legalized and goes through DAG combine.
  //         if (C2 >= 32 && (Leading - C2) == 1 && N0.hasOneUse() &&
  //             X.getOpcode() == ISD::SIGN_EXTEND_INREG &&
  //             cast<VTSDNode>(X.getOperand(1))->getVT() == MVT::i32) {
  //           SDNode *SRAIW =
  //               CurDAG->getMachineNode(RISCV::SRAIW, DL, VT, X.getOperand(0),
  //                                      CurDAG->getTargetConstant(31, DL, VT));
  //           SDNode *SRLIW = CurDAG->getMachineNode(
  //               RISCV::SRLIW, DL, VT, SDValue(SRAIW, 0),
  //               CurDAG->getTargetConstant(Leading - 32, DL, VT));
  //           ReplaceNode(Node, SRLIW);
  //           return;
  //         }

  //         // (srli (slli x, c3-c2), c3).
  //         // Skip if we could use (zext.w (sraiw X, C2)).
  //         bool Skip = Subtarget->hasStdExtZba() && Leading == 32 &&
  //                     X.getOpcode() == ISD::SIGN_EXTEND_INREG &&
  //                     cast<VTSDNode>(X.getOperand(1))->getVT() == MVT::i32;
  //         // Also Skip if we can use bexti.
  //         Skip |= Subtarget->hasStdExtZbs() && Leading == XLen - 1;
  //         if (OneUseOrZExtW && !Skip) {
  //           SDNode *SLLI = CurDAG->getMachineNode(
  //               RISCV::SLLI, DL, VT, X,
  //               CurDAG->getTargetConstant(Leading - C2, DL, VT));
  //           SDNode *SRLI = CurDAG->getMachineNode(
  //               RISCV::SRLI, DL, VT, SDValue(SLLI, 0),
  //               CurDAG->getTargetConstant(Leading, DL, VT));
  //           ReplaceNode(Node, SRLI);
  //           return;
  //         }
  //       }
  //     }

  //     // Turn (and (shl x, c2), c1) -> (srli (slli c2+c3), c3) if c1 is a mask
  //     // shifted by c2 bits with c3 leading zeros.
  //     if (LeftShift && isShiftedMask_64(C1)) {
  //       unsigned Leading = XLen - (64 - countLeadingZeros(C1));

  //       if (C2 + Leading < XLen &&
  //           C1 == (maskTrailingOnes<uint64_t>(XLen - (C2 + Leading)) << C2)) {
  //         // Use slli.uw when possible.
  //         if ((XLen - (C2 + Leading)) == 32 && Subtarget->hasStdExtZba()) {
  //           SDNode *SLLI_UW =
  //               CurDAG->getMachineNode(RISCV::SLLI_UW, DL, VT, X,
  //                                      CurDAG->getTargetConstant(C2, DL, VT));
  //           ReplaceNode(Node, SLLI_UW);
  //           return;
  //         }

  //         // (srli (slli c2+c3), c3)
  //         if (OneUseOrZExtW && !IsCANDI) {
  //           SDNode *SLLI = CurDAG->getMachineNode(
  //               RISCV::SLLI, DL, VT, X,
  //               CurDAG->getTargetConstant(C2 + Leading, DL, VT));
  //           SDNode *SRLI = CurDAG->getMachineNode(
  //               RISCV::SRLI, DL, VT, SDValue(SLLI, 0),
  //               CurDAG->getTargetConstant(Leading, DL, VT));
  //           ReplaceNode(Node, SRLI);
  //           return;
  //         }
  //       }
  //     }

  //     // Turn (and (shr x, c2), c1) -> (slli (srli x, c2+c3), c3) if c1 is a
  //     // shifted mask with c2 leading zeros and c3 trailing zeros.
  //     if (!LeftShift && isShiftedMask_64(C1)) {
  //       unsigned Leading = XLen - (64 - countLeadingZeros(C1));
  //       unsigned Trailing = countTrailingZeros(C1);
  //       if (Leading == C2 && C2 + Trailing < XLen && OneUseOrZExtW &&
  //           !IsCANDI) {
  //         unsigned SrliOpc = RISCV::SRLI;
  //         // If the input is zexti32 we should use SRLIW.
  //         if (X.getOpcode() == ISD::AND &&
  //             isa<ConstantSDNode>(X.getOperand(1)) &&
  //             X.getConstantOperandVal(1) == UINT64_C(0xFFFFFFFF)) {
  //           SrliOpc = RISCV::SRLIW;
  //           X = X.getOperand(0);
  //         }
  //         SDNode *SRLI = CurDAG->getMachineNode(
  //             SrliOpc, DL, VT, X,
  //             CurDAG->getTargetConstant(C2 + Trailing, DL, VT));
  //         SDNode *SLLI = CurDAG->getMachineNode(
  //             RISCV::SLLI, DL, VT, SDValue(SRLI, 0),
  //             CurDAG->getTargetConstant(Trailing, DL, VT));
  //         ReplaceNode(Node, SLLI);
  //         return;
  //       }
  //       // If the leading zero count is C2+32, we can use SRLIW instead of SRLI.
  //       if (Leading > 32 && (Leading - 32) == C2 && C2 + Trailing < 32 &&
  //           OneUseOrZExtW && !IsCANDI) {
  //         SDNode *SRLIW = CurDAG->getMachineNode(
  //             RISCV::SRLIW, DL, VT, X,
  //             CurDAG->getTargetConstant(C2 + Trailing, DL, VT));
  //         SDNode *SLLI = CurDAG->getMachineNode(
  //             RISCV::SLLI, DL, VT, SDValue(SRLIW, 0),
  //             CurDAG->getTargetConstant(Trailing, DL, VT));
  //         ReplaceNode(Node, SLLI);
  //         return;
  //       }
  //     }

  //     // Turn (and (shl x, c2), c1) -> (slli (srli x, c3-c2), c3) if c1 is a
  //     // shifted mask with no leading zeros and c3 trailing zeros.
  //     if (LeftShift && isShiftedMask_64(C1)) {
  //       unsigned Leading = XLen - (64 - countLeadingZeros(C1));
  //       unsigned Trailing = countTrailingZeros(C1);
  //       if (Leading == 0 && C2 < Trailing && OneUseOrZExtW && !IsCANDI) {
  //         SDNode *SRLI = CurDAG->getMachineNode(
  //             RISCV::SRLI, DL, VT, X,
  //             CurDAG->getTargetConstant(Trailing - C2, DL, VT));
  //         SDNode *SLLI = CurDAG->getMachineNode(
  //             RISCV::SLLI, DL, VT, SDValue(SRLI, 0),
  //             CurDAG->getTargetConstant(Trailing, DL, VT));
  //         ReplaceNode(Node, SLLI);
  //         return;
  //       }
  //       // If we have (32-C2) leading zeros, we can use SRLIW instead of SRLI.
  //       if (C2 < Trailing && Leading + C2 == 32 && OneUseOrZExtW && !IsCANDI) {
  //         SDNode *SRLIW = CurDAG->getMachineNode(
  //             RISCV::SRLIW, DL, VT, X,
  //             CurDAG->getTargetConstant(Trailing - C2, DL, VT));
  //         SDNode *SLLI = CurDAG->getMachineNode(
  //             RISCV::SLLI, DL, VT, SDValue(SRLIW, 0),
  //             CurDAG->getTargetConstant(Trailing, DL, VT));
  //         ReplaceNode(Node, SLLI);
  //         return;
  //       }
  //     }
  //   }

  //   if (tryShrinkShlLogicImm(Node))
  //     return;

  //   break;
  // }
  case ISD::MUL: {
    // Special case for calculating (mul (and X, C2), C1) where the full product
    // fits in XLen bits. We can shift X left by the number of leading zeros in
    // C2 and shift C1 left by XLen-lzcnt(C2). This will ensure the final
    // product has XLen trailing zeros, putting it in the output of MULHU. This
    // can avoid materializing a constant in a register for C2.

    // RHS should be a constant.
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C || !N1C->hasOneUse())
      break;

    // LHS should be an AND with constant.
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::AND || !isa<ConstantSDNode>(N0.getOperand(1)))
      break;

    uint64_t C2 = cast<ConstantSDNode>(N0.getOperand(1))->getZExtValue();

    // Constant should be a mask.
    if (!isMask_64(C2))
      break;

    // If this can be an ANDI, ZEXT.H or ZEXT.W, don't do this if the ANDI/ZEXT
    // has multiple users or the constant is a simm12. This prevents inserting
    // a shift and still have uses of the AND/ZEXT. Shifting a simm12 will
    // likely make it more costly to materialize. Otherwise, using a SLLI
    // might allow it to be compressed.
    bool IsANDIOrZExt =
        isInt<12>(C2) ||
        (C2 == UINT64_C(0xFFFF) && Subtarget->hasStdExtZbb()) ||
        (C2 == UINT64_C(0xFFFFFFFF) && Subtarget->hasStdExtZba());
    if (IsANDIOrZExt && (isInt<12>(N1C->getSExtValue()) || !N0.hasOneUse()))
      break;

    // We need to shift left the AND input and C1 by a total of XLen bits.

    // How far left do we need to shift the AND input?
    unsigned XLen = Subtarget->getXLen();
    unsigned LeadingZeros = XLen - (64 - countLeadingZeros(C2));

    // The constant gets shifted by the remaining amount unless that would
    // shift bits out.
    uint64_t C1 = N1C->getZExtValue();
    unsigned ConstantShift = XLen - LeadingZeros;
    if (ConstantShift > (XLen - (64 - countLeadingZeros(C1))))
      break;

    uint64_t ShiftedC1 = C1 << ConstantShift;
    // If this RV32, we need to sign extend the constant.
    if (XLen == 32)
      ShiftedC1 = SignExtend64<32>(ShiftedC1);

    // Create (mulhu (slli X, lzcnt(C2)), C1 << (XLen - lzcnt(C2))).
    SDNode *Imm = selectImm(CurDAG, DL, VT, ShiftedC1, *Subtarget);
    SDNode *SLLI =
        CurDAG->getMachineNode(RISCV::SLLI, DL, VT, N0.getOperand(0),
                               CurDAG->getTargetConstant(LeadingZeros, DL, VT));
    SDNode *MULHU = CurDAG->getMachineNode(RISCV::MULHU, DL, VT,
                                           SDValue(SLLI, 0), SDValue(Imm, 0));
    ReplaceNode(Node, MULHU);
    return;
  }
// TODO: fix or remove these codes
//   case ISD::INTRINSIC_VOID: {
//     unsigned IntrinsicOpcode = Node->getConstantOperandVal(1);
//     switch (IntrinsicOpcode) {
//       case Intrinsic::riscv_ventus_barrier: {
//         // SDNode *BARRIER = CurDAG->getMachineNode(RISCV::BARRIER, DL, VT, Node->getOperand(1), Node->getOperand(2));
//         // ReplaceNode(Node, BARRIER);
//         // return;
//       }
//       case Intrinsic::riscv_ventus_barrier_with_scope: {

//       }
//       case Intrinsic::riscv_ventus_barriersub: {

//       }
//       case Intrinsic::riscv_ventus_barriersub_with_scope: {

//       }
//     }
//     break;
//   }
  }
  // Select the default instruction.
  SelectCode(Node);
}

bool RISCVDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, unsigned ConstraintID, std::vector<SDValue> &OutOps) {
  switch (ConstraintID) {
  case InlineAsm::Constraint_m:
    // We just support simple memory operands that have a single address
    // operand and need no special handling.
    OutOps.push_back(Op);
    return false;
  case InlineAsm::Constraint_A:
    OutOps.push_back(Op);
    return false;
  default:
    break;
  }

  return true;
}

bool RISCVDAGToDAGISel::SelectAddrFrameIndex(SDValue Addr, SDValue &Base,
                                             SDValue &Offset) {
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), Subtarget->getXLenVT());
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), Subtarget->getXLenVT());
    return true;
  }

  return false;
}

// Select a frame index and an optional immediate offset from an ADD or OR.
bool RISCVDAGToDAGISel::SelectFrameAddrRegImm(SDValue Addr, SDValue &Base,
                                              SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  if (!CurDAG->isBaseWithConstantOffset(Addr))
    return false;

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<12>(CVal)) {
      Base = CurDAG->getTargetFrameIndex(FIN->getIndex(),
                                         Subtarget->getXLenVT());
      Offset = CurDAG->getTargetConstant(CVal, SDLoc(Addr),
                                         Subtarget->getXLenVT());
      return true;
    }
  }

  return false;
}

// Fold constant addresses.
static bool selectConstantAddr(SelectionDAG *CurDAG, const SDLoc &DL,
                               const MVT VT, const RISCVSubtarget *Subtarget,
                               SDValue Addr, SDValue &Base, SDValue &Offset) {
  if (!isa<ConstantSDNode>(Addr))
    return false;

  int64_t CVal = cast<ConstantSDNode>(Addr)->getSExtValue();

  // If the constant is a simm12, we can fold the whole constant and use X0 as
  // the base. If the constant can be materialized with LUI+simm12, use LUI as
  // the base. We can't use generateInstSeq because it favors LUI+ADDIW.
  int64_t Lo12 = SignExtend64<12>(CVal);
  int64_t Hi = (uint64_t)CVal - (uint64_t)Lo12;
  if (!Subtarget->is64Bit() || isInt<32>(Hi)) {
    if (Hi) {
      int64_t Hi20 = (Hi >> 12) & 0xfffff;
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::LUI, DL, VT,
                                 CurDAG->getTargetConstant(Hi20, DL, VT)),
          0);
    } else {
      Base = CurDAG->getRegister(RISCV::X0, VT);
    }
    Offset = CurDAG->getTargetConstant(Lo12, DL, VT);
    return true;
  }

  // Ask how constant materialization would handle this constant.
  RISCVMatInt::InstSeq Seq =
      RISCVMatInt::generateInstSeq(CVal, Subtarget->getFeatureBits());

  // If the last instruction would be an ADDI, we can fold its immediate and
  // emit the rest of the sequence as the base.
  if (Seq.back().Opc != RISCV::ADDI)
    return false;
  Lo12 = Seq.back().Imm;

  // Drop the last instruction.
  Seq.pop_back();
  assert(!Seq.empty() && "Expected more instructions in sequence");

  Base = SDValue(selectImmSeq(CurDAG, DL, VT, Seq), 0);
  Offset = CurDAG->getTargetConstant(Lo12, DL, VT);
  return true;
}

// Is this ADD instruction only used as the base pointer of scalar loads and
// stores?
static bool isWorthFoldingAdd(SDValue Add) {
  for (auto *Use : Add->uses()) {
    if (Use->getOpcode() != ISD::LOAD && Use->getOpcode() != ISD::STORE &&
        Use->getOpcode() != ISD::ATOMIC_LOAD &&
        Use->getOpcode() != ISD::ATOMIC_STORE)
      return false;
    EVT VT = cast<MemSDNode>(Use)->getMemoryVT();
    if (!VT.isScalarInteger() && VT != MVT::f16 && VT != MVT::f32 &&
        VT != MVT::f64)
      return false;
    // Don't allow stores of the value. It must be used as the address.
    if (Use->getOpcode() == ISD::STORE &&
        cast<StoreSDNode>(Use)->getValue() == Add)
      return false;
    if (Use->getOpcode() == ISD::ATOMIC_STORE &&
        cast<AtomicSDNode>(Use)->getVal() == Add)
      return false;
  }

  return true;
}

bool RISCVDAGToDAGISel::SelectAddrRegImm(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (Addr.getOpcode() == RISCVISD::ADD_LO) {
    Base = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<12>(CVal)) {
      Base = Addr.getOperand(0);
      if (Base.getOpcode() == RISCVISD::ADD_LO) {
        SDValue LoOperand = Base.getOperand(1);
        if (auto *GA = dyn_cast<GlobalAddressSDNode>(LoOperand)) {
          // If the Lo in (ADD_LO hi, lo) is a global variable's address
          // (its low part, really), then we can rely on the alignment of that
          // variable to provide a margin of safety before low part can overflow
          // the 12 bits of the load/store offset. Check if CVal falls within
          // that margin; if so (low part + CVal) can't overflow.
          const DataLayout &DL = CurDAG->getDataLayout();
          Align Alignment = commonAlignment(
              GA->getGlobal()->getPointerAlignment(DL), GA->getOffset());
          if (CVal == 0 || Alignment > CVal) {
            int64_t CombinedOffset = CVal + GA->getOffset();
            Base = Base.getOperand(0);
            Offset = CurDAG->getTargetGlobalAddress(
                GA->getGlobal(), SDLoc(LoOperand), LoOperand.getValueType(),
                CombinedOffset, GA->getTargetFlags());
            return true;
          }
        }
      }

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
      Offset = CurDAG->getTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  // Handle ADD with large immediates.
  if (Addr.getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr.getOperand(1))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    assert(!isInt<12>(CVal) && "simm12 not already handled?");

    // Handle immediates in the range [-4096,-2049] or [2048, 4094]. We can use
    // an ADDI for part of the offset and fold the rest into the load/store.
    // This mirrors the AddiPair PatFrag in RISCVInstrInfo.td.
    if (isInt<12>(CVal / 2) && isInt<12>(CVal - CVal / 2)) {
      int64_t Adj = CVal < 0 ? -2048 : 2047;
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADDI, DL, VT, Addr.getOperand(0),
                                 CurDAG->getTargetConstant(Adj, DL, VT)),
          0);
      Offset = CurDAG->getTargetConstant(CVal - Adj, DL, VT);
      return true;
    }

    // For larger immediates, we might be able to save one instruction from
    // constant materialization by folding the Lo12 bits of the immediate into
    // the address. We should only do this if the ADD is only used by loads and
    // stores that can fold the lo12 bits. Otherwise, the ADD will get iseled
    // separately with the full materialized immediate creating extra
    // instructions.
    if (isWorthFoldingAdd(Addr) &&
        selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr.getOperand(1), Base,
                           Offset)) {
      // Insert an ADD instruction with the materialized Hi52 bits.
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADD, DL, VT, Addr.getOperand(0), Base),
          0);
      return true;
    }
  }

  if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr, Base, Offset))
    return true;

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, VT);

  return true;
}

bool RISCVDAGToDAGISel::SelectPriAddrRegImm(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (Addr.getOpcode() == RISCVISD::ADD_LO) {
    Base = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<11>(CVal)) {
      Base = Addr.getOperand(0);
      if (Base.getOpcode() == RISCVISD::ADD_LO) {
        SDValue LoOperand = Base.getOperand(1);
        if (auto *GA = dyn_cast<GlobalAddressSDNode>(LoOperand)) {
          // If the Lo in (ADD_LO hi, lo) is a global variable's address
          // (its low part, really), then we can rely on the alignment of that
          // variable to provide a margin of safety before low part can overflow
          // the 12 bits of the load/store offset. Check if CVal falls within
          // that margin; if so (low part + CVal) can't overflow.
          const DataLayout &DL = CurDAG->getDataLayout();
          Align Alignment = commonAlignment(
              GA->getGlobal()->getPointerAlignment(DL), GA->getOffset());
          if (CVal == 0 || Alignment > CVal) {
            int64_t CombinedOffset = CVal + GA->getOffset();
            Base = Base.getOperand(0);
            Offset = CurDAG->getTargetGlobalAddress(
                GA->getGlobal(), SDLoc(LoOperand), LoOperand.getValueType(),
                CombinedOffset, GA->getTargetFlags());
            return true;
          }
        }
      }

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
      Offset = CurDAG->getTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  // Handle ADD with large immediates.
  if (Addr.getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr.getOperand(1))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    assert(!isInt<11>(CVal) && "simm11 not already handled?");

    // Handle immediates in the range [-4096,-2049] or [2048, 4094]. We can use
    // an ADDI for part of the offset and fold the rest into the load/store.
    // This mirrors the AddiPair PatFrag in RISCVInstrInfo.td.
    if (isInt<11>(CVal / 2) && isInt<11>(CVal - CVal / 2)) {
      int64_t Adj = CVal < 0 ? -2048 : 2047;
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADDI, DL, VT, Addr.getOperand(0),
                                 CurDAG->getTargetConstant(Adj, DL, VT)),
          0);
      Offset = CurDAG->getTargetConstant(CVal - Adj, DL, VT);
      return true;
    }

    // For larger immediates, we might be able to save one instruction from
    // constant materialization by folding the Lo12 bits of the immediate into
    // the address. We should only do this if the ADD is only used by loads and
    // stores that can fold the lo12 bits. Otherwise, the ADD will get iseled
    // separately with the full materialized immediate creating extra
    // instructions.
    if (isWorthFoldingAdd(Addr) &&
        selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr.getOperand(1), Base,
                           Offset)) {
      // Insert an ADD instruction with the materialized Hi52 bits.
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADD, DL, VT, Addr.getOperand(0), Base),
          0);
      return true;
    }
  }

  if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr, Base, Offset))
    return true;

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, VT);

  return true;
}

// FIXME: This is almost identical to SelectAddrRegImm now, but it will be
// modified to support more vALU addressing patterns.
bool RISCVDAGToDAGISel::SelectAddrRegReg(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (Addr.getOpcode() == RISCVISD::ADD_LO) {
    Base = Addr.getOperand(0);
    assert(0 && "TODO");
    // Offset = Addr.getOperand(1);
    return true;
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<12>(CVal)) {
      Base = Addr.getOperand(0);
      if (Base.getOpcode() == RISCVISD::ADD_LO) {
        SDValue LoOperand = Base.getOperand(1);
        if (auto *GA = dyn_cast<GlobalAddressSDNode>(LoOperand)) {
          // If the Lo in (ADD_LO hi, lo) is a global variable's address
          // (its low part, really), then we can rely on the alignment of that
          // variable to provide a margin of safety before low part can overflow
          // the 12 bits of the load/store offset. Check if CVal falls within
          // that margin; if so (low part + CVal) can't overflow.
          const DataLayout &DL = CurDAG->getDataLayout();
          Align Alignment = commonAlignment(
              GA->getGlobal()->getPointerAlignment(DL), GA->getOffset());
          if (CVal == 0 || Alignment > CVal) {
            assert(0 && "TODO");
            /*
            int64_t CombinedOffset = CVal + GA->getOffset();
            Base = Base.getOperand(0);
            Offset = CurDAG->getTargetGlobalAddress(
                GA->getGlobal(), SDLoc(LoOperand), LoOperand.getValueType(),
                CombinedOffset, GA->getTargetFlags());
            */
            return true;
          }
        }
      }

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
    //   assert(0 && "TODO");
      Offset = CurDAG->getTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  // Handle ADD with large immediates.
  if (Addr.getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr.getOperand(1))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    assert(!isInt<12>(CVal) && "simm12 not already handled?");

    // Handle immediates in the range [-4096,-2049] or [2048, 4094]. We can use
    // an ADDI for part of the offset and fold the rest into the load/store.
    // This mirrors the AddiPair PatFrag in RISCVInstrInfo.td.
    if (isInt<12>(CVal / 2) && isInt<12>(CVal - CVal / 2)) {
      int64_t Adj = CVal < 0 ? -2048 : 2047;
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADDI, DL, VT, Addr.getOperand(0),
                                 CurDAG->getTargetConstant(Adj, DL, VT)),
          0);
      assert(0 && "TODO");
      // Offset = CurDAG->getTargetConstant(CVal - Adj, DL, VT);
      return true;
    }

    // For larger immediates, we might be able to save one instruction from
    // constant materialization by folding the Lo12 bits of the immediate into
    // the address. We should only do this if the ADD is only used by loads and
    // stores that can fold the lo12 bits. Otherwise, the ADD will get iseled
    // separately with the full materialized immediate creating extra
    // instructions.
    if (isWorthFoldingAdd(Addr) &&
        selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr.getOperand(1), Base,
                           Offset)) {
      // Insert an ADD instruction with the materialized Hi52 bits.
      Base = SDValue(
          CurDAG->getMachineNode(RISCV::ADD, DL, VT, Addr.getOperand(0), Base),
          0);
      return true;
    }
  }

  if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr, Base, Offset))
    return true;

  Base = Addr;
  Offset = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, RISCV::X0,
                                  Subtarget->getXLenVT());

  return true;
}


bool RISCVDAGToDAGISel::selectShiftMask(SDValue N, unsigned ShiftWidth,
                                        SDValue &ShAmt) {
  ShAmt = N;

  // Shift instructions on RISCV only read the lower 5 or 6 bits of the shift
  // amount. If there is an AND on the shift amount, we can bypass it if it
  // doesn't affect any of those bits.
  if (ShAmt.getOpcode() == ISD::AND && isa<ConstantSDNode>(ShAmt.getOperand(1))) {
    const APInt &AndMask = ShAmt.getConstantOperandAPInt(1);

    // Since the max shift amount is a power of 2 we can subtract 1 to make a
    // mask that covers the bits needed to represent all shift amounts.
    assert(isPowerOf2_32(ShiftWidth) && "Unexpected max shift amount!");
    APInt ShMask(AndMask.getBitWidth(), ShiftWidth - 1);

    if (ShMask.isSubsetOf(AndMask)) {
      ShAmt = ShAmt.getOperand(0);
    } else {
      // SimplifyDemandedBits may have optimized the mask so try restoring any
      // bits that are known zero.
      KnownBits Known = CurDAG->computeKnownBits(ShAmt.getOperand(0));
      if (!ShMask.isSubsetOf(AndMask | Known.Zero))
        return true;
      ShAmt = ShAmt.getOperand(0);
    }
  }

  if (ShAmt.getOpcode() == ISD::SUB &&
      isa<ConstantSDNode>(ShAmt.getOperand(0))) {
    uint64_t Imm = ShAmt.getConstantOperandVal(0);
    // If we are shifting by N-X where N == 0 mod Size, then just shift by -X to
    // generate a NEG instead of a SUB of a constant.
    if (Imm != 0 && Imm % ShiftWidth == 0) {
      SDLoc DL(ShAmt);
      EVT VT = ShAmt.getValueType();
      SDValue Zero = CurDAG->getRegister(RISCV::X0, VT);
      unsigned NegOpc = VT == MVT::i64 ? RISCV::SUBW : RISCV::SUB;
      MachineSDNode *Neg = CurDAG->getMachineNode(NegOpc, DL, VT, Zero,
                                                  ShAmt.getOperand(1));
      ShAmt = SDValue(Neg, 0);
      return true;
    }
  }

  return true;
}

bool RISCVDAGToDAGISel::selectSExti32(SDValue N, SDValue &Val) {
  if (N.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      cast<VTSDNode>(N.getOperand(1))->getVT() == MVT::i32) {
    Val = N.getOperand(0);
    return true;
  }
  MVT VT = N.getSimpleValueType();
  if (CurDAG->ComputeNumSignBits(N) > (VT.getSizeInBits() - 32)) {
    Val = N;
    return true;
  }

  return false;
}

bool RISCVDAGToDAGISel::selectZExtBits(SDValue N, unsigned Bits, SDValue &Val) {
  if (N.getOpcode() == ISD::AND) {
    auto *C = dyn_cast<ConstantSDNode>(N.getOperand(1));
    if (C && C->getZExtValue() == maskTrailingOnes<uint64_t>(Bits)) {
      Val = N.getOperand(0);
      return true;
    }
  }
  MVT VT = N.getSimpleValueType();
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), Bits);
  if (CurDAG->MaskedValueIsZero(N, Mask)) {
    Val = N;
    return true;
  }

  return false;
}

/// Look for various patterns that can be done with a SHL that can be folded
/// into a SHXADD. \p ShAmt contains 1, 2, or 3 and is set based on which
/// SHXADD we are trying to match.
bool RISCVDAGToDAGISel::selectSHXADDOp(SDValue N, unsigned ShAmt,
                                       SDValue &Val) {
  if (N.getOpcode() == ISD::AND && isa<ConstantSDNode>(N.getOperand(1))) {
    SDValue N0 = N.getOperand(0);

    bool LeftShift = N0.getOpcode() == ISD::SHL;
    if ((LeftShift || N0.getOpcode() == ISD::SRL) &&
        isa<ConstantSDNode>(N0.getOperand(1))) {
      uint64_t Mask = N.getConstantOperandVal(1);
      unsigned C2 = N0.getConstantOperandVal(1);

      unsigned XLen = Subtarget->getXLen();
      if (LeftShift)
        Mask &= maskTrailingZeros<uint64_t>(C2);
      else
        Mask &= maskTrailingOnes<uint64_t>(XLen - C2);

      // Look for (and (shl y, c2), c1) where c1 is a shifted mask with no
      // leading zeros and c3 trailing zeros. We can use an SRLI by c2+c3
      // followed by a SHXADD with c3 for the X amount.
      if (isShiftedMask_64(Mask)) {
        unsigned Leading = XLen - (64 - countLeadingZeros(Mask));
        unsigned Trailing = countTrailingZeros(Mask);
        if (LeftShift && Leading == 0 && C2 < Trailing && Trailing == ShAmt) {
          SDLoc DL(N);
          EVT VT = N.getValueType();
          Val = SDValue(CurDAG->getMachineNode(
                            RISCV::SRLI, DL, VT, N0.getOperand(0),
                            CurDAG->getTargetConstant(Trailing - C2, DL, VT)),
                        0);
          return true;
        }
        // Look for (and (shr y, c2), c1) where c1 is a shifted mask with c2
        // leading zeros and c3 trailing zeros. We can use an SRLI by C3
        // followed by a SHXADD using c3 for the X amount.
        if (!LeftShift && Leading == C2 && Trailing == ShAmt) {
          SDLoc DL(N);
          EVT VT = N.getValueType();
          Val = SDValue(
              CurDAG->getMachineNode(
                  RISCV::SRLI, DL, VT, N0.getOperand(0),
                  CurDAG->getTargetConstant(Leading + Trailing, DL, VT)),
              0);
          return true;
        }
      }
    }
  }

  bool LeftShift = N.getOpcode() == ISD::SHL;
  if ((LeftShift || N.getOpcode() == ISD::SRL) &&
      isa<ConstantSDNode>(N.getOperand(1))) {
    SDValue N0 = N.getOperand(0);
    if (N0.getOpcode() == ISD::AND && N0.hasOneUse() &&
        isa<ConstantSDNode>(N0.getOperand(1))) {
      uint64_t Mask = N0.getConstantOperandVal(1);
      if (isShiftedMask_64(Mask)) {
        unsigned C1 = N.getConstantOperandVal(1);
        unsigned XLen = Subtarget->getXLen();
        unsigned Leading = XLen - (64 - countLeadingZeros(Mask));
        unsigned Trailing = countTrailingZeros(Mask);
        // Look for (shl (and X, Mask), C1) where Mask has 32 leading zeros and
        // C3 trailing zeros. If C1+C3==ShAmt we can use SRLIW+SHXADD.
        if (LeftShift && Leading == 32 && Trailing > 0 &&
            (Trailing + C1) == ShAmt) {
          SDLoc DL(N);
          EVT VT = N.getValueType();
          Val = SDValue(CurDAG->getMachineNode(
                            RISCV::SRLIW, DL, VT, N0.getOperand(0),
                            CurDAG->getTargetConstant(Trailing, DL, VT)),
                        0);
          return true;
        }
        // Look for (srl (and X, Mask), C1) where Mask has 32 leading zeros and
        // C3 trailing zeros. If C3-C1==ShAmt we can use SRLIW+SHXADD.
        if (!LeftShift && Leading == 32 && Trailing > C1 &&
            (Trailing - C1) == ShAmt) {
          SDLoc DL(N);
          EVT VT = N.getValueType();
          Val = SDValue(CurDAG->getMachineNode(
                            RISCV::SRLIW, DL, VT, N0.getOperand(0),
                            CurDAG->getTargetConstant(Trailing, DL, VT)),
                        0);
          return true;
        }
      }
    }
  }

  return false;
}

/// Look for various patterns that can be done with a SHL that can be folded
/// into a SHXADD_UW. \p ShAmt contains 1, 2, or 3 and is set based on which
/// SHXADD_UW we are trying to match.
bool RISCVDAGToDAGISel::selectSHXADD_UWOp(SDValue N, unsigned ShAmt,
                                          SDValue &Val) {
  if (N.getOpcode() == ISD::AND && isa<ConstantSDNode>(N.getOperand(1)) &&
      N.hasOneUse()) {
    SDValue N0 = N.getOperand(0);
    if (N0.getOpcode() == ISD::SHL && isa<ConstantSDNode>(N0.getOperand(1)) &&
        N0.hasOneUse()) {
      uint64_t Mask = N.getConstantOperandVal(1);
      unsigned C2 = N0.getConstantOperandVal(1);

      Mask &= maskTrailingZeros<uint64_t>(C2);

      // Look for (and (shl y, c2), c1) where c1 is a shifted mask with
      // 32-ShAmt leading zeros and c2 trailing zeros. We can use SLLI by
      // c2-ShAmt followed by SHXADD_UW with ShAmt for the X amount.
      if (isShiftedMask_64(Mask)) {
        unsigned Leading = countLeadingZeros(Mask);
        unsigned Trailing = countTrailingZeros(Mask);
        if (Leading == 32 - ShAmt && Trailing == C2 && Trailing > ShAmt) {
          SDLoc DL(N);
          EVT VT = N.getValueType();
          Val = SDValue(CurDAG->getMachineNode(
                            RISCV::SLLI, DL, VT, N0.getOperand(0),
                            CurDAG->getTargetConstant(C2 - ShAmt, DL, VT)),
                        0);
          return true;
        }
      }
    }
  }

  return false;
}

// Return true if all users of this SDNode* only consume the lower \p Bits.
// This can be used to form W instructions for add/sub/mul/shl even when the
// root isn't a sext_inreg. This can allow the ADDW/SUBW/MULW/SLLIW to CSE if
// SimplifyDemandedBits has made it so some users see a sext_inreg and some
// don't. The sext_inreg+add/sub/mul/shl will get selected, but still leave
// the add/sub/mul/shl to become non-W instructions. By checking the users we
// may be able to use a W instruction and CSE with the other instruction if
// this has happened. We could try to detect that the CSE opportunity exists
// before doing this, but that would be more complicated.
// TODO: Does this need to look through AND/OR/XOR to their users to find more
// opportunities.
bool RISCVDAGToDAGISel::hasAllNBitUsers(SDNode *Node, unsigned Bits) const {
  assert((Node->getOpcode() == ISD::ADD || Node->getOpcode() == ISD::SUB ||
          Node->getOpcode() == ISD::MUL || Node->getOpcode() == ISD::SHL ||
          Node->getOpcode() == ISD::SRL || Node->getOpcode() == ISD::AND ||
          Node->getOpcode() == ISD::OR || Node->getOpcode() == ISD::XOR ||
          Node->getOpcode() == ISD::SIGN_EXTEND_INREG ||
          isa<ConstantSDNode>(Node)) &&
         "Unexpected opcode");

  for (auto UI = Node->use_begin(), UE = Node->use_end(); UI != UE; ++UI) {
    SDNode *User = *UI;
    // Users of this node should have already been instruction selected
    if (!User->isMachineOpcode())
      return false;

    // TODO: Add more opcodes?
    switch (User->getMachineOpcode()) {
    default:
      return false;
    case RISCV::ADDW:
    case RISCV::ADDIW:
    case RISCV::SUBW:
    case RISCV::MULW:
    case RISCV::SLLW:
    case RISCV::SLLIW:
    case RISCV::SRAW:
    case RISCV::SRAIW:
    case RISCV::SRLW:
    case RISCV::SRLIW:
    case RISCV::DIVW:
    case RISCV::DIVUW:
    case RISCV::REMW:
    case RISCV::REMUW:
    case RISCV::ROLW:
    case RISCV::RORW:
    case RISCV::RORIW:
    case RISCV::CLZW:
    case RISCV::CTZW:
    case RISCV::CPOPW:
    case RISCV::SLLI_UW:
    // case RISCV::FMV_W_X:
    case RISCV::FCVT_H_W:
    case RISCV::FCVT_H_WU:
    case RISCV::FCVT_S_W:
    case RISCV::FCVT_S_WU:
    case RISCV::FCVT_D_W:
    case RISCV::FCVT_D_WU:
      if (Bits < 32)
        return false;
      break;
    case RISCV::SLL:
    case RISCV::SRA:
    case RISCV::SRL:
    case RISCV::ROL:
    case RISCV::ROR:
    case RISCV::BSET:
    case RISCV::BCLR:
    case RISCV::BINV:
      // Shift amount operands only use log2(Xlen) bits.
      if (UI.getOperandNo() != 1 || Bits < Log2_32(Subtarget->getXLen()))
        return false;
      break;
    case RISCV::SLLI:
      // SLLI only uses the lower (XLen - ShAmt) bits.
      if (Bits < Subtarget->getXLen() - User->getConstantOperandVal(1))
        return false;
      break;
    case RISCV::ANDI:
      if (Bits < (64 - countLeadingZeros(User->getConstantOperandVal(1))))
        return false;
      break;
    case RISCV::ORI: {
      uint64_t Imm = cast<ConstantSDNode>(User->getOperand(1))->getSExtValue();
      if (Bits < (64 - countLeadingOnes(Imm)))
        return false;
      break;
    }
    case RISCV::SEXT_B:
    case RISCV::PACKH:
      if (Bits < 8)
        return false;
      break;
    case RISCV::SEXT_H:
    case RISCV::FMV_H_X:
    case RISCV::ZEXT_H_RV32:
    case RISCV::ZEXT_H_RV64:
    case RISCV::PACKW:
      if (Bits < 16)
        return false;
      break;
    case RISCV::PACK:
      if (Bits < (Subtarget->getXLen() / 2))
        return false;
      break;
    case RISCV::ADD_UW:
    case RISCV::SH1ADD_UW:
    case RISCV::SH2ADD_UW:
    case RISCV::SH3ADD_UW:
      // The first operand to add.uw/shXadd.uw is implicitly zero extended from
      // 32 bits.
      if (UI.getOperandNo() != 0 || Bits < 32)
        return false;
      break;
    case RISCV::SB:
      if (UI.getOperandNo() != 0 || Bits < 8)
        return false;
      break;
    case RISCV::SH:
      if (UI.getOperandNo() != 0 || Bits < 16)
        return false;
      break;
    case RISCV::SW:
      if (UI.getOperandNo() != 0 || Bits < 32)
        return false;
      break;
    }
  }

  return true;
}

// Try to remove sext.w if the input is a W instruction or can be made into
// a W instruction cheaply.
bool RISCVDAGToDAGISel::doPeepholeSExtW(SDNode *N) {
  // Look for the sext.w pattern, addiw rd, rs1, 0.
  if (N->getMachineOpcode() != RISCV::ADDIW ||
      !isNullConstant(N->getOperand(1)))
    return false;

  SDValue N0 = N->getOperand(0);
  if (!N0.isMachineOpcode())
    return false;

  switch (N0.getMachineOpcode()) {
  default:
    break;
  case RISCV::ADD:
  case RISCV::ADDI:
  case RISCV::SUB:
  case RISCV::MUL:
  case RISCV::SLLI: {
    // Convert sext.w+add/sub/mul to their W instructions. This will create
    // a new independent instruction. This improves latency.
    unsigned Opc;
    switch (N0.getMachineOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode!");
    case RISCV::ADD:  Opc = RISCV::ADDW;  break;
    case RISCV::ADDI: Opc = RISCV::ADDIW; break;
    case RISCV::SUB:  Opc = RISCV::SUBW;  break;
    case RISCV::MUL:  Opc = RISCV::MULW;  break;
    case RISCV::SLLI: Opc = RISCV::SLLIW; break;
    }

    SDValue N00 = N0.getOperand(0);
    SDValue N01 = N0.getOperand(1);

    // Shift amount needs to be uimm5.
    if (N0.getMachineOpcode() == RISCV::SLLI &&
        !isUInt<5>(cast<ConstantSDNode>(N01)->getSExtValue()))
      break;

    SDNode *Result =
        CurDAG->getMachineNode(Opc, SDLoc(N), N->getValueType(0),
                               N00, N01);
    ReplaceUses(N, Result);
    return true;
  }
  case RISCV::ADDW:
  case RISCV::ADDIW:
  case RISCV::SUBW:
  case RISCV::MULW:
  case RISCV::SLLIW:
  case RISCV::PACKW:
    // Result is already sign extended just remove the sext.w.
    // NOTE: We only handle the nodes that are selected with hasAllWUsers.
    ReplaceUses(N, N0.getNode());
    return true;
  }

  return false;
}

// This pass converts a legalized DAG into a RISCV-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createRISCVISelDag(RISCVTargetMachine &TM,
                                       CodeGenOpt::Level OptLevel) {
  return new RISCVDAGToDAGISel(TM, OptLevel);
}
