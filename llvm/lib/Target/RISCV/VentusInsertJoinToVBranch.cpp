//===-- VentusInsertJoinToBranch.cpp - Insert join to VBranches -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// In Ventus, if VBranch instructions are generated, we need to insert join
// instructions in both `else` and `then` branch to tell hardware where these
// two branches need to join together
//
// we follow the following rules to insert join block and join instruction
//
// 1: Legalize all the return block
//    when there are one more return blocks in machine function, there must be
//    branches, we need to reduce return blocks number down to 1
// 1.1: If two return blocks have common nearest parent branch, this two blocks
//    need to be joined, and we add a hasBeenJoined marker for this parent
//    branch
// 1.2: after we complete 1.1 process, there maybe one more return blocks, we
//    need to further add join block, we recursively build dominator tree for
//    these return blocks, first we find the nearest common dominator branch for
//    two return blocks, and then get dominator tree path between dominator
//    and each return block, we need to check this path in which whether any
//    other branch blocks exists, ideally, the branch block in path should have
//    been joined and marked, if not, this path is illegal, these two block can
//    not be joined
//
// 2: Insert join instructions
// 2.1: we scan through the MachineBasic blocks and check what blocks to insert
//    join instruction, below MBB represents MachineBasic Block
// 2.2: The MBB must have one more predecessors and its nearest dominator must
//     be a VBranch
// 2.3: Then we analyze the the predecessor of MBB, if the predecessor
//    has single successor, we add a join instruction to the predecessor end,
//    other wise, we need to insert a join block between predecessor and MBB
//
// WRANING: Do not use -O(1|2|3) optimization option
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"
#include "llvm/CodeGen/CodeGenPassBuilder.h"
#include "llvm/CodeGen/MachineDominators.h"

#define VENTUS_INSERT_JOIN_TO_BRANCH "Insert join to VBranch"
#define DEBUG_TYPE "Insert_join_to_VBranch"

using namespace llvm;

namespace {

struct BranchInfo {
  bool isDivergentBranch = false; // MBB is divergent branch or not
  bool hasBeenJoined = false;     // MBB has been joined
};

class VentusInsertJoinToVBranch : public MachineFunctionPass {

public:
  const RISCVInstrInfo *TII;
  const RISCVFrameLowering *RFL;
  static char ID;
  MachineFunction *MachineFunc;
  SmallVector<MachineBasicBlock *, 8> ReturnBlocks;
  SmallDenseMap<MachineBasicBlock *, BranchInfo> BranchMBBInfo;
  MachineDominatorTree *MDT = new MachineDominatorTree();

  VentusInsertJoinToVBranch() : MachineFunctionPass(ID) {
    initializeVentusInsertJoinToVBranchPass(*PassRegistry::getPassRegistry());
  }

  ~VentusInsertJoinToVBranch() { delete MDT; }

  // Collect all the branch blocks information in function
  void collectBranchMBBInfo(MachineFunction &MF);

  bool insertJoinMBB(MachineBasicBlock &MBB1, MachineBasicBlock &MBB2);

  bool runOnMachineFunction(MachineFunction &MF) override;

  bool legalizeRetMBB(MachineBasicBlock &MBB);

  bool hasCommonBranchPredecessor(MachineBasicBlock &MBB1,
                                  MachineBasicBlock &MBB2);

  bool canJoinMBB(MachineBasicBlock &MBB1, MachineBasicBlock &MBB2);

  /// This function check two return blocks whether can join or not
  bool hasNoUnjoinedBranch(MachineBasicBlock *CurrMBB,
                           MachineBasicBlock *TargetMBB);

  /// Find all the branch predecessor no matter direct or indirect
  SmallVector<MachineBasicBlock *>
  findAllBranchPredecessor(MachineBasicBlock &MBB);

  /// Check MBB is divergent branch or not
  bool isDivergentBranchBlock(MachineBasicBlock &MBB) {
    if (MBB.empty())
      return false;

    const MachineInstr &MI = MBB.instr_back();
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

  /// Check MBB is common branch or not
  bool isCommonBranchBlock(MachineBasicBlock &MBB) {

    if (MBB.empty())
      return false;
    const MachineInstr &MI = MBB.instr_back();
    switch (MI.getOpcode()) {
    default:
      return false;
    case RISCV::BEQ:
    case RISCV::BNE:
    case RISCV::BLT:
    case RISCV::BGE:
    case RISCV::BLTU:
    case RISCV::BGEU:
      return true;
    }
  }

  bool analyzeRetMBB(MachineFunction &MF);

  /// Get return MBB count
  unsigned getReturnBlockCount(MachineFunction &MF);

  StringRef getPassName() const override {
    return VENTUS_INSERT_JOIN_TO_BRANCH;
  }
};

char VentusInsertJoinToVBranch::ID = 0;

void VentusInsertJoinToVBranch::collectBranchMBBInfo(MachineFunction &MF) {
  for (auto &MBB : MF) {
    if (isCommonBranchBlock(MBB))
      BranchMBBInfo[&MBB] = {false, false};

    else if (isDivergentBranchBlock(MBB))
      BranchMBBInfo[&MBB] = {true, false};
  }
}

unsigned VentusInsertJoinToVBranch::getReturnBlockCount(MachineFunction &MF) {
  // Clear return block before each analysis
  if (!ReturnBlocks.empty())
    ReturnBlocks.clear();
  for (auto &MBB : MF) {
    if (MBB.isReturnBlock())
      ReturnBlocks.push_back(&MBB);
  }
  return ReturnBlocks.size();
}

bool VentusInsertJoinToVBranch::insertJoinMBB(MachineBasicBlock &MBB1,
                                              MachineBasicBlock &MBB2) {
  if (MBB1.isReturnBlock() && MBB2.isReturnBlock()) {
    MachineBasicBlock *PseudoJoinMBB = MachineFunc->CreateMachineBasicBlock();
    BuildMI(*PseudoJoinMBB, PseudoJoinMBB->end(), DebugLoc(),
            TII->get(RISCV::PseudoRET));
    MachineFunc->push_back(PseudoJoinMBB);
    legalizeRetMBB(MBB1);
    legalizeRetMBB(MBB2);
    MBB1.addSuccessor(PseudoJoinMBB);
    MBB2.addSuccessor(PseudoJoinMBB);
    return true;
  }
  return false;
}

/// Check if two return blocks can join or not
bool VentusInsertJoinToVBranch::canJoinMBB(MachineBasicBlock &MBB1,
                                           MachineBasicBlock &MBB2) {
  auto DominatorBlock = MDT->findNearestCommonDominator(&MBB1, &MBB2);
  if (DominatorBlock) {
    if (!hasNoUnjoinedBranch(DominatorBlock, &MBB1) &&
        !hasNoUnjoinedBranch(DominatorBlock, &MBB2)) {
      BranchMBBInfo.find(DominatorBlock)->second.hasBeenJoined = true;
      return true;
    }
  }
  return false;
}

bool VentusInsertJoinToVBranch::runOnMachineFunction(MachineFunction &MF) {

  bool IsChanged = false;
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  RFL = static_cast<const RISCVFrameLowering *>(
      MF.getSubtarget().getFrameLowering());
  MachineFunc = &MF;
  collectBranchMBBInfo(MF);
  MDT->getBase().recalculate(*MachineFunc);

  // After this check, all return blocks are expected to be legal
  IsChanged |= analyzeRetMBB(MF);
  MDT->getBase().recalculate(*MachineFunc);
  assert(getReturnBlockCount(MF) == 1 &&
         "Join return MBB process not completed");
  for (auto &MBB : make_early_inc_range(MF)) {
    MachineDomTreeNode *Node = MDT->getNode(&MBB);
    if (Node && Node->getIDom()) {
      // At least two predecessors
      unsigned PredecessorNum = std::distance(MBB.pred_begin(), MBB.pred_end());
      auto FoundBranchMBB = BranchMBBInfo.find(Node->getIDom()->getBlock());
      if (FoundBranchMBB != BranchMBBInfo.end() &&
          FoundBranchMBB->getSecond().isDivergentBranch && PredecessorNum > 1) {
        SmallVector<MachineBasicBlock *, 4> Predecessors;
        for (auto Pred : MBB.predecessors())
          Predecessors.push_back(Pred);
        for (auto Predecessor : make_early_inc_range(Predecessors)) {
          // Divergent branch, insert a block between MBB & predecessor
          if (isDivergentBranchBlock(*Predecessor)) {

            MachineBasicBlock *NewBB = MF.CreateMachineBasicBlock();
            // This is essential to keep CFG legal, if MBB is the fall through
            // block of predecessor, the NewBB should replace MBB's place
            // otherwise, we only need to insert before MBB
            if (Predecessor->getFallThrough() == &MBB)
              MF.insert(std::next(Predecessor->getIterator()), NewBB);
            else
              MF.insert(MBB.getIterator(), NewBB);
            Predecessor->replaceSuccessor(&MBB, NewBB);
            NewBB->addSuccessor(&MBB);

            BuildMI(*NewBB, NewBB->end(), DebugLoc(), TII->get(RISCV::JOIN))
                .addMBB(&MBB);
            MachineInstr *LastInst = &(*Predecessor->getFirstInstrTerminator());
            assert(LastInst->isBranch() && "Not branch instruction");
            LastInst->getOperand(2).setMBB(NewBB);

          } else {
            // Avoid duplicate JOIN add
            if (Predecessor->empty() ||
                !(Predecessor->instr_back().getOpcode() == RISCV::JOIN)) {
              if (!Predecessor->empty() &&
                  Predecessor->instr_back().getOpcode() == RISCV::PseudoBR)
                Predecessor->instr_back().eraseFromParent();
              BuildMI(*Predecessor, Predecessor->end(), DebugLoc(),
                      TII->get(RISCV::JOIN))
                  .addMBB(&MBB);
            }
          }
        }
      }
    }
  }
  return IsChanged;
}

bool VentusInsertJoinToVBranch::analyzeRetMBB(MachineFunction &MF) {
  bool IsChanged = false;
  // Check two MBBs' nearest parent branch MBB is the same or not, if is same
  // we need to join them to a maybe Joint block. otherwise
  unsigned ReturnBlockNum = getReturnBlockCount(MF);
  for (size_t i = 0; i < ReturnBlockNum; i++) {
    for (size_t j = i + 1; j < ReturnBlockNum; j++) {
      if (hasCommonBranchPredecessor(*ReturnBlocks[i], *ReturnBlocks[j]))
        IsChanged |= insertJoinMBB(*ReturnBlocks[i], *ReturnBlocks[j]);
    }
  }
  // Rebuild dominator tree
  MDT->getBase().recalculate(MF);
  unsigned RetNum = getReturnBlockCount(MF);
  while (true) {
    for (size_t i = 0; i < ReturnBlocks.size(); i++) {
      for (size_t j = i + 1; j < ReturnBlocks.size(); j++) {
        if (canJoinMBB(*ReturnBlocks[i], *ReturnBlocks[j]))
          IsChanged |= insertJoinMBB(*ReturnBlocks[i], *ReturnBlocks[j]);
      }
    }
    // After check, rebuild dominator tree
    MDT->getBase().recalculate(MF);
    unsigned RetNum1 = getReturnBlockCount(MF);
    if (RetNum1 == RetNum)
      // Avoid dead loop
      break;
    RetNum = RetNum1;
  }
  return IsChanged;
}

/// Legalize return block, right now, we only consider tail call && ret
bool VentusInsertJoinToVBranch::legalizeRetMBB(MachineBasicBlock &MBB) {
  // Get last instruction in this basic block
  assert((MBB.isReturnBlock() || MBB.empty()) && "Not legal block");
  if (MBB.empty())
    return false;
  MachineInstr &LastInst = MBB.back();
  unsigned LastInstOpcode = LastInst.getOpcode();
  assert((LastInstOpcode == RISCV::PseudoRET ||
          LastInstOpcode == RISCV::PseudoTAIL) &&
         "Unexpected opcode");
  if (LastInstOpcode == RISCV::PseudoRET)
    // Get the return instruction's implicit operands
    LastInst.eraseFromParent();
  else
    LastInst.setDesc(TII->get(RISCV::PseudoCALL));

  return true;
}

bool VentusInsertJoinToVBranch::hasCommonBranchPredecessor(
    MachineBasicBlock &MBB1, MachineBasicBlock &MBB2) {
  auto BranchPredecessor1 = findAllBranchPredecessor(MBB1);
  auto BranchPredecessor2 = findAllBranchPredecessor(MBB2);
  for (auto BranchPred1 : BranchPredecessor1) {
    auto FoundBranchPred1 = std::find(BranchPredecessor2.begin(),
                                      BranchPredecessor2.end(), BranchPred1);
    if (FoundBranchPred1 != BranchPredecessor2.end()) {
      auto FoundBranchMBB = BranchMBBInfo.find(*FoundBranchPred1);
      if (FoundBranchMBB != BranchMBBInfo.end())
        // Update BranchMBB's hasBeenJoined flag
        FoundBranchMBB->getSecond().hasBeenJoined = true;
      return true;
    }
  }
  return false;
}

SmallVector<MachineBasicBlock *>
VentusInsertJoinToVBranch::findAllBranchPredecessor(MachineBasicBlock &MBB) {
  SmallVector<MachineBasicBlock *, 6> BranchParents;

  for (auto Pred : MBB.predecessors()) {
    unsigned PredNum = std::distance(Pred->succ_begin(), Pred->succ_end());
    if (PredNum >= 2)
      BranchParents.push_back(Pred);
    else {
      auto Parents = findAllBranchPredecessor(*Pred);
      BranchParents.insert(BranchParents.end(), Parents.begin(), Parents.end());
    }
  }

  return BranchParents;
}

bool VentusInsertJoinToVBranch::hasNoUnjoinedBranch(
    MachineBasicBlock *DominatorMBB, MachineBasicBlock *TargetMBB) {
  // Find the path between MBB1 and its immediate dominator
  MachineDomTreeNode *TargetMBBNode = MDT->getNode(TargetMBB);
  SmallVector<MachineBasicBlock *, 4> DominatorMBBs;
  // Build path between dominator DominatorMBB and TargetMBB
  // FIXME: Maybe can simplify below codes
  while (TargetMBBNode->getIDom()->getBlock() != DominatorMBB) {
    DominatorMBBs.push_back(TargetMBBNode->getIDom()->getBlock());
    TargetMBBNode = TargetMBBNode->getIDom();
  }
  // Traverse this path, if found unjoined branch, return true
  for (auto DominatorMBB : DominatorMBBs) {
    auto Dominator = BranchMBBInfo.find(DominatorMBB);
    if (Dominator != BranchMBBInfo.end()) {
      if (!Dominator->getSecond().hasBeenJoined)
        return true;
    }
  }
  return false;
}

} // end of anonymous namespace

INITIALIZE_PASS(VentusInsertJoinToVBranch, "Insert-join-to-VBranch",
                VENTUS_INSERT_JOIN_TO_BRANCH, false, false)

namespace llvm {
FunctionPass *createVentusInsertJoinToVBranchPass() {
  return new VentusInsertJoinToVBranch();
}
} // end of namespace llvm
