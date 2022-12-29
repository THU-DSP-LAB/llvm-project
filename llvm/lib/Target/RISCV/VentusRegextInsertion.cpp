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

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

using namespace llvm;

#define VENTUS_REGEXT_INSERTION_NAME "Ventus regext instruction insertion pass"

namespace {

class VentusRegextInsertion : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  VentusRegextInsertion() : MachineFunctionPass(ID)   {
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
  bool Modified = false;

  // FIXME: As this expansion pass will break def-use chain, it can not pass
  // the MachineVerifierPass.
  if (getCGPassBuilderOption().VerifyMachineCode)
    return Modified;

  for (auto &MBB : MF)
    Modified |= runOnMachineBasicBlock(MBB);
  return Modified;
}

bool VentusRegextInsertion::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for(auto &MI : MBB) {
    Modified |= insertRegext(MBB, MI);
  }
  return Modified;
}

bool VentusRegextInsertion::insertRegext(MachineBasicBlock &MBB,
                                         MachineInstr &MI) {
  bool hasOverflow = false;

  // 3 bits encoding for each rd, rs1, rs2, rs3, total 12 bits.
  // Each 3 bit can encode 0~7 which stands for base register offset 0~7 * 32.
  unsigned Offsets = 0;

  for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
    unsigned RegIdx = 0;
    MachineOperand &Op = MI.getOperand(i);
    if (!Op.isReg())
      continue;

    unsigned RegId = Op.getReg().id();
    // FIXME: The ordering of rd, rs1, rs2, rs3 may not be the same as
    // operand index 0, 1, 2, 3!
    // TODO: Simplify this!
    if (RISCV::GPRRegClass.contains(Op.getReg()) && RegId > RISCV::X31) {
      // Encode overflowed GPR register numbering
      Offsets |= (RegId - RISCV::X31 + 31) / 32 << (RegIdx * 3);
      unsigned NewReg = RISCV::X0 + ((RegId - RISCV::X31) % 32);
      Op.setRegIgnoreDUChain(Register(NewReg));
      hasOverflow = true;
    } else if (RISCV::VGPRRegClass.contains(Op.getReg()) &&
               RegId > RISCV::V31) {
      Offsets |= (RegId - RISCV::V31 + 31) / 32 << (RegIdx * 3);
      unsigned NewReg = RISCV::V0 + ((RegId - RISCV::V31) % 32);
      Op.setRegIgnoreDUChain(Register(NewReg));
      hasOverflow = true;
    }

    RegIdx++;
  }

  if (hasOverflow) {
    DebugLoc DL = MI.getDebugLoc();
    // Create instruction to expand register basic offset as imm * 32
    BuildMI(MBB, &MI, DL, TII->get(RISCV::REGEXT))
      .addReg(RISCV::X0).addReg(RISCV::X0).addImm(Offsets);
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