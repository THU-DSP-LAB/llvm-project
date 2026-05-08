//===-- VentusSGPRSIMTChecker.cpp - Check SIMT SGPR hazards --------------===//
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
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-sgpr-simt-checker"

namespace {

class VentusSGPRSIMTChecker : public MachineFunctionPass {
public:
  static char ID;

  VentusSGPRSIMTChecker() : MachineFunctionPass(ID) {
    initializeVentusSGPRSIMTCheckerPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Ventus SGPR SIMT Checker"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachinePostDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

struct SiblingSGPRAccessInfo {
  MachineBasicBlock *RootBB = nullptr;
  SmallVector<MachineBasicBlock *, 8> Blocks;
  DenseMap<Register, MachineInstr *> LiveInUses;
  DenseMap<Register, MachineInstr *> Defs;
};

using SGPRDefMap = DenseMap<Register, MachineInstr *>;

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
  for (auto J = I.getReverse();
       J != MBB.rend() && TII.isUnpredicatedTerminator(*J); ++J)
    ++NumTerminators;

  if (NumTerminators == 1 && isDivergentBranchOpcode(I->getOpcode()))
    return &*I;

  if (NumTerminators == 2 &&
      isDivergentBranchOpcode(std::prev(I)->getOpcode()) &&
      I->getDesc().isUnconditionalBranch())
    return &*std::prev(I);

  return nullptr;
}

static MachineBasicBlock *
getImmediatePostDominatorOrReport(MachineFunction &MF, MachineBasicBlock &MBB,
                                  MachinePostDominatorTree &MPDT) {
  auto *Node = MPDT.getNode(&MBB);
  auto *IDom = Node ? Node->getIDom() : nullptr;
  MachineBasicBlock *JoinBB = IDom ? IDom->getBlock() : nullptr;
  if (JoinBB)
    return JoinBB;

  report_fatal_error(
      Twine("Ventus SGPR SIMT Checker requires every divergent branch to "
            "converge; missing immediate post-dominator in function '") +
      MF.getName() + "', machine block #" + Twine(MBB.getNumber()));
}

static SmallVector<MachineBasicBlock *, 8>
collectSiblingBlocks(MachineBasicBlock &RootBB, MachineBasicBlock &JoinBB) {
  SmallVector<MachineBasicBlock *, 8> Worklist;
  SmallVector<MachineBasicBlock *, 8> Blocks;
  DenseSet<MachineBasicBlock *> Seen;
  Seen.insert(&JoinBB);
  Seen.insert(&RootBB);
  Worklist.push_back(&RootBB);

  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    Blocks.push_back(MBB);
    for (MachineBasicBlock *Succ : MBB->successors()) {
      if (!Seen.insert(Succ).second)
        continue;
      Worklist.push_back(Succ);
    }
  }

  return Blocks;
}

static bool isPhysicalSGPR(Register Reg) {
  return Reg.isPhysical() && Reg != RISCV::X0 &&
         RISCV::GPRRegClass.contains(Reg);
}

static void recordSGPRDef(Register Reg, MachineInstr &MI,
                          DenseSet<Register> &BlockDefs,
                          SGPRDefMap &Defs) {
  if (!isPhysicalSGPR(Reg))
    return;

  BlockDefs.insert(Reg);
  Defs.try_emplace(Reg, &MI);
}

static void recordSGPRRegMaskClobbers(const MachineOperand &MO,
                                      MachineInstr &MI,
                                      DenseSet<Register> &BlockDefs,
                                      SGPRDefMap &Defs) {
  assert(MO.isRegMask() && "expected a regmask operand");

  for (MCPhysReg Reg : RISCV::GPRRegClass)
    if (Reg != RISCV::X0 && MO.clobbersPhysReg(Reg))
      recordSGPRDef(Register(Reg), MI, BlockDefs, Defs);
}

static void eraseRestoredSGPRDef(Register Reg, SGPRDefMap &Defs,
                                 const TargetRegisterInfo &TRI) {
  if (!isPhysicalSGPR(Reg))
    return;

  SmallVector<Register, 4> Restored;
  for (const auto &Entry : Defs)
    if (TRI.regsOverlap(Reg.asMCReg(), Entry.first.asMCReg()))
      Restored.push_back(Entry.first);

  for (Register RestoredReg : Restored)
    Defs.erase(RestoredReg);
}

static bool mergeSGPRDefs(SGPRDefMap &Into, const SGPRDefMap &From) {
  bool Changed = false;
  for (const auto &Entry : From)
    Changed |= Into.try_emplace(Entry.first, Entry.second).second;
  return Changed;
}

static bool isSGPRSpillReload(const MachineFunction &MF,
                              const MachineInstr &MI) {
  if (MI.getOpcode() != RISCV::LW)
    return false;

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  for (const MachineMemOperand *MMO : MI.memoperands()) {
    const PseudoSourceValue *PSV = MMO->getPseudoValue();
    if (!PSV || PSV->kind() != PseudoSourceValue::FixedStack)
      continue;

    int FI = cast<FixedStackPseudoSourceValue>(PSV)->getFrameIndex();
    if (MFI.getStackID(FI) == TargetStackID::SGPRSpill)
      return true;
  }

  return false;
}

static bool hasSeenDef(Register Reg, const DenseSet<Register> &Defs,
                       const TargetRegisterInfo &TRI) {
  for (Register Def : Defs)
    if (TRI.regsOverlap(Reg.asMCReg(), Def.asMCReg()))
      return true;
  return false;
}

static bool isSiblingRootLiveIn(Register Reg, const SiblingSGPRAccessInfo &Info,
                                const TargetRegisterInfo &TRI) {
  for (const MachineBasicBlock::RegisterMaskPair &LiveIn :
       Info.RootBB->liveins())
    if (TRI.regsOverlap(Reg.asMCReg(), Register(LiveIn.PhysReg).asMCReg()))
      return true;
  return false;
}

static SGPRDefMap transferSiblingBlock(MachineFunction &MF,
                                       SiblingSGPRAccessInfo &Info,
                                       MachineBasicBlock &MBB,
                                       const SGPRDefMap &In,
                                       const TargetRegisterInfo &TRI) {
  SGPRDefMap State = In;
  DenseSet<Register> BlockDefs;
  for (MachineInstr &MI : MBB) {
    if (MI.isDebugInstr())
      continue;
    if (MI.getOpcode() == RISCV::PseudoSGPRKeepAliveBlock)
      continue;

    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isUse())
        continue;

      Register Reg = MO.getReg();
      if (!isPhysicalSGPR(Reg) || !MBB.isLiveIn(Reg.asMCReg()))
        continue;
      if (!isSiblingRootLiveIn(Reg, Info, TRI))
        continue;
      if (hasSeenDef(Reg, BlockDefs, TRI))
        continue;

      Info.LiveInUses.try_emplace(Reg, &MI);
    }

    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isRegMask()) {
        recordSGPRRegMaskClobbers(MO, MI, BlockDefs, State);
        continue;
      }

      if (!MO.isReg() || !MO.isDef())
        continue;

      Register Reg = MO.getReg();
      if (isSGPRSpillReload(MF, MI)) {
        if (isPhysicalSGPR(Reg))
          BlockDefs.insert(Reg);
        eraseRestoredSGPRDef(Reg, State, TRI);
      } else {
        recordSGPRDef(Reg, MI, BlockDefs, State);
      }
    }
  }
  return State;
}

static void collectSiblingAccessInfo(MachineFunction &MF,
                                     SiblingSGPRAccessInfo &Info,
                                     MachineBasicBlock &JoinBB,
                                     const TargetRegisterInfo &TRI) {
  DenseSet<MachineBasicBlock *> ComponentBlocks(Info.Blocks.begin(),
                                                Info.Blocks.end());
  DenseMap<MachineBasicBlock *, SGPRDefMap> InDefs;
  DenseMap<MachineBasicBlock *, SGPRDefMap> OutDefs;
  SmallVector<MachineBasicBlock *, 8> Worklist;
  DenseSet<MachineBasicBlock *> Queued;
  DenseSet<MachineBasicBlock *> Processed;
  Worklist.push_back(Info.RootBB);
  Queued.insert(Info.RootBB);

  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    Queued.erase(MBB);

    SGPRDefMap Out = transferSiblingBlock(MF, Info, *MBB, InDefs[MBB], TRI);
    bool FirstVisit = Processed.insert(MBB).second;
    bool OutChanged = mergeSGPRDefs(OutDefs[MBB], Out);
    if (!FirstVisit && !OutChanged)
      continue;

    for (MachineBasicBlock *Succ : MBB->successors()) {
      if (Succ == &JoinBB) {
        mergeSGPRDefs(Info.Defs, OutDefs[MBB]);
        continue;
      }
      if (!ComponentBlocks.contains(Succ))
        continue;
      bool InChanged = mergeSGPRDefs(InDefs[Succ], OutDefs[MBB]);
      if (!InChanged && Processed.contains(Succ))
        continue;
      if (Queued.insert(Succ).second)
        Worklist.push_back(Succ);
    }
  }
}

static void collectJoinAccessInfo(SiblingSGPRAccessInfo &Info,
                                  MachineBasicBlock &JoinBB,
                                  const TargetRegisterInfo &TRI) {
  Info.RootBB = &JoinBB;

  DenseSet<Register> BlockDefs;
  for (MachineInstr &MI : JoinBB) {
    if (MI.isDebugInstr())
      continue;
    if (MI.getOpcode() == RISCV::PseudoSGPRKeepAliveBlock)
      continue;

    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isUse())
        continue;

      Register Reg = MO.getReg();
      if (!isPhysicalSGPR(Reg) || !JoinBB.isLiveIn(Reg.asMCReg()))
        continue;
      if (hasSeenDef(Reg, BlockDefs, TRI))
        continue;

      Info.LiveInUses.try_emplace(Reg, &MI);
    }

    for (const MachineOperand &MO : MI.operands())
      if (MO.isReg() && MO.isDef())
        BlockDefs.insert(MO.getReg());
  }
}

static std::string formatMachineInstr(const MachineInstr &MI) {
  std::string Text;
  raw_string_ostream OS(Text);
  MI.print(OS);
  return OS.str();
}

static std::string formatReg(Register Reg, const TargetRegisterInfo &TRI) {
  std::string Text;
  raw_string_ostream OS(Text);
  OS << printReg(Reg, &TRI);
  return OS.str();
}

static void reportSIMTSiblingSGPRClobber(
    MachineFunction &MF, MachineBasicBlock &BranchBB, MachineBasicBlock &JoinBB,
    const SiblingSGPRAccessInfo &UseSibling,
    const SiblingSGPRAccessInfo &DefSibling, Register Reg, MachineInstr &UseMI,
    MachineInstr &DefMI, const TargetRegisterInfo &TRI) {
  report_fatal_error(
      Twine("Ventus SGPR SIMT sibling clobber in function '") + MF.getName() +
      "': branch block #" + Twine(BranchBB.getNumber()) + ", join block #" +
      Twine(JoinBB.getNumber()) + ", register " + formatReg(Reg, TRI) +
      " is used as live-in in sibling block #" +
      Twine(UseSibling.RootBB->getNumber()) +
      " but defined in sibling block #" +
      Twine(DefSibling.RootBB->getNumber()) +
      ".\n  live-in use: " + formatMachineInstr(UseMI) +
      "  sibling def: " + formatMachineInstr(DefMI));
}

static void checkSiblingPairForSGPRClobber(
    MachineFunction &MF, MachineBasicBlock &BranchBB, MachineBasicBlock &JoinBB,
    const SiblingSGPRAccessInfo &UseSibling,
    const SiblingSGPRAccessInfo &DefSibling, const TargetRegisterInfo &TRI) {
  for (const auto &UseEntry : UseSibling.LiveInUses) {
    Register UseReg = UseEntry.first;
    MachineInstr *UseMI = UseEntry.second;
    for (const auto &DefEntry : DefSibling.Defs) {
      Register DefReg = DefEntry.first;
      if (!TRI.regsOverlap(UseReg.asMCReg(), DefReg.asMCReg()))
        continue;

      reportSIMTSiblingSGPRClobber(MF, BranchBB, JoinBB, UseSibling, DefSibling,
                                   UseReg, *UseMI, *DefEntry.second, TRI);
    }
  }
}

static void verifyNoSIMTSiblingSGPRClobber(MachineFunction &MF,
                                           const RISCVInstrInfo &TII,
                                           MachinePostDominatorTree &MPDT,
                                           const TargetRegisterInfo &TRI) {
  for (MachineBasicBlock &MBB : MF) {
    MachineInstr *BranchMI = getDivergentBranchInstr(MBB, TII);
    if (!BranchMI)
      continue;

    MachineBasicBlock *JoinBB =
        getImmediatePostDominatorOrReport(MF, MBB, MPDT);
    SmallVector<SiblingSGPRAccessInfo, 2> Siblings;
    for (MachineBasicBlock *Succ : MBB.successors()) {
      if (Succ == JoinBB)
        continue;
      SiblingSGPRAccessInfo Info;
      Info.RootBB = Succ;
      Info.Blocks = collectSiblingBlocks(*Succ, *JoinBB);
      collectSiblingAccessInfo(MF, Info, *JoinBB, TRI);
      Siblings.push_back(std::move(Info));
    }

    SiblingSGPRAccessInfo JoinInfo;
    collectJoinAccessInfo(JoinInfo, *JoinBB, TRI);
    Siblings.push_back(std::move(JoinInfo));

    for (unsigned I = 0, E = Siblings.size(); I < E; ++I)
      for (unsigned J = 0; J < E; ++J)
        if (I != J)
          checkSiblingPairForSGPRClobber(MF, MBB, *JoinBB, Siblings[I],
                                         Siblings[J], TRI);
  }
}

bool VentusSGPRSIMTChecker::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  verifyNoSIMTSiblingSGPRClobber(MF, *ST.getInstrInfo(),
                                 getAnalysis<MachinePostDominatorTree>(),
                                 *ST.getRegisterInfo());
  return false;
}

} // namespace

INITIALIZE_PASS(VentusSGPRSIMTChecker, DEBUG_TYPE, "Ventus SGPR SIMT Checker",
                false, false)

char VentusSGPRSIMTChecker::ID = 0;

namespace llvm {

FunctionPass *createVentusSGPRSIMTCheckerPass() {
  return new VentusSGPRSIMTChecker();
}

} // namespace llvm
