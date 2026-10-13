//===-- VentusSGPRKeepAlive.cpp - Preserve SGPRs across divergence --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-sgpr-keepalive"

namespace {

static cl::opt<bool> UseLegacySGPRKeepAlive(
    "ventus-use-legacy-sgpr-keepalive", cl::Hidden, cl::init(false),
    cl::desc("Enable legacy Ventus SGPR keepalive insertion before register "
             "allocation"));

class VentusInsertSGPRKeepAlive : public MachineFunctionPass {
public:
  static char ID;

  VentusInsertSGPRKeepAlive() : MachineFunctionPass(ID) {
    initializeVentusInsertSGPRKeepAlivePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Ventus Insert SGPR KeepAlive"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    AU.addRequired<MachinePostDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineRegisterInfo *MRI = nullptr;
  const RISCVInstrInfo *TII = nullptr;
  MachineDominatorTree *MDT = nullptr;
  MachinePostDominatorTree *MPDT = nullptr;

  bool collectRegionRegs(MachineBasicBlock &BranchBB, MachineInstr &BranchMI,
                         MachineBasicBlock &JoinBB,
                         SmallSetVector<Register, 8> &Regs) const;
};

class VentusRemoveSGPRKeepAlive : public MachineFunctionPass {
public:
  static char ID;

  VentusRemoveSGPRKeepAlive() : MachineFunctionPass(ID) {
    initializeVentusRemoveSGPRKeepAlivePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Ventus Remove SGPR KeepAlive"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

struct BranchKeepAliveInfo {
  MachineBasicBlock *JoinBB;
  SmallSetVector<Register, 8> Regs;
  SmallVector<MachineBasicBlock *, 4> ExitPreds;
};

static bool isDivergentBranchOpcode(unsigned Opcode) {
  switch (Opcode) {
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

static MachineInstr *getDivergentBranchInstr(MachineBasicBlock &MBB,
                                             const RISCVInstrInfo &TII) {
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !TII.isUnpredicatedTerminator(*I))
    return nullptr;

  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && TII.isUnpredicatedTerminator(*J);
       ++J)
    ++NumTerminators;

  if (NumTerminators == 1 && isDivergentBranchOpcode(I->getOpcode()))
    return &*I;

  if (NumTerminators == 2 && isDivergentBranchOpcode(std::prev(I)->getOpcode()) &&
      I->getDesc().isUnconditionalBranch())
    return &*std::prev(I);

#ifndef NDEBUG
  for (auto J = I.getReverse(); J != MBB.rend() && TII.isUnpredicatedTerminator(*J);
       ++J)
    assert(!isDivergentBranchOpcode(J->getOpcode()) && "Unresolved divergent branch");
#endif

  return nullptr;
}

static bool hasDivergentBranch(MachineFunction &MF, const RISCVInstrInfo &TII) {
  for (MachineBasicBlock &MBB : MF)
    if (getDivergentBranchInstr(MBB, TII))
      return true;
  return false;
}

static bool convergeReturnBlock(MachineFunction &MF, const RISCVInstrInfo &TII) {
  DenseSet<MachineBasicBlock *> ReturnBlocks;
  for (auto &BB : MF)
    if (BB.isReturnBlock())
      ReturnBlocks.insert(&BB);

  assert(!ReturnBlocks.empty() && "At least one return block");
  if (ReturnBlocks.size() == 1)
    return false;

  auto *NewRetBB = MF.CreateMachineBasicBlock();
  BuildMI(NewRetBB, DebugLoc(), TII.get(RISCV::PseudoRET));
  MF.insert(MF.end(), NewRetBB);

  for (auto *RetBB : ReturnBlocks) {
    MachineInstr &RetMI = RetBB->back();
    assert(RetMI.getOpcode() == RISCV::PseudoRET && "Unexpected opcode");
    RetMI.eraseFromParent();
    if (RetBB->getFallThrough() != NewRetBB)
      BuildMI(RetBB, DebugLoc(), TII.get(RISCV::PseudoBR)).addMBB(NewRetBB);
    RetBB->addSuccessor(NewRetBB);
  }

  return true;
}

static SmallVector<MachineBasicBlock *, 8>
collectRegionBlocks(MachineBasicBlock &BranchBB, MachineBasicBlock &JoinBB) {
  SmallVector<MachineBasicBlock *, 8> Worklist;
  SmallVector<MachineBasicBlock *, 8> RegionBlocks;
  DenseSet<MachineBasicBlock *> Seen;
  Seen.insert(&BranchBB);
  Seen.insert(&JoinBB);

  for (MachineBasicBlock *Succ : BranchBB.successors()) {
    if (!Seen.insert(Succ).second)
      continue;
    Worklist.push_back(Succ);
  }

  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    RegionBlocks.push_back(MBB);
    for (MachineBasicBlock *Succ : MBB->successors()) {
      if (!Seen.insert(Succ).second)
        continue;
      Worklist.push_back(Succ);
    }
  }

  return RegionBlocks;
}

static bool isScalarVirtualRegister(Register Reg, const MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return false;
  return RISCVRegisterInfo::isSGPRClass(MRI.getRegClass(Reg));
}

static bool isBeforeInBlock(const MachineInstr &DefMI, const MachineInstr &UseMI) {
  const MachineBasicBlock &MBB = *DefMI.getParent();
  for (const MachineInstr &MI : MBB) {
    if (&MI == &DefMI)
      return true;
    if (&MI == &UseMI)
      return false;
  }
  return false;
}

static bool isDefinedBeforeBranch(Register Reg, const MachineRegisterInfo &MRI,
                                  const DenseSet<MachineBasicBlock *> &RegionBlocks,
                                  MachineBasicBlock &BranchBB,
                                  MachineInstr &BranchMI) {
  MachineInstr *DefMI = MRI.getVRegDef(Reg);
  if (!DefMI)
    return false;

  MachineBasicBlock *DefBB = DefMI->getParent();
  if (DefBB != &BranchBB)
    return !RegionBlocks.contains(DefBB);

  return isBeforeInBlock(*DefMI, BranchMI);
}

static bool dominatesBlockEnd(Register Reg, const MachineRegisterInfo &MRI,
                              const MachineDominatorTree &MDT,
                              MachineBasicBlock &UseBB) {
  MachineInstr *DefMI = MRI.getVRegDef(Reg);
  if (!DefMI)
    return false;
  return MDT.dominates(DefMI->getParent(), &UseBB);
}

static bool dominatesJoin(Register Reg, const MachineRegisterInfo &MRI,
                          const MachineDominatorTree &MDT,
                          MachineBasicBlock &JoinBB) {
  return dominatesBlockEnd(Reg, MRI, MDT, JoinBB);
}

static MachineBasicBlock *
getImmediatePostDominatorOrReport(MachineFunction &MF, MachineBasicBlock &MBB,
                                  MachinePostDominatorTree &MPDT,
                                  StringRef PassName) {
  auto *Node = MPDT.getNode(&MBB);
  auto *IDom = Node ? Node->getIDom() : nullptr;
  MachineBasicBlock *JoinBB = IDom ? IDom->getBlock() : nullptr;
  if (JoinBB)
    return JoinBB;

  report_fatal_error(Twine(PassName) +
                     " requires every divergent branch to converge; missing "
                     "immediate post-dominator in function '" +
                     MF.getName() + "', machine block #" +
                     Twine(MBB.getNumber()));
}

static bool isJoinPhiIncomingFromRegion(
    const MachineOperand &PredMO, MachineBasicBlock &BranchBB,
    const DenseSet<MachineBasicBlock *> &RegionBlocks) {
  if (!PredMO.isMBB())
    return false;

  MachineBasicBlock *PredBB = PredMO.getMBB();
  return PredBB == &BranchBB || RegionBlocks.contains(PredBB);
}

static bool hasSuccessor(MachineBasicBlock &MBB, MachineBasicBlock &SuccBB) {
  for (MachineBasicBlock *Succ : MBB.successors())
    if (Succ == &SuccBB)
      return true;
  return false;
}

static SmallVector<MachineBasicBlock *, 4>
collectJoinExitPreds(MachineBasicBlock &BranchBB,
                     ArrayRef<MachineBasicBlock *> RegionBlocks,
                     MachineBasicBlock &JoinBB) {
  SmallVector<MachineBasicBlock *, 4> ExitPreds;
  DenseSet<MachineBasicBlock *> Seen;

  auto MaybeAddExitPred = [&](MachineBasicBlock *MBB) {
    if (Seen.insert(MBB).second && hasSuccessor(*MBB, JoinBB))
      ExitPreds.push_back(MBB);
  };

  MaybeAddExitPred(&BranchBB);
  for (MachineBasicBlock *MBB : RegionBlocks)
    MaybeAddExitPred(MBB);

  return ExitPreds;
}

static bool isRemovableKeepAliveEdgeBlock(
    const MachineBasicBlock &MBB,
    const DenseSet<const MachineBasicBlock *> &MarkedKeepAliveBlocks) {
  if (MBB.isEHPad() || MBB.isInlineAsmBrIndirectTarget() || MBB.isReturnBlock())
    return false;
  if (MBB.pred_size() != 1 || MBB.succ_size() != 1)
    return false;
  if (!MarkedKeepAliveBlocks.contains(&MBB))
    return false;

  MachineBasicBlock *Succ = *MBB.succ_begin();
  for (const MachineInstr &MI : MBB) {
    if (MI.isDebugInstr())
      continue;
    if (MI.getOpcode() == RISCV::PseudoBR && MI.getOperand(0).getMBB() == Succ)
      continue;
    return false;
  }

  return true;
}

static void removeKeepAliveEdgeBlock(MachineBasicBlock &MBB) {
  MachineBasicBlock *Pred = *MBB.pred_begin();
  MachineBasicBlock *Succ = *MBB.succ_begin();

  Pred->ReplaceUsesOfBlockWith(&MBB, Succ);
  Succ->replacePhiUsesWith(&MBB, Pred);
  MBB.removeSuccessor(Succ);
  MBB.eraseFromParent();
  Pred->updateTerminator(Succ);
}

static void collectJoinPhiRegs(MachineBasicBlock &BranchBB, MachineInstr &BranchMI,
                               MachineBasicBlock &JoinBB,
                               const DenseSet<MachineBasicBlock *> &RegionBlocks,
                               const MachineRegisterInfo &MRI,
                               SmallSetVector<Register, 8> &Regs) {
  // PHI incoming values are edge uses, so join-only consumers would be missed
  // by the region body scan unless we inspect the join block explicitly.
  for (MachineInstr &MI : JoinBB) {
    if (!MI.isPHI())
      break;

    for (unsigned I = 1, E = MI.getNumOperands(); I + 1 < E; I += 2) {
      const MachineOperand &RegMO = MI.getOperand(I);
      const MachineOperand &PredMO = MI.getOperand(I + 1);
      if (!RegMO.isReg() || !isJoinPhiIncomingFromRegion(PredMO, BranchBB, RegionBlocks))
        continue;

      Register Reg = RegMO.getReg();
      if (!isScalarVirtualRegister(Reg, MRI))
        continue;

      if (!isDefinedBeforeBranch(Reg, MRI, RegionBlocks, BranchBB, BranchMI))
        continue;

      Regs.insert(Reg);
    }
  }
}

bool VentusInsertSGPRKeepAlive::collectRegionRegs(
    MachineBasicBlock &BranchBB, MachineInstr &BranchMI, MachineBasicBlock &JoinBB,
    SmallSetVector<Register, 8> &Regs) const {
  SmallVector<MachineBasicBlock *, 8> RegionBlocks =
      collectRegionBlocks(BranchBB, JoinBB);

  DenseSet<MachineBasicBlock *> RegionBlockSet;
  for (MachineBasicBlock *MBB : RegionBlocks)
    RegionBlockSet.insert(MBB);

  for (MachineBasicBlock *MBB : RegionBlocks) {
    for (MachineInstr &MI : *MBB) {
      if (MI.isDebugInstr())
        continue;

      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse())
          continue;

        Register Reg = MO.getReg();
        if (!isScalarVirtualRegister(Reg, *MRI))
          continue;

        if (!isDefinedBeforeBranch(Reg, *MRI, RegionBlockSet, BranchBB, BranchMI))
          continue;

        Regs.insert(Reg);
      }
    }
  }

  collectJoinPhiRegs(BranchBB, BranchMI, JoinBB, RegionBlockSet, *MRI, Regs);

  return !Regs.empty();
}

bool VentusInsertSGPRKeepAlive::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.isVentusGPGPU())
    return false;

  MRI = &MF.getRegInfo();
  TII = ST.getInstrInfo();
  MDT = &getAnalysis<MachineDominatorTree>();
  MPDT = &getAnalysis<MachinePostDominatorTree>();

  if (!hasDivergentBranch(MF, *TII))
    return false;

  bool Changed = convergeReturnBlock(MF, *TII);
  if (Changed) {
    MDT->getBase().recalculate(MF);
    MPDT->getBase().recalculate(MF);
  }

  if (!UseLegacySGPRKeepAlive)
    return Changed;

  SmallVector<BranchKeepAliveInfo, 8> BranchInfos;
  for (MachineBasicBlock &MBB : MF) {
    MachineInstr *BranchMI = getDivergentBranchInstr(MBB, *TII);
    if (!BranchMI)
      continue;

    MachineBasicBlock *JoinBB = getImmediatePostDominatorOrReport(
        MF, MBB, *MPDT, "Ventus Insert SGPR KeepAlive");

    BranchKeepAliveInfo Info{JoinBB, {}, {}};
    if (!collectRegionRegs(MBB, *BranchMI, *JoinBB, Info.Regs))
      continue;

    SmallVector<MachineBasicBlock *, 8> RegionBlocks =
        collectRegionBlocks(MBB, *JoinBB);
    Info.ExitPreds = collectJoinExitPreds(MBB, RegionBlocks, *JoinBB);
    BranchInfos.push_back(std::move(Info));
  }

  MapVector<MachineBasicBlock *, SmallSetVector<Register, 8>> JoinToRegs;
  DenseMap<MachineBasicBlock *, DenseMap<MachineBasicBlock *, MachineBasicBlock *>>
      EdgeToKeepAliveBlock;
  MapVector<MachineBasicBlock *, SmallSetVector<Register, 8>> BlockToRegs;

  auto GetOrCreateKeepAliveBlock = [&](MachineBasicBlock *PredBB,
                                       MachineBasicBlock *JoinBB) {
    MachineBasicBlock *&KeepAliveBB = EdgeToKeepAliveBlock[PredBB][JoinBB];
    if (KeepAliveBB)
      return KeepAliveBB;

    KeepAliveBB = PredBB->SplitCriticalEdge(JoinBB, *this);
    assert(KeepAliveBB && "Expected keepalive edge split to succeed");
    BuildMI(*KeepAliveBB, KeepAliveBB->begin(), DebugLoc(),
            TII->get(RISCV::PseudoSGPRKeepAliveBlock));
    Changed = true;
    return KeepAliveBB;
  };

  for (BranchKeepAliveInfo &Info : BranchInfos) {
    for (Register Reg : Info.Regs) {
      if (dominatesJoin(Reg, *MRI, *MDT, *Info.JoinBB)) {
        JoinToRegs[Info.JoinBB].insert(Reg);
        continue;
      }

      for (MachineBasicBlock *ExitPred : Info.ExitPreds) {
        if (!dominatesBlockEnd(Reg, *MRI, *MDT, *ExitPred))
          continue;
        MachineBasicBlock *KeepAliveBB =
            GetOrCreateKeepAliveBlock(ExitPred, Info.JoinBB);
        BlockToRegs[KeepAliveBB].insert(Reg);
      }
    }
  }

  for (auto &Entry : JoinToRegs) {
    MachineBasicBlock *JoinBB = Entry.first;
    auto InsertIt = JoinBB->getFirstNonPHI();
    for (Register Reg : Entry.second) {
      BuildMI(*JoinBB, InsertIt, DebugLoc(), TII->get(RISCV::PseudoSGPRKeepAlive))
          .addReg(Reg);
      Changed = true;
    }
  }

  for (auto &Entry : BlockToRegs) {
    MachineBasicBlock *KeepAliveBB = Entry.first;
    auto InsertIt = KeepAliveBB->getFirstTerminator();
    for (Register Reg : Entry.second) {
      BuildMI(*KeepAliveBB, InsertIt, DebugLoc(),
              TII->get(RISCV::PseudoSGPRKeepAlive))
          .addReg(Reg);
      Changed = true;
    }
  }

  return Changed;
}

bool VentusRemoveSGPRKeepAlive::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.isVentusGPGPU())
    return false;

  bool Changed = false;
  DenseSet<const MachineBasicBlock *> MarkedKeepAliveBlocks;
  for (MachineBasicBlock &MBB : MF) {
    for (auto MI = MBB.begin(), E = MBB.end(); MI != E;) {
      MachineInstr &Instr = *MI++;
      if (Instr.getOpcode() == RISCV::PseudoSGPRKeepAliveBlock)
        MarkedKeepAliveBlocks.insert(&MBB);

      if (Instr.getOpcode() != RISCV::PseudoSGPRKeepAlive &&
          Instr.getOpcode() != RISCV::PseudoSGPRKeepAliveBlock)
        continue;
      Instr.eraseFromParent();
      Changed = true;
    }
  }

  SmallVector<MachineBasicBlock *, 8> DeadKeepAliveBlocks;
  for (MachineBasicBlock &MBB : MF)
    if (isRemovableKeepAliveEdgeBlock(MBB, MarkedKeepAliveBlocks))
      DeadKeepAliveBlocks.push_back(&MBB);

  for (MachineBasicBlock *MBB : DeadKeepAliveBlocks) {
    removeKeepAliveEdgeBlock(*MBB);
    Changed = true;
  }

  return Changed;
}

} // namespace

INITIALIZE_PASS(VentusInsertSGPRKeepAlive, "ventus-sgpr-keepalive",
                "Ventus Insert SGPR KeepAlive", false, false)

INITIALIZE_PASS(VentusRemoveSGPRKeepAlive, "ventus-remove-sgpr-keepalive",
                "Ventus Remove SGPR KeepAlive", false, false)

char VentusInsertSGPRKeepAlive::ID = 0;
char VentusRemoveSGPRKeepAlive::ID = 0;

namespace llvm {

FunctionPass *createVentusInsertSGPRKeepAlivePass() {
  return new VentusInsertSGPRKeepAlive();
}

FunctionPass *createVentusRemoveSGPRKeepAlivePass() {
  return new VentusRemoveSGPRKeepAlive();
}

} // namespace llvm
