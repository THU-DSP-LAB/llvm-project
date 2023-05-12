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
  static char ID;
  MachineFunction *MachineFunc;
  const RISCVRegisterInfo *MRI;
  const MachineRegisterInfo *MR;
  SmallVector<MachineBasicBlock *, 8> ReturnBlock;
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

  bool hasCommonNearestParentBranch(MachineBasicBlock &MBB1,
                                    MachineBasicBlock &MBB2);

  bool canJoinMBB(MachineBasicBlock &MBB1, MachineBasicBlock &MBB2);

  /// This function check two return blocks whether can join or not
  bool hasNoUnjoinedBranch(MachineBasicBlock *CurrMBB,
                           MachineBasicBlock *TargetMBB);

  /// Find all the branch predecessor no matter direct or indirect
  SmallVector<MachineBasicBlock *>
  findAllNearestParentBranches(MachineBasicBlock &MBB);

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
  // virtual void getAnalysisUsage(AnalysisUsage &AU) const override {
  //     AU.addRequired<BranchFolderPass>();
  //     AU.setPreservesAll();
  // }
  /// Legalize all the return MBB
  bool canJoinRetMBB(MachineFunction &MF);

  /// Get return MBB numbers
  unsigned getReturnBlockNum(MachineFunction &MF);

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

unsigned VentusInsertJoinToVBranch::getReturnBlockNum(MachineFunction &MF) {
  // Clear return block before each analysis
  if (!ReturnBlock.empty())
    ReturnBlock.clear();
  unsigned ReturnBlockNum = 0;
  for (auto &MBB : MF) {
    if (MBB.isReturnBlock()) {
      // Original return blocks
      ReturnBlock.push_back(&MBB);
      ReturnBlockNum++;
    }
  }
  return ReturnBlockNum;
}

bool VentusInsertJoinToVBranch::insertJoinMBB(MachineBasicBlock &MBB1,
                                              MachineBasicBlock &MBB2) {
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
  MRI = MF.getSubtarget<RISCVSubtarget>().getRegisterInfo();
  MR = &MF.getRegInfo();
  MachineFunc = &MF;
  collectBranchMBBInfo(MF);
  MDT->getBase().recalculate(*MachineFunc);

  // After this check, all return blocks are expected to be legal
  IsChanged |= canJoinRetMBB(MF);
  MDT->getBase().recalculate(*MachineFunc);
  // assert(getReturnBlockNum(MF) == 1 && "Join return MBB process not
  // completed");
  for (auto &MBB : make_early_inc_range(MF)) {
    MachineDomTreeNode *Node = MDT->getNode(&MBB);
    if (Node && Node->getIDom()) {
      // At least two predecessors
      unsigned PredecessorNum = std::distance(MBB.pred_begin(), MBB.pred_end());
      if (BranchMBBInfo.find(Node->getIDom()->getBlock()) !=
              BranchMBBInfo.end() &&
          BranchMBBInfo.find(Node->getIDom()->getBlock())
              ->getSecond()
              .isDivergentBranch &&
          PredecessorNum > 1) {
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
            if (!(Predecessor->instr_back().getOpcode() == RISCV::JOIN))
              BuildMI(*Predecessor, Predecessor->end(), DebugLoc(),
                      TII->get(RISCV::JOIN))
                  .addMBB(&MBB);
          }
        }
      }
    }
  }
  return IsChanged;
}

bool VentusInsertJoinToVBranch::canJoinRetMBB(MachineFunction &MF) {
  bool IsChanged = false;
  // Check two MBBs' nearest parent branch MBB is the same or not, if is same
  // we need to join them to a maybe Joint block. otherwise
  unsigned ReturnBlockNum = getReturnBlockNum(MF);
  for (size_t i = 0; i < ReturnBlockNum; i++) {
    for (size_t j = i + 1; j < ReturnBlockNum; j++) {
      if (hasCommonNearestParentBranch(*ReturnBlock[i], *ReturnBlock[j]))
        IsChanged |= insertJoinMBB(*ReturnBlock[i], *ReturnBlock[j]);
    }
  }
  // Rebuild dominator tree
  MDT->getBase().recalculate(MF);
  unsigned RetNum = getReturnBlockNum(MF);
  while (true) {
    for (size_t i = 0; i < ReturnBlock.size(); i++) {
      for (size_t j = i + 1; j < ReturnBlock.size(); j++) {
        if (canJoinMBB(*ReturnBlock[i], *ReturnBlock[j]))
          IsChanged |= insertJoinMBB(*ReturnBlock[i], *ReturnBlock[j]);
      }
    }
    // After check, rebuild dominator tree
    MDT->getBase().recalculate(MF);
    unsigned RetNum1 = getReturnBlockNum(MF);
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
  if (MBB.empty())
    return false;
  MachineInstr *LastInst = &(*MBB.instr_rbegin());
  unsigned LastInstOpcode = LastInst->getOpcode();
  assert(LastInstOpcode == RISCV::PseudoRET ||
         LastInstOpcode == RISCV::PseudoTAIL && "Unexpected opcode");
  // If opcode is RISCV::PseudoRET, keep a copy of this instruction
  if (LastInstOpcode == RISCV::PseudoRET)
    // Get the return instruction's implicit operands
    LastInst->eraseFromParent();

  else
    LastInst->setDesc(TII->get(RISCV::PseudoCALL));
  return true;
}

bool VentusInsertJoinToVBranch::hasCommonNearestParentBranch(
    MachineBasicBlock &MBB1, MachineBasicBlock &MBB2) {
  auto ParentBranches1 = findAllNearestParentBranches(MBB1);
  auto ParentBranches2 = findAllNearestParentBranches(MBB2);
  for (auto Branch : ParentBranches1) {
    if (std::find(ParentBranches2.begin(), ParentBranches2.end(), Branch) !=
        ParentBranches2.end()) {
      auto BranchMBB =
          std::find(ParentBranches2.begin(), ParentBranches2.end(), Branch);
      if (BranchMBBInfo.find(*BranchMBB) != BranchMBBInfo.end())
        // Update BranchMBB's hasBeenJoined flag
        BranchMBBInfo.find(*BranchMBB)->getSecond().hasBeenJoined = true;
      return true;
    }
  }
  return false;
}

SmallVector<MachineBasicBlock *>
VentusInsertJoinToVBranch::findAllNearestParentBranches(
    MachineBasicBlock &MBB) {
  SmallVector<MachineBasicBlock *, 8> BranchParents;

  for (auto Pred : MBB.predecessors()) {
    unsigned PredNum = std::distance(Pred->succ_begin(), Pred->succ_end());
    if (PredNum >= 2)
      BranchParents.push_back(Pred);
    else {
      auto Parents = findAllNearestParentBranches(*Pred);
      BranchParents.insert(BranchParents.end(), Parents.begin(), Parents.end());
    }
  }

  return BranchParents;
}

bool VentusInsertJoinToVBranch::hasNoUnjoinedBranch(
    MachineBasicBlock *DominatorMBB, MachineBasicBlock *TargetMBB) {
  // Find the path between MBB1 and its immediate dominator
  MachineDomTreeNode *TargetMBBNode = MDT->getNode(TargetMBB);
  SmallVector<MachineBasicBlock *, 4> Path;
  // Build path between dominator DominatorMBB and TargetMBB
  // FIXME: Maybe can simplify below codes
  while (TargetMBBNode && TargetMBBNode->getBlock() != DominatorMBB &&
         TargetMBBNode->getIDom()->getBlock() != DominatorMBB) {
    Path.push_back(TargetMBBNode->getBlock());
    TargetMBBNode = TargetMBBNode->getIDom();
  }
  // Traverse this path, if found unjoined branch, return true
  for (auto path : Path) {
    if (BranchMBBInfo.find(path) != BranchMBBInfo.end()) {
      if (!BranchMBBInfo.find(path)->getSecond().hasBeenJoined)
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
