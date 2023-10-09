//===-- RISCVFrameLowering.cpp - RISCV Frame Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISCV implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "RISCVFrameLowering.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCV.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/MC/MCDwarf.h"
#include <algorithm>


using namespace llvm;

// For now we use x18, a.k.a s2, as pointer to shadow call stack.
// User should explicitly set -ffixed-x18 and not use x18 in their asm.
static void emitSCSPrologue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  if (!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack))
    return;

  const auto &STI = MF.getSubtarget<RISCVSubtarget>();
  Register RAReg = STI.getRegisterInfo()->getRARegister();

  // Do not save RA to the SCS if it's not saved to the regular stack,
  // i.e. RA is not at risk of being overwritten.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (llvm::none_of(
          CSI, [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  Register SCSPReg = RISCVABI::getSCSPReg();

  auto &Ctx = MF.getFunction().getContext();
  if (!STI.isRegisterReservedByUser(SCSPReg)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "x18 not reserved by user for Shadow Call Stack."});
    return;
  }

  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  if (RVFI->useSaveRestoreLibCalls(MF)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Shadow Call Stack cannot be combined with Save/Restore LibCalls."});
    return;
  }

  const RISCVInstrInfo *TII = STI.getInstrInfo();
  bool IsRV64 = STI.hasFeature(RISCV::Feature64Bit);
  int64_t SlotSize = STI.getXLen() / 8;
  // Store return address to shadow call stack
  // s[w|d]  ra, 0(s2)
  // addi    s2, s2, [4|8]
  BuildMI(MBB, MI, DL, TII->get(IsRV64 ? RISCV::SD : RISCV::SW))
      .addReg(RAReg)
      .addReg(SCSPReg)
      .addImm(0)
      .setMIFlag(MachineInstr::FrameSetup);
  BuildMI(MBB, MI, DL, TII->get(RISCV::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(SlotSize)
      .setMIFlag(MachineInstr::FrameSetup);
}

static void emitSCSEpilogue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  if (!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack))
    return;

  const auto &STI = MF.getSubtarget<RISCVSubtarget>();
  Register RAReg = STI.getRegisterInfo()->getRARegister();

  // See emitSCSPrologue() above.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (llvm::none_of(
          CSI, [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  Register SCSPReg = RISCVABI::getSCSPReg();

  auto &Ctx = MF.getFunction().getContext();
  if (!STI.isRegisterReservedByUser(SCSPReg)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "x18 not reserved by user for Shadow Call Stack."});
    return;
  }

  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  if (RVFI->useSaveRestoreLibCalls(MF)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Shadow Call Stack cannot be combined with Save/Restore LibCalls."});
    return;
  }

  const RISCVInstrInfo *TII = STI.getInstrInfo();
  bool IsRV64 = STI.hasFeature(RISCV::Feature64Bit);
  int64_t SlotSize = STI.getXLen() / 8;
  // Load return address from shadow call stack
  // l[w|d]  ra, -[4|8](s2)
  // addi    s2, s2, -[4|8]
  BuildMI(MBB, MI, DL, TII->get(IsRV64 ? RISCV::LD : RISCV::LW))
      .addReg(RAReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
  BuildMI(MBB, MI, DL, TII->get(RISCV::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
}

// Get the ID of the libcall used for spilling and restoring callee saved
// registers. The ID is representative of the number of registers saved or
// restored by the libcall, except it is zero-indexed - ID 0 corresponds to a
// single register.
static int getLibCallID(const MachineFunction &MF,
                        const std::vector<CalleeSavedInfo> &CSI) {
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

  if (CSI.empty() || !RVFI->useSaveRestoreLibCalls(MF))
    return -1;

  Register MaxReg = RISCV::NoRegister;
  for (auto &CS : CSI)
    // RISCVRegisterInfo::hasReservedSpillSlot assigns negative frame indexes to
    // registers which can be saved by libcall.
    if (CS.getFrameIdx() < 0)
      MaxReg = std::max(MaxReg.id(), CS.getReg().id());

  if (MaxReg == RISCV::NoRegister)
    return -1;

  switch (MaxReg) {
  default:
    llvm_unreachable("Something has gone wrong!");
  case /*s11*/ RISCV::X27: return 12;
  case /*s10*/ RISCV::X26: return 11;
  case /*s9*/  RISCV::X25: return 10;
  case /*s8*/  RISCV::X24: return 9;
  case /*s7*/  RISCV::X23: return 8;
  case /*s6*/  RISCV::X22: return 7;
  case /*s5*/  RISCV::X21: return 6;
  case /*s4*/  RISCV::X20: return 5;
  case /*s3*/  RISCV::X19: return 4;
  case /*s2*/  RISCV::X18: return 3;
  case /*s1*/  RISCV::X9:  return 2;
  case /*s0*/  RISCV::X8:  return 1;
  case /*ra*/  RISCV::X1:  return 0;
  }
}

// Get the name of the libcall used for spilling callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getSpillLibCallName(const MachineFunction &MF,
                    const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const SpillLibCalls[] = {
    "__riscv_save_0",
    "__riscv_save_1",
    "__riscv_save_2",
    "__riscv_save_3",
    "__riscv_save_4",
    "__riscv_save_5",
    "__riscv_save_6",
    "__riscv_save_7",
    "__riscv_save_8",
    "__riscv_save_9",
    "__riscv_save_10",
    "__riscv_save_11",
    "__riscv_save_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return SpillLibCalls[LibCallID];
}

// Get the name of the libcall used for restoring callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getRestoreLibCallName(const MachineFunction &MF,
                      const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const RestoreLibCalls[] = {
    "__riscv_restore_0",
    "__riscv_restore_1",
    "__riscv_restore_2",
    "__riscv_restore_3",
    "__riscv_restore_4",
    "__riscv_restore_5",
    "__riscv_restore_6",
    "__riscv_restore_7",
    "__riscv_restore_8",
    "__riscv_restore_9",
    "__riscv_restore_10",
    "__riscv_restore_11",
    "__riscv_restore_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return RestoreLibCalls[LibCallID];
}

// Return true if the specified function should have a dedicated frame
// pointer register.  This is true if frame pointer elimination is
// disabled, if it needs dynamic stack realignment, if the function has
// variable sized allocas, or if the frame address is taken.
bool RISCVFrameLowering::hasFP(const MachineFunction &MF) const {
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // For entry functions we can use an immediate offset in most cases, so the
  // presence of calls doesn't imply we need a distinct frame pointer.
  if (MFI.hasCalls() &&
      !MF.getInfo<RISCVMachineFunctionInfo>()->isEntryFunction()) {
    // All offsets are unsigned, so need to be addressed in the same direction
    // as stack growth.

    // FIXME: This function is pretty broken, since it can be called before the
    // frame layout is determined or CSR spills are inserted.
    return MFI.getStackSize() != 0;
  }
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         RegInfo->hasStackRealignment(MF) || MFI.hasVarSizedObjects() ||
         MFI.isFrameAddressTaken();
}

bool RISCVFrameLowering::hasBP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  // If we do not reserve stack space for outgoing arguments in prologue,
  // we will adjust the stack pointer before call instruction. After the
  // adjustment, we can not use SP to access the stack objects for the
  // arguments. Instead, use BP to access these stack objects.
  return (MFI.hasVarSizedObjects() ||
          (!hasReservedCallFrame(MF) && (!MFI.isMaxCallFrameSizeComputed() ||
                                         MFI.getMaxCallFrameSize() != 0))) &&
         TRI->hasStackRealignment(MF);
}

// Determines the size of the frame and maximum call frame size.
void RISCVFrameLowering::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  // auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t FrameSize = MFI.getStackSize();

  // Get the alignment.
  Align StackAlign = getStackAlign();

  // Make sure the frame is aligned.
  FrameSize = alignTo(FrameSize, StackAlign);

  // Update frame info.
  MFI.setStackSize(FrameSize);
}

// Returns the register used to hold the frame pointer.
static Register getFPReg(const RISCVSubtarget &STI) { return RISCV::X8; }

// Returns the register used to hold the per-thread stack pointer.
static Register getTPReg(const RISCVSubtarget &STI) { return RISCV::X4; }

// Returns the register used to hold the stack pointer.
static Register getSPReg(const RISCVSubtarget &STI) { return RISCV::X2; }

static SmallVector<CalleeSavedInfo, 8>
getNonLibcallCSI(const MachineFunction &MF,
                 const std::vector<CalleeSavedInfo> &CSI) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  SmallVector<CalleeSavedInfo, 8> NonLibcallCSI;

  for (auto &CS : CSI) {
    int FI = CS.getFrameIdx();
    // TODO: For now, we don't define VGPR callee saved registers, when we later
    // add VGPR callee saved register, remember to modify here
    if (FI >= 0 && (MFI.getStackID(FI) == RISCVStackID::Default ||
                    MFI.getStackID(FI) == RISCVStackID::SGPRSpill))
      NonLibcallCSI.push_back(CS);
  }

  return NonLibcallCSI;
}

void RISCVFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *RMFI = MF.getInfo<RISCVMachineFunctionInfo>();
  const RISCVRegisterInfo *RI = STI.getRegisterInfo();
  auto *CurrentProgramInfo = const_cast<VentusProgramInfo *>(
                                                    STI.getVentusProgramInfo());
  const RISCVInstrInfo *TII = STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  bool IsEntryFunction = RMFI->isEntryFunction();

  Register SPReg = getSPReg(STI);
  Register TPReg = getTPReg(STI);

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Emit prologue for shadow call stack.
  emitSCSPrologue(MF, MBB, MBBI, DL);

  // Since spillCalleeSavedRegisters may have inserted a libcall, skip past
  // any instructions marked as FrameSetup
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup))
    ++MBBI;

  // Determine the correct frame layout
  determineFrameLayout(MF);

  // Determine stack ID for each frame index
  determineStackID(MF);

  // If libcalls are used to spill and restore callee-saved registers, the frame
  // has two sections; the opaque section managed by the libcalls, and the
  // section managed by MachineFrameInfo which can also hold callee saved
  // registers in fixed stack slots, both of which have negative frame indices.
  // This gets even more complicated when incoming arguments are passed via the
  // stack, as these too have negative frame indices. An example is detailed
  // below:
  //
  //  | incoming arg | <- FI[-3]
  //  | libcallspill |
  //  | calleespill  | <- FI[-2]
  //  | calleespill  | <- FI[-1]
  //  | this_frame   | <- FI[0]
  //
  // For negative frame indices, the offset from the frame pointer will differ
  // depending on which of these groups the frame index applies to.
  // The following calculates the correct offset knowing the number of callee
  // saved registers spilt by the two methods.
  if (int LibCallRegs = getLibCallID(MF, MFI.getCalleeSavedInfo()) + 1) {
    // Calculate the size of the frame managed by the libcall. The libcalls are
    // implemented such that the stack will always be 16 byte aligned.
    unsigned LibCallFrameSize = alignTo((STI.getXLen() / 8) * LibCallRegs, 16);
    RMFI->setLibCallStackSize(LibCallFrameSize);
  }

  uint64_t SPStackSize = getStackSize(MF, RISCVStackID::SGPRSpill);
  uint64_t TPStackSize = getStackSize(MF, RISCVStackID::VGPRSpill);
  CurrentProgramInfo->PDSMemory += TPStackSize;
  // FIXME: need to add local data declaration calculation
  CurrentProgramInfo->LDSMemory += SPStackSize;
  //uint64_t RealStackSize = IsEntryFunction ?
  //                                SPStackSize + RMFI->getLibCallStackSize() :
  //                                TPStackSize + RMFI->getLibCallStackSize();

  // Early exit if there is no need to allocate on the stack
  if (MFI.getStackSize() == 0 && !MFI.adjustsStack())
    return;

  // If the stack pointer has been marked as reserved, then produce an error if
  // the frame requires stack allocation
  if (STI.isRegisterReservedByUser(SPReg))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "Stack pointer required, but has been reserved."});

  if (STI.isRegisterReservedByUser(TPReg))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "Thread pointer required, but has been reserved."});

  // Allocate space on the local-mem stack and private-mem stack if necessary.
  if(SPStackSize) {
    RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg,
                  StackOffset::getFixed(SPStackSize),
                  MachineInstr::FrameSetup, getStackAlign());

    // Emit ".cfi_def_cfa_offset SPStackSize"
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, SPStackSize));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex)
        .setMIFlag(MachineInstr::FrameSetup);
  }

  if(TPStackSize) {
    RI->adjustReg(MBB, MBBI, DL, TPReg, TPReg,
                  StackOffset::getFixed(TPStackSize),
                  MachineInstr::FrameSetup, getStackAlign());

    // Emit ".cfi_def_cfa_offset TPStackSize"
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, TPStackSize));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex)
        .setMIFlag(MachineInstr::FrameSetup);
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::VMV_V_X),
        RI->getPrivateMemoryBaseRegister(MF))
      .addReg(TPReg);
  }

  const auto &CSI = MFI.getCalleeSavedInfo();

  // The frame pointer is callee-saved, and code has been generated for us to
  // save it to the stack. We need to skip over the storing of callee-saved
  // registers as the frame pointer must be modified after it has been saved
  // to the stack, not before.
  // FIXME: assumes exactly one instruction is used to save each callee-saved
  // register.
  std::advance(MBBI, getNonLibcallCSI(MF, CSI).size());

  // Iterate over list of callee-saved registers and emit .cfi_offset
  // directives.
  for (const auto &Entry : CSI) {
    int FrameIdx = Entry.getFrameIdx();
    int64_t Offset;
    // Offsets for objects with fixed locations (IE: those saved by libcall) are
    // simply calculated from the frame index.
    if (FrameIdx < 0)
      Offset = FrameIdx * (int64_t) STI.getXLen() / 8;
    else
      Offset = MFI.getObjectOffset(Entry.getFrameIdx()) -
               RMFI->getLibCallStackSize();
    Register Reg = Entry.getReg();
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
        nullptr, RI->getDwarfRegNum(Reg, true), Offset));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex)
        .setMIFlag(MachineInstr::FrameSetup);
  }
}

void RISCVFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const RISCVRegisterInfo *RI = STI.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  Register SPReg = getSPReg(STI);
  Register TPReg = getTPReg(STI);

  // Get the insert location for the epilogue. If there were no terminators in
  // the block, get the last instruction.
  MachineBasicBlock::iterator MBBI = MBB.end();
  DebugLoc DL;
  if (!MBB.empty()) {
    MBBI = MBB.getLastNonDebugInstr();
    if (MBBI != MBB.end())
      DL = MBBI->getDebugLoc();

    MBBI = MBB.getFirstTerminator();

    // If callee-saved registers are saved via libcall, place stack adjustment
    // before this call.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;
  }

  const auto &CSI = getNonLibcallCSI(MF, MFI.getCalleeSavedInfo());

  // Skip to before the restores of callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  auto LastFrameDestroy = MBBI;
  if (!CSI.empty())
    LastFrameDestroy = std::prev(MBBI, CSI.size());

  // Get 2 stack size for TP and SP
  uint64_t SPStackSize = getStackSize(MF, RISCVStackID::SGPRSpill);
  uint64_t TPStackSize = getStackSize(MF, RISCVStackID::VGPRSpill);

  // Deallocate stack
  if(SPStackSize)
    RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg,
                  StackOffset::getFixed(-SPStackSize),
                  MachineInstr::FrameDestroy, getStackAlign());
  if(TPStackSize)
    RI->adjustReg(MBB, MBBI, DL, TPReg, TPReg,
                  StackOffset::getFixed(-TPStackSize),
                  MachineInstr::FrameDestroy, getStackAlign());

  // Emit epilogue for shadow call stack.
  emitSCSEpilogue(MF, MBB, MBBI, DL);
}

uint64_t RISCVFrameLowering::getExtractedStackOffset(const MachineFunction &MF,
  unsigned FI, RISCVStackID::Value Stack) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = 0;
  for(int I = FI + 1; I != MFI.getObjectIndexEnd(); I++) {
    if(static_cast<unsigned>(MFI.getStackID(I)) != Stack) {
      // Need to consider the alignment for different frame index
      uint64_t Size = MFI.getObjectSize(I);
      StackSize +=  Size;
    }
  }
  return StackSize;
}

StackOffset
RISCVFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                           Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  // Callee-saved registers should be referenced relative to the stack
  // pointer (positive offset), otherwise use the frame pointer (negative
  // offset).
  const auto &CSI = getNonLibcallCSI(MF, MFI.getCalleeSavedInfo());
  int MinCSFI = 0;
  int MaxCSFI = -1;
  auto StackID = MFI.getStackID(FI);

  assert((StackID == RISCVStackID::Default ||
          StackID == RISCVStackID::SGPRSpill ||
          StackID == RISCVStackID::VGPRSpill) &&
         "Unexpected stack ID for the frame object.");
  uint8_t Stack = MFI.getStackID(FI);
  StackOffset Offset =
      StackOffset::getFixed(MFI.getObjectOffset(FI) - getOffsetOfLocalArea()
         -getExtractedStackOffset(MF, FI, RISCVStackID::Value(Stack))
         + MFI.getOffsetAdjustment());



  // Different stacks for sALU and vALU threads.
  FrameReg = StackID == RISCVStackID::SGPRSpill ? RISCV::X2 : RISCV::X4;

  if (CSI.size()) {
    // For callee saved registers
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
    if (FI >= MinCSFI && FI <= MaxCSFI) {
      Offset -= StackOffset::getFixed(RVFI->getVarArgsSaveSize());
      return Offset;
    }
  }
  // TODO: This only saves sGPR CSRs, as we haven't define vGPR CSRs
  // within getNonLibcallCSI.
  // if (FI >= MinCSFI && FI <= MaxCSFI) {
  Offset -= StackOffset::getFixed(
    getStackSize(const_cast<MachineFunction&>(MF),
                  (RISCVStackID::Value)StackID));
  return Offset;
}

void RISCVFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // Unconditionally spill RA and FP only if the function uses a frame
  // pointer.
  if (hasFP(MF)) {
    SavedRegs.set(RISCV::X1);
    SavedRegs.set(RISCV::X8);
  }
  // Mark BP as used if function has dedicated base pointer.
  if (hasBP(MF))
    SavedRegs.set(RISCVABI::getBPReg());
}

static unsigned estimateFunctionSizeInBytes(const MachineFunction &MF,
                                            const RISCVInstrInfo &TII) {
  unsigned FnSize = 0;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      // Far branches over 20-bit offset will be relaxed in branch relaxation
      // pass. In the worst case, conditional branches will be relaxed into
      // the following instruction sequence. Unconditional branches are
      // relaxed in the same way, with the exception that there is no first
      // branch instruction.
      //
      //        foo
      //        bne     t5, t6, .rev_cond # `TII->getInstSizeInBytes(MI)` bytes
      //        sd      s11, 0(sp)        # 4 bytes, or 2 bytes in RVC
      //        jump    .restore, s11     # 8 bytes
      // .rev_cond
      //        bar
      //        j       .dest_bb          # 4 bytes, or 2 bytes in RVC
      // .restore:
      //        ld      s11, 0(sp)        # 4 bytes, or 2 bytes in RVC
      // .dest:
      //        baz
      if (MI.isConditionalBranch())
        FnSize += TII.getInstSizeInBytes(MI);
      if (MI.isConditionalBranch() || MI.isUnconditionalBranch()) {
        if (MF.getSubtarget<RISCVSubtarget>().hasStdExtC())
          FnSize += 2 + 8 + 2 + 2;
        else
          FnSize += 4 + 8 + 4 + 4;
        continue;
      }

      FnSize += TII.getInstSizeInBytes(MI);
    }
  }
  return FnSize;
}

// Not preserve stack space within prologue for outgoing variables when the
// function contains variable size objects or there are vector objects accessed
// by the frame pointer.
// Let eliminateCallFramePseudoInstr preserve stack space for it.
bool RISCVFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator RISCVFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {

  // const RISCVMachineFunctionInfo *MFI = MF.getInfo<RISCVMachineFunctionInfo>();
  // Kernel and normal function has different stack pointer for Ventus GPGPU.
  Register SPReg = RISCV::X4; // MFI->isEntryFunction() ? RISCV::X2 : RISCV::X4;
  DebugLoc DL = MI->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = MI->getOperand(0).getImm();

    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);

      if (MI->getOpcode() == RISCV::ADJCALLSTACKDOWN)
        Amount = -Amount;

      const RISCVRegisterInfo &RI = *STI.getRegisterInfo();
      RI.adjustReg(MBB, MI, DL, SPReg, SPReg, StackOffset::getFixed(Amount),
                   MachineInstr::NoFlags, getStackAlign());
    }
  }

  return MBB.erase(MI);
}

// We would like to split the SP adjustment to reduce prologue/epilogue
// as following instructions. In this way, the offset of the callee saved
// register could fit in a single store.
//   add     sp,sp,-2032
//   sw      ra,2028(sp)
//   sw      s0,2024(sp)
//   sw      s1,2020(sp)
//   sw      s3,2012(sp)
//   sw      s4,2008(sp)
//   add     sp,sp,-64
uint64_t
RISCVFrameLowering::getFirstSPAdjustAmount(const MachineFunction &MF) const {
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  uint64_t StackSize = MFI.getStackSize();

  // Disable SplitSPAdjust if save-restore libcall is used. The callee-saved
  // registers will be pushed by the save-restore libcalls, so we don't have to
  // split the SP adjustment in this case.
  if (RVFI->getLibCallStackSize())
    return 0;

  // Align to VSW/VLW signed 11 bits offset
  // Return the FirstSPAdjustAmount if the StackSize can not fit in a signed
  // 11-bit and there exists a callee-saved register needing to be pushed.
  if (!isInt<11>(StackSize) && (CSI.size() > 0)) {
    // FirstSPAdjustAmount is chosen as (1024 - StackAlign) because 1024 will
    // cause sp = sp + 1024 in the epilogue to be split into multiple
    // instructions. Offsets smaller than 1024 can fit in a single load/store
    // instruction, and we have to stick with the stack alignment. 1024 has
    // 16-byte alignment. The stack alignment for RV32 and RV64 is 16 and for
    // RV32E it is 4. So (1024 - StackAlign) will satisfy the stack alignment.
    return 1024 - getStackAlign().value();
  }
  return 0;
}

uint64_t RISCVFrameLowering::getStackSize(MachineFunction &MF,
                                          RISCVStackID::Value ID) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = 0;

  for(int I = MFI.getObjectIndexBegin(); I != MFI.getObjectIndexEnd(); I++) {
    if(static_cast<unsigned>(MFI.getStackID(I)) == ID) {
      // Need to consider the alignment for different frame index
      uint64_t Size = ((MFI.getObjectSize(I) + 3) >> 2) * 4;
      StackSize += Size;
    }

  }
  return StackSize;
}

void RISCVFrameLowering::determineStackID(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  for(int I = MFI.getObjectIndexBegin(); I != MFI.getObjectIndexEnd(); I++) {
    // FIXME: There is no sGPR spill stack!
    // MFI.setStackID(I, RISCVStackID::VGPRSpill);

    MachinePointerInfo PtrInfo = MachinePointerInfo::getFixedStack(MF,I);
    if(MFI.getStackID(I) != RISCVStackID::SGPRSpill &&
       PtrInfo.getAddrSpace() == RISCVAS::PRIVATE_ADDRESS)
      MFI.setStackID(I, RISCVStackID::VGPRSpill);
    else
     MFI.setStackID(I, RISCVStackID::SGPRSpill);
  }
}

bool RISCVFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  const char *SpillLibCall = getSpillLibCallName(*MF, CSI);
  if (SpillLibCall) {
    // Add spill libcall via non-callee-saved register t0.
    BuildMI(MBB, MI, DL, TII.get(RISCV::PseudoCALLReg), RISCV::X5)
        .addExternalSymbol(SpillLibCall, RISCVII::MO_CALL)
        .setMIFlag(MachineInstr::FrameSetup);

    // Add registers spilled in libcall as liveins.
    for (auto &CS : CSI)
      MBB.addLiveIn(CS.getReg());
  }

  // Manually spill values not spilled by libcall.
  const auto &NonLibcallCSI = getNonLibcallCSI(*MF, CSI);
  for (auto &CS : NonLibcallCSI) {
    // Insert the spill to the stack frame.
    Register Reg = CS.getReg();

    const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
    // TODO: Have we allocated stack for vGPR spilling?
    if(Reg.id() < RISCV::V0 || Reg.id() > RISCV::V255) {
      MF->getFrameInfo().setStackID(CS.getFrameIdx(), RISCVStackID::SGPRSpill);
      // FIXME: Right now, no vgpr callee saved register, maybe later needed
      TII.storeRegToStackSlot(MBB, MI, Reg, !MBB.isLiveIn(Reg), CS.getFrameIdx(),
                            RC, TRI);
    }
    // else {
      // FIXME: Right now, no callee saved register for VGPR
      // MF->getFrameInfo().setStackID(CS.getFrameIdx(), RISCVStackID::VGPRSpill);
    // }
  }

  return true;
}

bool RISCVFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  // Manually restore values not restored by libcall.
  // Keep the same order as in the prologue. There is no need to reverse the
  // order in the epilogue. In addition, the return address will be restored
  // first in the epilogue. It increases the opportunity to avoid the
  // load-to-use data hazard between loading RA and return by RA.
  // loadRegFromStackSlot can insert multiple instructions.
  const auto &NonLibcallCSI = getNonLibcallCSI(*MF, CSI);
  for (auto &CS : NonLibcallCSI) {
    Register Reg = CS.getReg();
    const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
    if(Reg.id() < RISCV::V0 || Reg.id() > RISCV::V255 )
        TII.loadRegFromStackSlot(MBB, MI, Reg, CS.getFrameIdx(), RC, TRI);
    assert(MI != MBB.begin() && "loadRegFromStackSlot didn't insert any code!");
  }

  const char *RestoreLibCall = getRestoreLibCallName(*MF, CSI);
  if (RestoreLibCall) {
    // Add restore libcall via tail call.
    MachineBasicBlock::iterator NewMI =
        BuildMI(MBB, MI, DL, TII.get(RISCV::PseudoTAIL))
            .addExternalSymbol(RestoreLibCall, RISCVII::MO_CALL)
            .setMIFlag(MachineInstr::FrameDestroy);

    // Remove trailing returns, since the terminator is now a tail call to the
    // restore function.
    if (MI != MBB.end() && MI->getOpcode() == RISCV::PseudoRET) {
      NewMI->copyImplicitOps(*MF, *MI);
      MI->eraseFromParent();
    }
  }

  return true;
}

bool RISCVFrameLowering::enableShrinkWrapping(const MachineFunction &MF) const {
  // Keep the conventional code flow when not optimizing.
  if (MF.getFunction().hasOptNone())
    return false;

  return true;
}

bool RISCVFrameLowering::canUseAsPrologue(const MachineBasicBlock &MBB) const {
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const MachineFunction *MF = MBB.getParent();
  const auto *RVFI = MF->getInfo<RISCVMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Inserting a call to a __riscv_save libcall requires the use of the register
  // t0 (X5) to hold the return address. Therefore if this register is already
  // used we can't insert the call.

  RegScavenger RS;
  RS.enterBasicBlock(*TmpMBB);
  return !RS.isRegUsed(RISCV::X5);
}

bool RISCVFrameLowering::canUseAsEpilogue(const MachineBasicBlock &MBB) const {
  const MachineFunction *MF = MBB.getParent();
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const auto *RVFI = MF->getInfo<RISCVMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Using the __riscv_restore libcalls to restore CSRs requires a tail call.
  // This means if we still need to continue executing code within this function
  // the restore cannot take place in this basic block.

  if (MBB.succ_size() > 1)
    return false;

  MachineBasicBlock *SuccMBB =
      MBB.succ_empty() ? TmpMBB->getFallThrough() : *MBB.succ_begin();

  // Doing a tail call should be safe if there are no successors, because either
  // we have a returning block or the end of the block is unreachable, so the
  // restore will be eliminated regardless.
  if (!SuccMBB)
    return true;

  // The successor can only contain a return, since we would effectively be
  // replacing the successor with our own tail return at the end of our block.
  return SuccMBB->isReturnBlock() && SuccMBB->size() == 1;
}

bool RISCVFrameLowering::isSupportedStackID(TargetStackID::Value ID) const {
  RISCVStackID::Value StackID = (RISCVStackID::Value)ID;
  switch (StackID) {
  case RISCVStackID::Default:
  case RISCVStackID::SGPRSpill:
  case RISCVStackID::VGPRSpill:
    return true;
  case RISCVStackID::ScalableVector:
  case RISCVStackID::NoAlloc:
  case RISCVStackID::WasmLocal:
    return false;
  }
  llvm_unreachable("Invalid RISCVStackID::Value");
}

/// TODO: Implements this interface
bool RISCVFrameLowering::storeRegToReg(const TargetRegisterInfo *TRI) const {

  return false;
}

/// TODO: Implements this interface
bool RISCVFrameLowering::loadRegFromReg(const TargetRegisterInfo *TRI) const {

  return false;
}