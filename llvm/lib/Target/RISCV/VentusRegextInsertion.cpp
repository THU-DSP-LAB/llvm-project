//===-- VentusRegextInsertion.cpp - Insert regext instruction -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that insert "regext" instruction immediately before
// any instruction whose register number is greater than 31(due RISC-V
// instruction register number encoding limit). This pass should be run just
// before machine code emission.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

using namespace llvm;

#define VENTUS_REGEXT_INSERTION_NAME "Ventus regext instruction insertion pass"

namespace {

class VentusRegextInsertion : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  const RISCVRegisterInfo *TRI;
  static char ID;

  VentusRegextInsertion() : MachineFunctionPass(ID) {
    initializeVentusRegextInsertionPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return VENTUS_REGEXT_INSERTION_NAME;
  }

private:
  bool runOnMachineBasicBlock(MachineBasicBlock &MBB);

  /// Insert "regext" instruction immediately before given MI.
  bool insertRegext(MachineBasicBlock &MBB, MachineInstr &MI);
};

char VentusRegextInsertion::ID = 0;

bool VentusRegextInsertion::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  TRI = static_cast<const RISCVRegisterInfo *>(
      MF.getSubtarget().getRegisterInfo());
  bool Modified = false;

  for (auto &MBB : MF)
    Modified |= runOnMachineBasicBlock(MBB);
  return Modified;
}

bool VentusRegextInsertion::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (auto &MI : MBB) {
    // Skip KILL instruction
    if (MI.getOpcode() == TargetOpcode::KILL)
      continue;

    Modified |= insertRegext(MBB, MI);
  }
  return Modified;
}

bool VentusRegextInsertion::insertRegext(MachineBasicBlock &MBB,
                                         MachineInstr &MI) {
  bool hasOverflow = false;

  if (MI.isPseudo() && RISCVII::isVOPIMM11(MI.getDesc().TSFlags))
    return false;

  // 3 bits encoding for each rd, rs1, rs2, rs3, total 12 bits.
  // Each 3 bit can encode 0~7 which stands for base register offset 0~7 * 32.
  unsigned Offsets = 0;

  for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
    MachineOperand &Op = MI.getOperand(i);
    if (!Op.isReg() ||
        MI.getDesc().getOperandConstraint(i, MCOI::TIED_TO) != -1 ||
        MI.isDebugInstr())
      continue;

    uint16_t RegEncodingValue = TRI->getEncodingValue(Op.getReg());
    if (RegEncodingValue > 31) {
      hasOverflow = true;
      // FIXME: Support assembler/disassembler
      int Pos = MI.getDesc().getOperandConstraint(i, MCOI::CUSTOM);
      assert(Pos != -1 && "Out of range[0, 31] register operand custom "
                          "constraint that must be present.");
      Offsets |= (RegEncodingValue >> 5 & 0x7) << (3 * Pos);
    }
  }

  if (hasOverflow) {
    DebugLoc DL = MI.getDebugLoc();
    // Create instruction to expand register basic offset as imm * 32
    BuildMI(MBB, &MI, DL, TII->get(RISCV::REGEXT), RISCV::X0)
        .addReg(RISCV::X0)
        .addImm(Offsets);
  }

  return hasOverflow;
}

} // end of anonymous namespace

INITIALIZE_PASS(VentusRegextInsertion, "ventus-regext-insertion",
                VENTUS_REGEXT_INSERTION_NAME, false, false)

namespace llvm {
FunctionPass *createVentusRegextInsertionPass() {
  return new VentusRegextInsertion();
}
} // end of namespace llvm
