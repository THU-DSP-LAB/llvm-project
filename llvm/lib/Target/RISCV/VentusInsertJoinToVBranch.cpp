//===-- VentusInsertJoinToBranch.cpp - Insert join to VBranches -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// In ventus, if VBranch instructions are generated, we need to insert setrpc
// and join instructions to tell hardware where branches need to join
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/MC/MCContext.h"

#define VENTUS_INSERT_JOIN_TO_BRANCH "Insert join to VBranch"
#define DEBUG_TYPE "Insert_join_to_VBranch"

using namespace llvm;

namespace {

class VentusInsertJoinToVBranch : public MachineFunctionPass {

public:
  const RISCVInstrInfo *TII;
  static char ID;
  MachinePostDominatorTree *MPDT;

  VentusInsertJoinToVBranch()
      : MachineFunctionPass(ID), TII(nullptr), MPDT(nullptr) {
    initializeVentusInsertJoinToVBranchPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachinePostDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool checkJoinMBB(MachineBasicBlock &MBB) const;

  MachineInstr *getDivergentBranchInstr(MachineBasicBlock &MBB);

  bool convergeReturnBlock(MachineFunction &MF);

  StringRef getPassName() const override {
    return VENTUS_INSERT_JOIN_TO_BRANCH;
  }
};

char VentusInsertJoinToVBranch::ID = 0;

bool VentusInsertJoinToVBranch::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  MPDT = &getAnalysis<MachinePostDominatorTree>();

  // After this, all return blocks are expected to be legal
  bool IsChanged = convergeReturnBlock(MF);
  if (IsChanged)
    MPDT->getBase().recalculate(MF);

#ifndef NDEBUG
  unsigned NumberRetBB = 0;
  for (auto &BB : MF)
    if (BB.isReturnBlock())
      NumberRetBB++;
  assert(NumberRetBB == 1 && "Converge return MBB process not completed");
#endif

  DenseSet<MachineBasicBlock *> JoinedBB;

  for (auto &MBB : MF) {
    if (auto *VBranch = getDivergentBranchInstr(MBB)) {
      auto *PostIDomBB = MPDT->getNode(&MBB)->getIDom()->getBlock();
      assert(PostIDomBB);

      PostIDomBB->setLabelMustBeEmitted();

      MCSymbol *AUIPCSymbol = MF.getContext().createNamedTempSymbol("pcrel_hi");
      MachineInstr *MIAUIPC = BuildMI(MBB, VBranch->getIterator(), DebugLoc(),
                                      TII->get(RISCV::AUIPC), RISCV::X6)
                                  .addMBB(PostIDomBB, RISCVII::MO_PCREL_HI);
      MIAUIPC->setPreInstrSymbol(MF, AUIPCSymbol);
      BuildMI(MBB, VBranch->getIterator(), DebugLoc(), TII->get(RISCV::SETRPC))
          .addReg(RISCV::X0, RegState::Define | RegState::Dead)
          .addReg(RISCV::X6)
          .addSym(AUIPCSymbol, RISCVII::MO_PCREL_LO);

      // FIXME: There is something wrong when add this operand.
      // VBranch->addOperand(MachineOperand::CreateReg(
      //     RISCV::RPC, false /* isDef */, true /* isImp */));
      if (!JoinedBB.contains(PostIDomBB)) {
        IsChanged = true;
        JoinedBB.insert(PostIDomBB);
        // Insert join instruction after last vmv.v instruction
        BuildMI(*PostIDomBB, PostIDomBB->begin(), DebugLoc(),
                TII->get(RISCV::JOIN))
            .addReg(RISCV::X0)
            .addReg(RISCV::X0)
            .addImm(0);
        IsChanged |= checkJoinMBB(*PostIDomBB);
      }
    }
  }
  return IsChanged;
}

static bool isDivergentBranch(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    return false;
  case RISCV::VBEQ:
  case RISCV::VBNE:
  case RISCV::VBLT:
  case RISCV::VBGE:
  case RISCV::VBLTU:
  case RISCV::VBGEU:
    return true;
  }
}

MachineInstr *
VentusInsertJoinToVBranch::getDivergentBranchInstr(MachineBasicBlock &MBB) {
  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !TII->isUnpredicatedTerminator(*I))
    return nullptr;

  // Count the number of terminators.
  int NumTerminators = 0;
  for (auto J = I.getReverse();
       J != MBB.rend() && TII->isUnpredicatedTerminator(*J); J++)
    NumTerminators++;

  // Handle a single conditional branch.
  if (NumTerminators == 1 && isDivergentBranch(*I))
    return &(*I);

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && isDivergentBranch(*std::prev(I)) &&
      I->getDesc().isUnconditionalBranch())
    return &(*std::prev(I));

#ifndef NDEBUG
  for (auto J = I.getReverse();
       J != MBB.rend() && TII->isUnpredicatedTerminator(*J); J++)
    assert(!isDivergentBranch(*J) && "Unresolved divergent branch");
#endif

  // Otherwise, we can't handle this.
  return nullptr;
}

bool VentusInsertJoinToVBranch::convergeReturnBlock(MachineFunction &MF) {
  DenseSet<MachineBasicBlock *> ReturnBlocks;
  for (auto &BB : MF)
    if (BB.isReturnBlock())
      ReturnBlocks.insert(&BB);

  assert(!ReturnBlocks.empty() && "At least one return block");

  // No need to converge if there is one return block.
  if (ReturnBlocks.size() == 1)
    return false;

  auto *NewRetBB = MF.CreateMachineBasicBlock();
  BuildMI(NewRetBB, DebugLoc(), TII->get(RISCV::PseudoRET));
  MF.insert(MF.end(), NewRetBB);

  for (auto *RetBB : ReturnBlocks) {
    auto &RetMI = RetBB->back();
    assert(RetMI.getOpcode() == RISCV::PseudoRET && "Unexpected opcode");
    RetMI.eraseFromParent();
    if (RetBB->getFallThrough() != NewRetBB)
      BuildMI(RetBB, DebugLoc(), TII->get(RISCV::PseudoBR)).addMBB(NewRetBB);
    RetBB->addSuccessor(NewRetBB);
  }

  return true;
}

bool VentusInsertJoinToVBranch::checkJoinMBB(MachineBasicBlock &MBB) const {
  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MR = MBB.getParent()->getRegInfo();
  bool IsChanged = false;
  // When MBB has only one predecessor, return directly
  if (std::distance(MBB.pred_begin(), MBB.pred_end()) <= 1)
    return IsChanged;

  // For some instructions like vmv.v,  if the src register are defined in
  // all predecessors, then it should not appear after join point
  for (auto &MI : make_early_inc_range(MBB)) {
    // FIXME: Maybe vfmv.v.f instruction need to be checked too
    if (MI.getOpcode() != RISCV::VMV_V_X)
      continue;

    // The number of MBB needed to be inserted VMV instruction
    unsigned NeedToBeInsertMBBNum = 0;
    assert(MI.getOperand(1).isReg() && "unexpected operator");
    auto Defines = MR.def_instructions(MI.getOperand(1).getReg());
    bool IsInSameBlock = false;
    SmallVector<std::pair<MachineBasicBlock *, MachineBasicBlock::iterator>>
        MBBMaybeInsertedInstr;
    for (auto &Def : Defines) {
      unsigned Opcode = Def.getOpcode();
      // FIXME: Find a better way to handle this in tablegen
      if (Opcode == RISCV::JOIN || Opcode == RISCV::SETRPC ||
          Opcode == RISCV::REGEXT || Opcode == RISCV::REGEXTI)
        continue;

      // When define instruction is in the same MBB, no need to change position
      for (auto Iter = MBB.instr_begin(); Iter != MI.getIterator(); Iter++) {
        if (&Def.getDesc() == &Iter->getDesc()) {
          IsInSameBlock = true;
          break;
        }
      }

      if (IsInSameBlock)
        continue;

      // Check if define instruction is in the predecessors or not
      for (auto *Pre : MBB.predecessors()) {
        MachineBasicBlock::iterator Insert = Pre->begin();
        for (auto &MI1 : *Pre) {
          // Get last register definition
          if (&MI1 == &Def)
            Insert = MI1.getIterator();
        }
        if (Insert != Pre->begin()) {
          // Last instruction define in Pre MBB
          NeedToBeInsertMBBNum++;
          MBBMaybeInsertedInstr.push_back({Pre, Insert});
        }
      }
    }
    if (NeedToBeInsertMBBNum >= 2) {
      // Means there are more than two blocks need to insert vmv instruction
      IsChanged |= true;
      MBB.addLiveIn(MCRegister(MI.getOperand(0).getReg()));
      for (auto Pair : MBBMaybeInsertedInstr)
        Pair.first->insertAfter(Pair.second, MF->CloneMachineInstr(&MI));
      MI.eraseFromParent();
    }
  }
  return IsChanged;
}

} // end of anonymous namespace

INITIALIZE_PASS(VentusInsertJoinToVBranch, "Insert-join-to-VBranch",
                VENTUS_INSERT_JOIN_TO_BRANCH, false, false)

namespace llvm {
FunctionPass *createVentusInsertJoinToVBranchPass() {
  return new VentusInsertJoinToVBranch();
}
} // end of namespace llvm
