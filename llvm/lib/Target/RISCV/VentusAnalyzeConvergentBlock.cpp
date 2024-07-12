//===-- VentusAnalyzeConvergentBlock.cpp - Analyze data convergent block --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Analyze convergent basic block in control flow
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"

#define ANALYZE_CONVERGENT_BLOCK "Analyze convergent block"
#define DEBUG_TYPE "Convergent block analysis"

using namespace llvm;

namespace {

class AnalyzeConvergentBlock : public MachineFunctionPass {

public:
  const RISCVInstrInfo *TII;
  static char ID;

  AnalyzeConvergentBlock() : MachineFunctionPass(ID), TII(nullptr) {
    initializeAnalyzeConvergentBlockPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  // Basically, we need to analyze control flow mainly for if/else branch join
  // block
  bool splitJoinMBB(MachineBasicBlock &MBB) const;

  StringRef getPassName() const override { return ANALYZE_CONVERGENT_BLOCK; }
};

char AnalyzeConvergentBlock::ID = 0;

bool AnalyzeConvergentBlock::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool IsChanged = false;
  for (auto &MBB : make_early_inc_range(MF)) {
    // Just analyze join block
    if (MBB.pred_size() == 1)
      continue;
    IsChanged |= splitJoinMBB(MBB);
  }
  return IsChanged;
}

bool AnalyzeConvergentBlock::splitJoinMBB(MachineBasicBlock &MBB) const {
  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MR = MBB.getParent()->getRegInfo();
  // Find last vmv instruction in MBB
  // so we reverse the iteration
  MachineInstrBundleIterator<MachineInstr> VMVIns = nullptr;
  for (auto I = MBB.end(); I != MBB.front(); I--) {
    if (I->getOpcode() == RISCV::VMV_V_X) {
      VMVIns = I;
      break;
    }
  }
  // Split this block into two block
  bool IsChanged = false;

  return IsChanged;
}

} // end of anonymous namespace

INITIALIZE_PASS(AnalyzeConvergentBlock, "Analyze convergent block",
                ANALYZE_CONVERGENT_BLOCK, false, false)

namespace llvm {
FunctionPass *createAnalyzeConvergentBlockPass() {
  return new AnalyzeConvergentBlock();
}

} // end of namespace llvm
