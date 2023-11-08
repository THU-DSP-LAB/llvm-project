//===-- RISCVExpandPseudoInsts.cpp - Expand pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define RISCV_EXPAND_PSEUDO_NAME "RISCV pseudo instruction expansion pass"
#define RISCV_PRERA_EXPAND_PSEUDO_NAME                                         \
  "RISCV Pre-RA pseudo instruction expansion pass"

namespace {

class RISCVExpandPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVExpandPseudo() : MachineFunctionPass(ID) {
    initializeRISCVExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandCCOp(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                  MachineBasicBlock::iterator &NextMBBI);
  bool expandBarrier(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     MachineBasicBlock::iterator &NextMBBI);
  bool expandCompareSelect(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI);
  bool expandVIIMM11(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator MBBI);
};

char RISCVExpandPseudo::ID = 0;

bool RISCVExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool RISCVExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

// TODO: Expand macro instruction with more than 32 registers here?
bool RISCVExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  // RISCVInstrInfo::getInstSizeInBytes expects that the total size of the
  // expanded instructions for each pseudo is correct in the Size field of the
  // tablegen definition for the pseudo.
  if (RISCVII::isVOPIMM11(MBBI->getDesc().TSFlags))
    return expandVIIMM11(MBB, MBBI);

  switch (MBBI->getOpcode()) {
  case RISCV::PseudoCCMOVGPR:
    return expandCCOp(MBB, MBBI, NextMBBI);
  case RISCV::PseudoBarrier:
  case RISCV::PseudoSubGroupBarrier:
    return expandBarrier(MBB, MBBI, NextMBBI);
  case RISCV::PseudoVMSLTU_VI:
  case RISCV::PseudoVMSLT_VI:
  case RISCV::PseudoVMSGE_VI:
  case RISCV::PseudoVMSGEU_VI:
    return expandCompareSelect(MBB, MBBI, NextMBBI);
  }
  return false;
}

bool RISCVExpandPseudo::expandVIIMM11(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  const TargetRegisterInfo *TRI = MBB.getParent()->getSubtarget().getRegisterInfo();
  const MCInstrDesc *MCID = nullptr;

  switch (MBBI->getOpcode()) {
    default:
      llvm_unreachable("Please add IMM11 Pseudo case here!");
    case RISCV::PseudoVOR_VI_IMM11:
      MCID = &TII->get(RISCV::VOR_VI);
      break;
    case RISCV::PseudoVXOR_VI_IMM11:
      MCID = &TII->get(RISCV::VXOR_VI);
      break;
    case RISCV::PseudoVRSUB_VI_IMM11:
      MCID = &TII->get(RISCV::VRSUB_VI);
      break;
    case RISCV::PseudoVAND_VI_IMM11:
      MCID = &TII->get(RISCV::VAND_VI);
      break;
    case RISCV::PseudoVMSNE_VI_IMM11:
      MCID = &TII->get(RISCV::VMSNE_VI);
      break;
    case RISCV::PseudoVMSEQ_VI_IMM11:
      MCID = &TII->get(RISCV::VMSEQ_VI);
      break;
  }

  assert(MCID && "Unexpected opcode");
  MBBI->setDesc(*MCID);

  int64_t Imm = 0;
  signed LowImm = 0;
  signed HighImm = 0;
  signed Offsets = 0;
  signed TmpImm = 0;

  for (unsigned i = 0; i < MBBI->getNumOperands(); ++i) {
    MachineOperand &Op = MBBI->getOperand(i);

    if (Op.isImm()) {
      Imm = Op.getImm();

      if (Imm >= 0) {
        Imm &= 0b01111111111;
      }
      else {
        Imm  = -Imm;
        Imm  = ~Imm + 1;
        Imm &= 0b01111111111;
        Imm |= 0b10000000000;
      }

      LowImm  = Imm & 0b00000011111;
      TmpImm  = ~LowImm + 1;
      TmpImm &= 0b01111;
      LowImm  = (LowImm & 0b10000) ? -TmpImm : LowImm;

      HighImm = (Imm & 0b11111100000) >> 5;
      TmpImm  = ~HighImm + 1;
      TmpImm  &= 0b011111;
      HighImm = (HighImm & 0b100000) ? -TmpImm : HighImm;

      Op.ChangeToImmediate(LowImm);

      continue;
    }

    if (!Op.isReg() || MBBI->getDesc().getOperandConstraint(i, MCOI::TIED_TO) != -1)
      continue;

    // deal with register numbers larger than 32.
    if (Op.isReg() && 
        MBBI->getDesc().getOperandConstraint(i, MCOI::TIED_TO) == -1) {
      uint16_t RegEncodingValue = TRI->getEncodingValue(Op.getReg());
      if (RegEncodingValue > 31) {
        int Pos = MBBI->getDesc().getOperandConstraint(i, MCOI::CUSTOM);
        assert(Pos != -1 && "Out of range[0, 31] register operand custom "
                            "constraint that must be present.");
        Offsets |= (RegEncodingValue >> 5 & 0x7) << (3 * Pos);
      }
    }
  }

  DebugLoc DL = MBBI->getDebugLoc();
  // Create instruction to expand imm5 or register basic offset as imm * 32. 
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::REGEXTI), RISCV::X0)
      .addReg(RISCV::X0)
      .addImm((HighImm << 6) | Offsets);

  return true;
}

bool RISCVExpandPseudo::expandBarrier(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineBasicBlock::iterator &NextMBBI) {
  assert((MBBI->getOpcode() == RISCV::PseudoBarrier ||
          MBBI->getOpcode() == RISCV::PseudoSubGroupBarrier) &&
         "Unexpected opcode");
  bool isBarrier = MBBI->getOpcode() == RISCV::PseudoBarrier;
  unsigned BarrierOpcode = isBarrier ? RISCV::BARRIER : RISCV::SUBGROUP_BARRIER;
  uint32_t MemFlag = MBBI->getOperand(0).getImm();
  // when use barriersub, MemScope is default to be 0
  uint32_t MemScope = isBarrier ? MBBI->getOperand(1).getImm() : 0;
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(BarrierOpcode))
      .addImm((MemScope << 3) + MemFlag);
  MBBI->eraseFromParent();
  return true;
}

bool RISCVExpandPseudo::expandCompareSelect(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  unsigned Opcode = MBBI->getOpcode();
  if (Opcode == RISCV::PseudoVMSLT_VI) {
    Opcode = RISCV::VMSLE_VI;
  } else if (Opcode == RISCV::PseudoVMSLTU_VI) {
    Opcode = RISCV::VMSLEU_VI;
  } else if (Opcode == RISCV::PseudoVMSGE_VI) {
    Opcode = RISCV::VMSGT_VI;
  } else if (Opcode == RISCV::PseudoVMSGEU_VI) {
    Opcode = RISCV::VMSGTU_VI;
  } else {
    llvm_unreachable("Unexpected Opcode!");
  }
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opcode),
          MBBI->getOperand(0).getReg())
      .addReg(MBBI->getOperand(1).getReg())
      .addImm(MBBI->getOperand(2).getImm() - 1);
  MBBI->eraseFromParent();
  return true;
}

bool RISCVExpandPseudo::expandCCOp(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MachineBasicBlock::iterator &NextMBBI) {
  assert(MBBI->getOpcode() == RISCV::PseudoCCMOVGPR && "Unexpected opcode");

  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *TrueBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  MachineBasicBlock *MergeBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  MF->insert(++MBB.getIterator(), TrueBB);
  MF->insert(++TrueBB->getIterator(), MergeBB);

  // We want to copy the "true" value when the condition is true which means
  // we need to invert the branch condition to jump over TrueBB when the
  // condition is false.
  auto CC = static_cast<RISCVCC::CondCode>(MI.getOperand(3).getImm());
  CC = RISCVCC::getOppositeBranchCondition(CC);

  // Insert branch instruction.
  BuildMI(MBB, MBBI, DL, TII->getBrCond(CC))
      .addReg(MI.getOperand(1).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addMBB(MergeBB);

  Register DestReg = MI.getOperand(0).getReg();
  assert(MI.getOperand(4).getReg() == DestReg);

  // Add MV.
  BuildMI(TrueBB, DL, TII->get(RISCV::ADDI), DestReg)
      .add(MI.getOperand(5))
      .addImm(0);

  TrueBB->addSuccessor(MergeBB);

  MergeBB->splice(MergeBB->end(), &MBB, MI, MBB.end());
  MergeBB->transferSuccessors(&MBB);

  MBB.addSuccessor(TrueBB);
  MBB.addSuccessor(MergeBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  // Make sure live-ins are correctly attached to this new basic block.
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *TrueBB);
  computeAndAddLiveIns(LiveRegs, *MergeBB);

  return true;
}

class RISCVPreRAExpandPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVPreRAExpandPseudo() : MachineFunctionPass(ID) {
    initializeRISCVPreRAExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return RISCV_PRERA_EXPAND_PSEUDO_NAME;
  }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandAuipcInstPair(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI,
                           unsigned FlagsHi, unsigned SecondOpcode);
  bool expandLoadLocalAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadAddress(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI,
                         MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSIEAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSGDAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
};

char RISCVPreRAExpandPseudo::ID = 0;

bool RISCVPreRAExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool RISCVPreRAExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVPreRAExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineBasicBlock::iterator &NextMBBI) {

  switch (MBBI->getOpcode()) {
  case RISCV::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA:
    return expandLoadAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA_TLS_IE:
    return expandLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA_TLS_GD:
    return expandLoadTLSGDAddress(MBB, MBBI, NextMBBI);
  }
  return false;
}

bool RISCVPreRAExpandPseudo::expandAuipcInstPair(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI, unsigned FlagsHi,
    unsigned SecondOpcode) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg =
      MF->getRegInfo().createVirtualRegister(&RISCV::GPRRegClass);

  MachineOperand &Symbol = MI.getOperand(1);
  Symbol.setTargetFlags(FlagsHi);
  MCSymbol *AUIPCSymbol = MF->getContext().createNamedTempSymbol("pcrel_hi");

  MachineInstr *MIAUIPC =
      BuildMI(MBB, MBBI, DL, TII->get(RISCV::AUIPC), ScratchReg).add(Symbol);
  MIAUIPC->setPreInstrSymbol(*MF, AUIPCSymbol);

  MachineInstr *SecondMI =
      BuildMI(MBB, MBBI, DL, TII->get(SecondOpcode), DestReg)
          .addReg(ScratchReg)
          .addSym(AUIPCSymbol, RISCVII::MO_PCREL_LO);

  if (MI.hasOneMemOperand())
    SecondMI->addMemOperand(*MF, *MI.memoperands_begin());

  MI.eraseFromParent();
  return true;
}

bool RISCVPreRAExpandPseudo::expandLoadLocalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_PCREL_HI,
                             RISCV::ADDI);
}

bool RISCVPreRAExpandPseudo::expandLoadAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  assert(MF->getTarget().isPositionIndependent());
  const auto &STI = MF->getSubtarget<RISCVSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? RISCV::LD : RISCV::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_GOT_HI,
                             SecondOpcode);
}

bool RISCVPreRAExpandPseudo::expandLoadTLSIEAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  const auto &STI = MF->getSubtarget<RISCVSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? RISCV::LD : RISCV::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_TLS_GOT_HI,
                             SecondOpcode);
}

bool RISCVPreRAExpandPseudo::expandLoadTLSGDAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_TLS_GD_HI,
                             RISCV::ADDI);
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandPseudo, "riscv-expand-pseudo",
                RISCV_EXPAND_PSEUDO_NAME, false, false)

INITIALIZE_PASS(RISCVPreRAExpandPseudo, "riscv-prera-expand-pseudo",
                RISCV_PRERA_EXPAND_PSEUDO_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVExpandPseudoPass() { return new RISCVExpandPseudo(); }
FunctionPass *createRISCVPreRAExpandPseudoPass() {
  return new RISCVPreRAExpandPseudo();
}

} // end of namespace llvm
