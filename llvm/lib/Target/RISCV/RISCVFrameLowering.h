//===-- RISCVFrameLowering.h - Define frame lowering for RISCV -*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class implements RISCV-specific bits of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVFRAMELOWERING_H
#define LLVM_LIB_TARGET_RISCV_RISCVFRAMELOWERING_H

#include "RISCV.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Support/TypeSize.h"

namespace llvm {
class RISCVSubtarget;

class RISCVFrameLowering : public TargetFrameLowering {
public:
  explicit RISCVFrameLowering(const RISCVSubtarget &STI)
      : TargetFrameLowering(StackGrowsUp,
                            /*StackAlignment=*/Align(16),
                            /*LocalAreaOffset=*/0,
                            /*TransientStackAlignment=*/Align(16)),
        STI(STI) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI,
                                     Register &FrameReg) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS) const override;

  bool hasFP(const MachineFunction &MF) const override;

  bool hasBP(const MachineFunction &MF) const;

  bool hasReservedCallFrame(const MachineFunction &MF) const override;
  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;
  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;

  bool storeRegToReg(const TargetRegisterInfo *TRI) const override;

  bool loadRegFromReg(const TargetRegisterInfo *TRI) const override;
  // Get the first stack adjustment amount for SplitSPAdjust.
  // Return 0 if we don't want to to split the SP adjustment in prologue and
  // epilogue.
  uint64_t getFirstSPAdjustAmount(const MachineFunction &MF) const;

  bool canUseAsPrologue(const MachineBasicBlock &MBB) const override;
  bool canUseAsEpilogue(const MachineBasicBlock &MBB) const override;

  /// Get stack size for different stack ID
  uint64_t getStackSize(const MachineFunction &MF, RISCVStackID::Value ID) const;


  /// Calculate frame object's stack offset
  /// Frame Objects:
  /// fi#0: id=4 size=48, align=4, at location [SP+8]
  /// fi#1: id=1 size=4, align=4, at location [SP+4] \
  /// fi#2: id=1 size=4, align=4, at location [SP] \
  /// fi#3: id=4 size=4, align=4, at location [SP+16] \
  /// As we can see, if we split the stack, different frame offset calculation
  /// need to be modified too, when calculate the TP stack offset, we need to
  /// extract the stack offset of 'SP' in machine function frame
  uint64_t getStackOffset(const MachineFunction &MF, int FI,
                          RISCVStackID::Value Stack) const;

  /// Before insert prolog/epilog information, set stack ID for each frame index
  void determineStackID(MachineFunction &MF) const;

  bool enableShrinkWrapping(const MachineFunction &MF) const override;

  bool isSupportedStackID(TargetStackID::Value ID) const override;

protected:
  const RISCVSubtarget &STI;

private:
  void determineFrameLayout(MachineFunction &MF) const;
};
} // namespace llvm
#endif
