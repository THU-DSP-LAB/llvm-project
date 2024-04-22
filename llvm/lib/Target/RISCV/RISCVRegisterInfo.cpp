//===-- RISCVRegisterInfo.cpp - RISCV Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISCV implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "RISCVRegisterInfo.h"
#include "RISCV.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "RISCVGenRegisterInfo.inc"

using namespace llvm;

static cl::opt<bool>
    DisableRegAllocHints("riscv-disable-regalloc-hints", cl::Hidden,
                         cl::init(false),
                         cl::desc("Disable two address hints for register "
                                  "allocation"));

static_assert(RISCV::X1 == RISCV::X0 + 1, "Register list not consecutive");
static_assert(RISCV::X31 == RISCV::X0 + 31, "Register list not consecutive");
static_assert(RISCV::F1_H == RISCV::F0_H + 1, "Register list not consecutive");
static_assert(RISCV::F31_H == RISCV::F0_H + 31,
              "Register list not consecutive");
static_assert(RISCV::F1_F == RISCV::F0_F + 1, "Register list not consecutive");
static_assert(RISCV::F31_F == RISCV::F0_F + 31,
              "Register list not consecutive");
static_assert(RISCV::F1_D == RISCV::F0_D + 1, "Register list not consecutive");
static_assert(RISCV::F31_D == RISCV::F0_D + 31,
              "Register list not consecutive");
static_assert(RISCV::V1 == RISCV::V0 + 1, "Register list not consecutive");
static_assert(RISCV::V31 == RISCV::V0 + 31, "Register list not consecutive");

RISCVRegisterInfo::RISCVRegisterInfo(unsigned HwMode)
    : RISCVGenRegisterInfo(RISCV::X1, /*DwarfFlavour*/0, /*EHFlavor*/0,
                           /*PC*/0, HwMode) {}

const MCPhysReg *
RISCVRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  assert(!MF->getFunction().hasFnAttribute("interrupt") &&
         "Ventus GPGPU doesn't support interrupt!");
  auto &Subtarget = MF->getSubtarget<RISCVSubtarget>();
  switch (Subtarget.getTargetABI()) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case RISCVABI::ABI_ILP32:
  case RISCVABI::ABI_LP64:
    return CSR_ILP32_LP64_SaveList;
  case RISCVABI::ABI_ILP32F:
  case RISCVABI::ABI_LP64F:
    return CSR_ILP32F_LP64F_SaveList;
  case RISCVABI::ABI_ILP32D:
  case RISCVABI::ABI_LP64D:
    return CSR_ILP32D_LP64D_SaveList;
  }
}

BitVector RISCVRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  const RISCVFrameLowering *TFI = getFrameLowering(MF);
  BitVector Reserved(getNumRegs());

  // Mark any registers requested to be reserved as such
  for (size_t Reg = 0; Reg < getNumRegs(); Reg++) {
    if (MF.getSubtarget<RISCVSubtarget>().isRegisterReservedByUser(Reg))
      markSuperRegs(Reserved, Reg);
  }

  // Use markSuperRegs to ensure any register aliases are also reserved
  markSuperRegs(Reserved, RISCV::X0); // zero
  markSuperRegs(Reserved, RISCV::X2); // sp
  markSuperRegs(Reserved, RISCV::X8); // s0
  markSuperRegs(Reserved, RISCV::X3); // gp
  markSuperRegs(Reserved, RISCV::X4); // tp
  // Reserve the base register if we need to realign the stack and allocate
  // variable-sized objects at runtime.
  if (TFI->hasBP(MF))
    markSuperRegs(Reserved, RISCVABI::getBPReg()); // bp

  // Floating point environment registers.
  markSuperRegs(Reserved, RISCV::FRM);
  markSuperRegs(Reserved, RISCV::FFLAGS);

  markSuperRegs(Reserved, getPrivateMemoryBaseRegister(
                                const_cast<MachineFunction&>(MF)));

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool RISCVRegisterInfo::isAsmClobberable(const MachineFunction &MF,
                                         MCRegister PhysReg) const {
  return !MF.getSubtarget<RISCVSubtarget>().isRegisterReservedByUser(PhysReg);
}

const uint32_t *RISCVRegisterInfo::getNoPreservedMask() const {
  return CSR_NoRegs_RegMask;
}

// Frame indexes representing locations of CSRs which are given a fixed location
// by save/restore libcalls.
static const std::pair<unsigned, int> FixedCSRFIMap[] = {
  {/*ra*/  RISCV::X1,   -1},
  {/*s0*/  RISCV::X8,   -2},
  {/*s1*/  RISCV::X9,   -3},
  {/*s2*/  RISCV::X18,  -4},
  {/*s3*/  RISCV::X19,  -5},
  {/*s4*/  RISCV::X20,  -6},
  {/*s5*/  RISCV::X21,  -7},
  {/*s6*/  RISCV::X22,  -8},
  {/*s7*/  RISCV::X23,  -9},
  {/*s8*/  RISCV::X24,  -10},
  {/*s9*/  RISCV::X25,  -11},
  {/*s10*/ RISCV::X26,  -12},
  {/*s11*/ RISCV::X27,  -13}
};

bool RISCVRegisterInfo::hasReservedSpillSlot(const MachineFunction &MF,
                                             Register Reg,
                                             int &FrameIdx) const {
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  if (!RVFI->useSaveRestoreLibCalls(MF))
    return false;

  const auto *FII =
      llvm::find_if(FixedCSRFIMap, [&](auto P) { return P.first == Reg; });
  if (FII == std::end(FixedCSRFIMap))
    return false;

  FrameIdx = FII->second;
  return true;
}

/// Returns a lowest register that is not used at any point in the function.
///        If all registers are used, then this function will return
///         RISCV::NoRegister. If \p ReserveHighestVGPR = true, then return
///         highest unused register.
MCRegister RISCVRegisterInfo::findUnusedRegister(const MachineRegisterInfo &MRI,
                                              const TargetRegisterClass *RC,
                                              const MachineFunction &MF,
                                              bool ReserveHighestVGPR) const {
  if (ReserveHighestVGPR) {
    for (MCRegister Reg : reverse(*RC))
      if (MRI.isAllocatable(Reg) && !MRI.isPhysRegUsed(Reg))
        return Reg;
  } else {
    for (MCRegister Reg : *RC)
      if (MRI.isAllocatable(Reg) && !MRI.isPhysRegUsed(Reg))
        return Reg;
  }
  return MCRegister();
}

bool RISCVRegisterInfo::isSGPRReg(const MachineRegisterInfo &MRI,
                                  Register Reg) const {
  const TargetRegisterClass *RC;
  if (Reg.isVirtual())
    RC = MRI.getRegClass(Reg);
  else
    RC = getPhysRegClass(Reg);
  return RC ? isSGPRClass(RC) : false;
}

bool RISCVRegisterInfo::isVGPRReg(const MachineRegisterInfo &MRI,
                                  Register Reg) const {
  const TargetRegisterClass *RC;
  if (Reg.isVirtual())
    RC = MRI.getRegClass(Reg);
  else
    RC = getPhysRegClass(Reg);
  return RC ? isVGPRClass(RC) : false;
}

void RISCVRegisterInfo::insertRegToSet(const MachineRegisterInfo &MRI,
                    DenseSet<unsigned int> *CurrentRegisterAddedSet,
                    SubVentusProgramInfo *CurrentSubProgramInfo,
                    Register Reg) const {
  if (CurrentRegisterAddedSet->contains(Reg))
    return;

  // Beyond the limits of SGPR and VGPR
  if (Reg.id() < RISCV::V0 || Reg.id() > RISCV::X63)
    return;

  CurrentRegisterAddedSet->insert(Reg);

  if (!isSGPRReg(MRI, Reg))
    CurrentSubProgramInfo->VGPRUsage++;
  else
    CurrentSubProgramInfo->SGPRUsage++;
}

const Register RISCVRegisterInfo::getPrivateMemoryBaseRegister(
                        const MachineFunction &MF) const {
  // FIXME: V0-V31 are used for argument registers, so here we use V32 for
  // private memory based register, but V32 is beyond the 5 bits ranges, when
  // this register are used, one more instruction is used
  // since v0-v7 is used in variadic function arguments
  return  MF.getFunction().isVarArg() ? RISCV::V8 : RISCV::V32;
}

const TargetRegisterClass *
RISCVRegisterInfo::getPhysRegClass(MCRegister Reg) const {
  static const TargetRegisterClass *const BaseClasses[] = {
    /*
    &RISCV::VGPR_LO16RegClass,
    &RISCV::VGPR_HI16RegClass,
    &RISCV::SReg_LO16RegClass,
    &RISCV::SReg_HI16RegClass,
    &RISCV::SReg_32RegClass,
    */
    &RISCV::VGPRRegClass,
    &RISCV::GPRRegClass,
  };

  for (const TargetRegisterClass *BaseClass : BaseClasses) {
    if (BaseClass->contains(Reg)) {
      return BaseClass;
    }
  }
  assert(0 && "TODO: Add sub/super registers");
  return nullptr;
}

void RISCVRegisterInfo::adjustReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator II,
                                  const DebugLoc &DL, Register DestReg,
                                  Register SrcReg, StackOffset Offset,
                                  MachineInstr::MIFlag Flag,
                                  MaybeAlign RequiredAlign) const {

  if (DestReg == SrcReg && !Offset.getFixed() && !Offset.getScalable())
    return;

  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const RISCVInstrInfo *TII = ST.getInstrInfo();

  bool KillSrcReg = false;

  int64_t Val = Offset.getFixed();
  if (DestReg == SrcReg && Val == 0)
    return;

  const uint64_t Align = RequiredAlign.valueOrOne().value();

  if (isInt<12>(Val)) {
    BuildMI(MBB, II, DL, TII->get(RISCV::ADDI), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrcReg))
        .addImm(Val)
        .setMIFlag(Flag);
    return;
  }

  // Try to split the offset across two ADDIs. We need to keep the intermediate
  // result aligned after each ADDI.  We need to determine the maximum value we
  // can put in each ADDI. In the negative direction, we can use -2048 which is
  // always sufficiently aligned. In the positive direction, we need to find the
  // largest 12-bit immediate that is aligned.  Exclude -4096 since it can be
  // created with LUI.
  assert(Align < 2048 && "Required alignment too large");
  int64_t MaxPosAdjStep = 2048 - Align;
  if (Val > -4096 && Val <= (2 * MaxPosAdjStep)) {
    int64_t FirstAdj = Val < 0 ? -2048 : MaxPosAdjStep;
    Val -= FirstAdj;
    // Keep the intermediate aligned after each ADDI no matter for SP or TP
    BuildMI(MBB, II, DL, TII->get(RISCV::ADDI), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrcReg))
        .addImm(FirstAdj)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, TII->get(RISCV::ADDI), DestReg)
        .addReg(DestReg, RegState::Kill)
        .addImm(Val)
        .setMIFlag(Flag);
    return;
  }

  unsigned Opc = RISCV::ADD;
  if (Val < 0) {
    Val = -Val;
    Opc = RISCV::SUB;
  }

  Register ScratchReg = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  TII->movImm(MBB, II, DL, ScratchReg, Val, Flag);
  BuildMI(MBB, II, DL, TII->get(Opc), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrcReg))
      .addReg(ScratchReg, RegState::Kill)
      .setMIFlag(Flag);
}

void RISCVRegisterInfo::adjustPriMemRegOffset(MachineFunction &MF,
              MachineBasicBlock &MBB, MachineInstr &MI, int64_t Offset,
              Register PriMemReg, unsigned FIOperandNum) const{
  auto &MRI = MF.getRegInfo();
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const RISCVInstrInfo *TII = ST.getInstrInfo();
  assert(!isSGPRReg(MRI, PriMemReg) && "Private memory base address in VGPR");
  bool IsNegative = (Offset < -1024);
  (--MI.getIterator());
  Register ScratchReg = MRI.createVirtualRegister(&RISCV::VGPRRegClass);
  // FIXME: maybe it is better change offset once rather than insert a new
  // machine instruction??
  BuildMI(MBB, --MI.getIterator(), (--MI.getIterator())->getDebugLoc(),
    TII->get(RISCV::VADD_VI))
    .addReg(ScratchReg)
    .addReg(PriMemReg)
    .addImm(IsNegative ? (Offset / -1024) * 1024 : -(Offset / 1024) * 1024);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(IsNegative ?
          Offset + (Offset / -1024) * 1024
          : Offset - (Offset / 1024) * 1024);
  MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg,
                                            /*IsDef*/false,
                                            /*IsImp*/false,
                                            /*IsKill*/true);
}

/// This function is to eliminate frame index for MachineInstruction in
/// StoreRegToSlot/LoadRegFromSlot function
bool RISCVRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected non-zero SPAdj value");

  MachineInstr &MI = *II;
  MachineBasicBlock *MBB = MI.getParent();
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const RISCVRegisterInfo *RI = ST.getRegisterInfo();
  const RISCVInstrInfo *RII = ST.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  auto FrameIndexID = MF.getFrameInfo().getStackID(FrameIndex);

  Register FrameReg;
  StackOffset Offset = // FIXME: The FrameReg and Offset should be depended on
                       // divergency route.
      getFrameLowering(MF)->getFrameIndexReference(MF, FrameIndex, FrameReg);
  // TODO: finish
  // if(!RII->isVGPRMemoryAccess(MI))
  //   Offset -= StackOffset::getFixed(
  //             MF.getInfo<RISCVMachineFunctionInfo>()->getVarArgsSaveSize() -
  //             4);
  int64_t Lo11 = Offset.getFixed();
  Offset += StackOffset::getFixed(MI.getOperand(FIOperandNum + 1).getImm());

  if (!isInt<32>(Offset.getFixed())) {
    report_fatal_error(
        "Frame offsets outside of the signed 32-bit range not supported");
  }
  // FIXME: vsw/vlw has 11 bits immediates
  if (MI.getOpcode() == RISCV::ADDI && !isInt<11>(Offset.getFixed())) {
    // We chose to emit the canonical immediate sequence rather than folding
    // the offset into the using add under the theory that doing so doesn't
    // save dynamic instruction count and some target may fuse the canonical
    // 32 bit immediate sequence.  We still need to clear the portion of the
    // offset encoded in the immediate.
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(0);
  } else {
    // We can encode an add with 12 bit signed immediate in the immediate
    // operand of our user instruction.  As a result, the remaining
    // offset can by construction, at worst, a LUI and a ADD.
    int64_t Val = Offset.getFixed();
    Lo11 = SignExtend64<12>(Val);

    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Lo11);
    Offset =
        StackOffset::get((uint64_t)Val - (uint64_t)Lo11, Offset.getScalable());
    // adjustReg(*II->getParent(), II, DL, DestReg, FrameReg, Offset,
    //   MachineInstr::NoFlags, std::nullopt);
  }
  Register DestReg = MI.getOperand(0).getReg();
  if (Offset.getScalable() || Offset.getFixed()) {

    if (MI.getOpcode() == RISCV::ADDI)
      DestReg = MI.getOperand(0).getReg();
    else
      DestReg = MRI.createVirtualRegister(&RISCV::GPRRegClass);
    // !!!Very important for adjust
    adjustReg(*II->getParent(), II, DL, DestReg, FrameReg, Offset,
              MachineInstr::NoFlags, std::nullopt);
  }
  if (MI.getOpcode() == RISCV::ADDI &&
      static_cast<unsigned>(FrameIndexID) == RISCVStackID::VGPRSpill) {
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(FrameReg,
                          /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
  }

  if (RII->isPrivateMemoryAccess(MI) && FrameReg == RISCV::X4) {
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(getPrivateMemoryBaseRegister(MF),
                          /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
    // simm11 locates in range [-1024, 1023], if offset not in this range, then
    // we legalize the offset
    if (!isInt<12>(Lo11))
      adjustPriMemRegOffset(MF, *MI.getParent(), MI, Lo11,
                            getPrivateMemoryBaseRegister(MF), FIOperandNum);
  }

  if (RII->isPrivateMemoryAccess(MI) && FrameReg == RISCV::X2) {
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(getPrivateMemoryBaseRegister(MF),
                          /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
    // simm11 locates in range [-1024, 1023], if offset not in this range, then
    // we legalize the offset
    MI.setDesc(RII->get(RII->getUniformMemoryOpcode(MI)));
    if (!isInt<12>(Lo11))
      adjustPriMemRegOffset(MF, *MI.getParent(), MI, Lo11,
                            getPrivateMemoryBaseRegister(MF), FIOperandNum);
  }

  // else
  //   MI.getOperand(FIOperandNum)
  //       .ChangeToRegister(FrameReg, /*IsDef*/ false,
  //                         /*IsImp*/ false,
  //                         /*IsKill*/ false);
  if (RII->isUniformMemoryAccess(MI) && FrameReg == RISCV::X4) {
    Register DestReg =
        MF.getRegInfo().createVirtualRegister(&RISCV::VGPRRegClass);
    MI.setDesc(RII->get(RII->getPrivateMemoryOpcode(MI)));
    BuildMI(*MBB, II, DL, RII->get(RISCV::VMV_V_X), DestReg)
        .addReg(MI.getOperand(FIOperandNum - 1).getReg());
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(getPrivateMemoryBaseRegister(MF), /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
    MI.getOperand(FIOperandNum - 1)
        .ChangeToRegister(DestReg, /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);

    return false;
  }

  if (RII->isLocalMemoryAccess(MI) && FrameReg == RISCV::X4) {
    Register DestReg =
        MF.getRegInfo().createVirtualRegister(&RISCV::VGPRRegClass);
    BuildMI(*MBB, II, DL, RII->get(RISCV::VMV_V_X), DestReg).addReg(FrameReg);
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(getFrameRegister(MF), /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
    MI.setDesc(RII->get(RII->getPrivateMemoryOpcode(MI)));
    return false;
  }

  if (RII->isLocalMemoryAccess(MI) && FrameReg == RISCV::X2) {
    Register DestReg =
        MF.getRegInfo().createVirtualRegister(&RISCV::VGPRRegClass);
    BuildMI(*MBB, II, DL, RII->get(RISCV::VMV_V_X), DestReg).addReg(FrameReg);
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(DestReg, /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
    return false;
  }

  if (RII->isPrivateMemoryAccess(MI))
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(getPrivateMemoryBaseRegister(MF), /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);
  else
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(DestReg == MI.getOperand(0).getReg() ? FrameReg
                                                               : DestReg,
                          /*IsDef*/ false,
                          /*IsImp*/ false,
                          /*IsKill*/ false);

  // If after materializing the adjustment, we have a pointless ADDI, remove it
  if (MI.getOpcode() == RISCV::ADDI &&
      MI.getOperand(0).getReg() == MI.getOperand(1).getReg() &&
      MI.getOperand(2).getImm() == 0) {
    MI.eraseFromParent();
    return true;
  }

  return false;
}

Register RISCVRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MF.getInfo<RISCVMachineFunctionInfo>()->isEntryFunction()
      ? RISCV::X2 : RISCV::X4;
}

const uint32_t *
RISCVRegisterInfo::getCallPreservedMask(const MachineFunction & MF,
                                        CallingConv::ID CC) const {
  auto &Subtarget = MF.getSubtarget<RISCVSubtarget>();

  if (CC == CallingConv::GHC)
    return CSR_NoRegs_RegMask;
  switch (Subtarget.getTargetABI()) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case RISCVABI::ABI_ILP32:
  case RISCVABI::ABI_LP64:
    return CSR_ILP32_LP64_RegMask;
  case RISCVABI::ABI_ILP32F:
  case RISCVABI::ABI_LP64F:
    return CSR_ILP32F_LP64F_RegMask;
  case RISCVABI::ABI_ILP32D:
  case RISCVABI::ABI_LP64D:
    return CSR_ILP32D_LP64D_RegMask;
  }
}

const TargetRegisterClass *
RISCVRegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                             const MachineFunction &) const {
  return RC;
}

void RISCVRegisterInfo::getOffsetOpcodes(const StackOffset &Offset,
                                         SmallVectorImpl<uint64_t> &Ops) const {
  // VLENB is the length of a vector register in bytes. We use <vscale x 8 x i8>
  // to represent one vector register. The dwarf offset is
  // VLENB * scalable_offset / 8.
  assert(Offset.getScalable() % 8 == 0 && "Invalid frame offset");

  // Add fixed-sized offset using existing DIExpression interface.
  DIExpression::appendOffset(Ops, Offset.getFixed());

  unsigned VLENB = getDwarfRegNum(RISCV::VLENB, true);
  int64_t VLENBSized = Offset.getScalable() / 8;
  if (VLENBSized > 0) {
    Ops.push_back(dwarf::DW_OP_constu);
    Ops.push_back(VLENBSized);
    Ops.append({dwarf::DW_OP_bregx, VLENB, 0ULL});
    Ops.push_back(dwarf::DW_OP_mul);
    Ops.push_back(dwarf::DW_OP_plus);
  } else if (VLENBSized < 0) {
    Ops.push_back(dwarf::DW_OP_constu);
    Ops.push_back(-VLENBSized);
    Ops.append({dwarf::DW_OP_bregx, VLENB, 0ULL});
    Ops.push_back(dwarf::DW_OP_mul);
    Ops.push_back(dwarf::DW_OP_minus);
  }
}

unsigned
RISCVRegisterInfo::getRegisterCostTableIndex(const MachineFunction &MF) const {
  return MF.getSubtarget<RISCVSubtarget>().hasStdExtC() ? 1 : 0;
}

// Add two address hints to improve chances of being able to use a compressed
// instruction.
bool RISCVRegisterInfo::getRegAllocationHints(
    Register VirtReg, ArrayRef<MCPhysReg> Order,
    SmallVectorImpl<MCPhysReg> &Hints, const MachineFunction &MF,
    const VirtRegMap *VRM, const LiveRegMatrix *Matrix) const {
  const MachineRegisterInfo *MRI = &MF.getRegInfo();

  bool BaseImplRetVal = TargetRegisterInfo::getRegAllocationHints(
      VirtReg, Order, Hints, MF, VRM, Matrix);

  if (!VRM || DisableRegAllocHints)
    return BaseImplRetVal;

  // Add any two address hints after any copy hints.
  SmallSet<Register, 4> TwoAddrHints;

  auto tryAddHint = [&](const MachineOperand &VRRegMO, const MachineOperand &MO,
                        bool NeedGPRC) -> void {
    Register Reg = MO.getReg();
    Register PhysReg =
        Register::isPhysicalRegister(Reg) ? Reg : Register(VRM->getPhys(Reg));
    if (PhysReg && (!NeedGPRC || RISCV::GPRCRegClass.contains(PhysReg))) {
      assert(!MO.getSubReg() && !VRRegMO.getSubReg() && "Unexpected subreg!");
      if (!MRI->isReserved(PhysReg) && !is_contained(Hints, PhysReg))
        TwoAddrHints.insert(PhysReg);
    }
  };

  // This is all of the compressible binary instructions. If an instruction
  // needs GPRC register class operands \p NeedGPRC will be set to true.
  auto isCompressible = [](const MachineInstr &MI, bool &NeedGPRC) {
    NeedGPRC = false;
    switch (MI.getOpcode()) {
    default:
      return false;
    case RISCV::AND:
    case RISCV::OR:
    case RISCV::XOR:
    case RISCV::SUB:
    case RISCV::ADDW:
    case RISCV::SUBW:
      NeedGPRC = true;
      return true;
    case RISCV::ANDI:
      NeedGPRC = true;
      return MI.getOperand(2).isImm() && isInt<6>(MI.getOperand(2).getImm());
    case RISCV::SRAI:
    case RISCV::SRLI:
      NeedGPRC = true;
      return true;
    case RISCV::ADD:
    case RISCV::SLLI:
      return true;
    case RISCV::ADDI:
    case RISCV::ADDIW:
      return MI.getOperand(2).isImm() && isInt<6>(MI.getOperand(2).getImm());
    }
  };

  // Returns true if this operand is compressible. For non-registers it always
  // returns true. Immediate range was already checked in isCompressible.
  // For registers, it checks if the register is a GPRC register. reg-reg
  // instructions that require GPRC need all register operands to be GPRC.
  auto isCompressibleOpnd = [&](const MachineOperand &MO) {
    if (!MO.isReg())
      return true;
    Register Reg = MO.getReg();
    Register PhysReg =
        Register::isPhysicalRegister(Reg) ? Reg : Register(VRM->getPhys(Reg));
    return PhysReg && RISCV::GPRCRegClass.contains(PhysReg);
  };

  for (auto &MO : MRI->reg_nodbg_operands(VirtReg)) {
    const MachineInstr &MI = *MO.getParent();
    unsigned OpIdx = MI.getOperandNo(&MO);
    bool NeedGPRC;
    if (isCompressible(MI, NeedGPRC)) {
      if (OpIdx == 0 && MI.getOperand(1).isReg()) {
        if (!NeedGPRC || isCompressibleOpnd(MI.getOperand(2)))
          tryAddHint(MO, MI.getOperand(1), NeedGPRC);
        if (MI.isCommutable() && MI.getOperand(2).isReg() &&
            (!NeedGPRC || isCompressibleOpnd(MI.getOperand(1))))
          tryAddHint(MO, MI.getOperand(2), NeedGPRC);
      } else if (OpIdx == 1 &&
                 (!NeedGPRC || isCompressibleOpnd(MI.getOperand(2)))) {
        tryAddHint(MO, MI.getOperand(0), NeedGPRC);
      } else if (MI.isCommutable() && OpIdx == 2 &&
                 (!NeedGPRC || isCompressibleOpnd(MI.getOperand(1)))) {
        tryAddHint(MO, MI.getOperand(0), NeedGPRC);
      }
    }
  }

  for (MCPhysReg OrderReg : Order)
    if (TwoAddrHints.count(OrderReg))
      Hints.push_back(OrderReg);

  return BaseImplRetVal;
}
