//===-- VentusSIMTInterference.cpp - SIMT RA interference -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VentusSIMTInterference.h"
#include "RISCV.h"
#include "RISCVRegisterInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervalUnion.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-simt-interference"

namespace {

struct SIMTScope {
  MachineBasicBlock *BranchBB = nullptr;
  MachineInstr *BranchMI = nullptr;
  MachineBasicBlock *JoinBB = nullptr;
  SmallVector<SmallVector<MachineBasicBlock *, 8>, 2> Components;
  DenseMap<const MachineBasicBlock *, unsigned> ComponentOf;
  DenseSet<const MachineBasicBlock *> RegionBlocks;
};

struct CachedShadow {
  LiveRange Range;
  VNInfo::Allocator Allocator;
};

class VentusSIMTInterference final : public RegAllocExtraInterference {
  MachineFunction *MF = nullptr;
  LiveIntervals *LIS = nullptr;
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  SmallVector<SIMTScope, 8> Scopes;
  DenseMap<const MachineBasicBlock *, SmallVector<unsigned, 2>> ScopesByBlock;
  DenseSet<Register> AssignedShadowRegs;

public:
  void analyze(MachineFunction &MF, LiveIntervals &LIS, VirtRegMap &VRM,
               LiveRegMatrix &Matrix) override;
  LiveRegMatrix::InterferenceKind
  checkInterference(const LiveInterval &VirtReg, MCRegister PhysReg,
                    LiveRegMatrix &Matrix) override;
  void assign(const LiveInterval &VirtReg, MCRegister PhysReg,
              LiveRegMatrix &Matrix) override;
  bool unassign(const LiveInterval &VirtReg, MCRegister PhysReg,
                LiveRegMatrix &Matrix) override;
  void invalidateVirtRegs() override {
    // AssignedShadowRegs tracks only ownership in LiveRegMatrix. It must
    // survive query invalidation so later unassign can remove shadow segments.
  }

private:
  void buildScopes();
  void addScope(MachineBasicBlock &BranchBB, MachineInstr &BranchMI,
                MachineBasicBlock &JoinBB);
  LiveRange computeShadowRange(const LiveInterval &LI,
                               VNInfo::Allocator &Allocator) const;
  bool needsScopeShadow(const LiveInterval &LI, const SIMTScope &Scope,
                        const MachineOperand &UseMO) const;
  bool isSGPRInterval(const LiveInterval &LI) const;
  bool hasVirtualInterference(const LiveRange &Range, MCRegister PhysReg,
                              LiveRegMatrix &Matrix) const;
  bool hasRegUnitInterference(const LiveRange &Range, MCRegister PhysReg) const;
  bool hasRegMaskInterference(const LiveRange &Range, MCRegister PhysReg) const;
  void insertShadow(const LiveInterval &LI, MCRegister PhysReg,
                    const LiveRange &Shadow, LiveRegMatrix &Matrix) const;
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
                                             const TargetInstrInfo &TII) {
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
      Twine("Ventus SIMT RA interference requires every divergent branch to "
            "converge; missing immediate post-dominator in function '") +
      MF.getName() + "', machine block #" + Twine(MBB.getNumber()));
}

static bool isIgnorableUse(const MachineInstr &MI) {
  return MI.isDebugInstr();
}

static void addRangeSegment(LiveRange &Range, SlotIndex Start, SlotIndex End,
                            VNInfo *VNI) {
  if (Start < End)
    Range.addSegment(LiveRange::Segment(Start, End, VNI));
}

static void addMinusLiveInterval(LiveRange &Range, SlotIndex Start,
                                 SlotIndex End, const LiveInterval &LI,
                                 VNInfo *VNI) {
  if (Start >= End)
    return;

  SlotIndex Cursor = Start;
  auto I = LI.find(Start);
  for (auto E = LI.end(); I != E && I->start < End; ++I) {
    if (I->end <= Cursor)
      continue;
    if (Cursor < I->start)
      addRangeSegment(Range, Cursor, std::min(I->start, End), VNI);
    Cursor = std::max(Cursor, std::min(I->end, End));
    if (Cursor >= End)
      return;
  }
  addRangeSegment(Range, Cursor, End, VNI);
}

void VentusSIMTInterference::analyze(MachineFunction &MF, LiveIntervals &LIS,
                                     VirtRegMap &VRM,
                                     LiveRegMatrix &Matrix) {
  this->MF = &MF;
  this->LIS = &LIS;
  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MRI = &MF.getRegInfo();
  Scopes.clear();
  ScopesByBlock.clear();
  AssignedShadowRegs.clear();
  buildScopes();
}

void VentusSIMTInterference::buildScopes() {
  MachinePostDominatorTree MPDT;
  MPDT.getBase().recalculate(*MF);

  for (MachineBasicBlock &MBB : *MF) {
    MachineInstr *BranchMI = getDivergentBranchInstr(MBB, *TII);
    if (!BranchMI)
      continue;
    addScope(MBB, *BranchMI,
             *getImmediatePostDominatorOrReport(*MF, MBB, MPDT));
  }
}

void VentusSIMTInterference::addScope(MachineBasicBlock &BranchBB,
                                      MachineInstr &BranchMI,
                                      MachineBasicBlock &JoinBB) {
  SIMTScope Scope;
  Scope.BranchBB = &BranchBB;
  Scope.BranchMI = &BranchMI;
  Scope.JoinBB = &JoinBB;

  DenseSet<MachineBasicBlock *> Seen;
  Seen.insert(&BranchBB);
  Seen.insert(&JoinBB);
  unsigned Component = 0;

  for (MachineBasicBlock *Succ : BranchBB.successors()) {
    if (Succ == &JoinBB || !Seen.insert(Succ).second)
      continue;
    Scope.Components.emplace_back();
    SmallVector<MachineBasicBlock *, 8> Worklist;
    Worklist.push_back(Succ);
    while (!Worklist.empty()) {
      MachineBasicBlock *MBB = Worklist.pop_back_val();
      Scope.Components[Component].push_back(MBB);
      Scope.ComponentOf[MBB] = Component;
      Scope.RegionBlocks.insert(MBB);
      for (MachineBasicBlock *Next : MBB->successors()) {
        if (Next == &JoinBB || !Seen.insert(Next).second)
          continue;
        Worklist.push_back(Next);
      }
    }
    ++Component;
  }

  if (Scope.Components.size() < 2)
    return;

  unsigned ScopeID = Scopes.size();
  for (const auto &CompBlocks : Scope.Components)
    for (MachineBasicBlock *MBB : CompBlocks)
      ScopesByBlock[MBB].push_back(ScopeID);
  Scopes.push_back(std::move(Scope));
}

bool VentusSIMTInterference::isSGPRInterval(const LiveInterval &LI) const {
  if (!LI.reg().isVirtual() || MRI->reg_nodbg_empty(LI.reg()))
    return false;
  return RISCVRegisterInfo::isSGPRClass(MRI->getRegClass(LI.reg()));
}

bool VentusSIMTInterference::needsScopeShadow(
    const LiveInterval &LI, const SIMTScope &Scope,
    const MachineOperand &UseMO) const {
  const MachineInstr &UseMI = *UseMO.getParent();
  SlotIndex UseIdx = LIS->getInstructionIndex(UseMI).getRegSlot();
  VNInfo *VNI = LI.getVNInfoBefore(UseIdx);
  if (!VNI || VNI->isUnused())
    return false;

  SlotIndex DefIdx = VNI->def;
  MachineBasicBlock *DefBB = LIS->getMBBFromIndex(DefIdx);
  if (!DefBB)
    return true;
  if (DefBB == Scope.BranchBB) {
    SlotIndex BranchIdx = LIS->getInstructionIndex(*Scope.BranchMI).getRegSlot();
    return DefIdx < BranchIdx;
  }
  if (DefBB == Scope.JoinBB)
    return false;
  return !Scope.RegionBlocks.contains(DefBB);
}

LiveRange
VentusSIMTInterference::computeShadowRange(const LiveInterval &LI,
                                           VNInfo::Allocator &Allocator) const {
  LiveRange Shadow;
  if (!isSGPRInterval(LI) || Scopes.empty())
    return Shadow;

  DenseMap<unsigned, BitVector> UsedComponents;
  for (const MachineOperand &MO : MRI->use_nodbg_operands(LI.reg())) {
    const MachineInstr &UseMI = *MO.getParent();
    if (isIgnorableUse(UseMI))
      continue;

    auto ScopeIt = ScopesByBlock.find(UseMI.getParent());
    if (ScopeIt != ScopesByBlock.end()) {
      for (unsigned ScopeID : ScopeIt->second) {
        const SIMTScope &Scope = Scopes[ScopeID];
        auto CompIt = Scope.ComponentOf.find(UseMI.getParent());
        if (CompIt == Scope.ComponentOf.end())
          continue;
        if (!needsScopeShadow(LI, Scope, MO))
          continue;

        BitVector &Comps = UsedComponents[ScopeID];
        if (Comps.empty())
          Comps.resize(Scope.Components.size());
        Comps.set(CompIt->second);
      }
      continue;
    }

    for (unsigned ScopeID = 0, ScopeE = Scopes.size(); ScopeID != ScopeE;
         ++ScopeID) {
      const SIMTScope &Scope = Scopes[ScopeID];
      if (UseMI.getParent() != Scope.JoinBB)
        continue;
      if (UseMI.getOpcode() == RISCV::PseudoSGPRKeepAliveBlock)
        continue;
      if (!needsScopeShadow(LI, Scope, MO))
        continue;

      BitVector &Comps = UsedComponents[ScopeID];
      if (Comps.empty())
        Comps.resize(Scope.Components.size());
      Comps.set();
    }
  }

  if (UsedComponents.empty())
    return Shadow;

  VNInfo *VNI = new (Allocator) VNInfo(0, SlotIndex());
  for (const auto &Entry : UsedComponents) {
    const SIMTScope &Scope = Scopes[Entry.first];
    const BitVector &Used = Entry.second;
    bool UseAllComponents = Used.count() > 1;
    for (unsigned C = 0, E = Scope.Components.size(); C != E; ++C) {
      if (!UseAllComponents && Used.test(C))
        continue;
      for (MachineBasicBlock *MBB : Scope.Components[C])
        addMinusLiveInterval(Shadow, LIS->getMBBStartIdx(MBB),
                             LIS->getMBBEndIdx(MBB), LI, VNI);
    }
  }

  return Shadow;
}

bool VentusSIMTInterference::hasVirtualInterference(
    const LiveRange &Range, MCRegister PhysReg, LiveRegMatrix &Matrix) const {
  if (Range.empty())
    return false;
  for (MCRegUnitIterator Unit(PhysReg, TRI); Unit.isValid(); ++Unit) {
    LiveIntervalUnion::Query Query;
    Query.reset(0, Range, Matrix.getLiveUnions()[*Unit]);
    if (Query.checkInterference())
      return true;
  }
  return false;
}

bool VentusSIMTInterference::hasRegUnitInterference(
    const LiveRange &Range, MCRegister PhysReg) const {
  if (Range.empty())
    return false;
  for (MCRegUnitIterator Unit(PhysReg, TRI); Unit.isValid(); ++Unit)
    if (Range.overlaps(LIS->getRegUnit(*Unit)))
      return true;
  return false;
}

bool VentusSIMTInterference::hasRegMaskInterference(
    const LiveRange &Range, MCRegister PhysReg) const {
  if (Range.empty())
    return false;

  ArrayRef<SlotIndex> Slots = LIS->getRegMaskSlots();
  ArrayRef<const uint32_t *> Bits = LIS->getRegMaskBits();
  for (const LiveRange::Segment &Segment : Range) {
    auto SlotIt = llvm::lower_bound(Slots, Segment.start);
    for (auto SlotEnd = Slots.end(); SlotIt != SlotEnd && *SlotIt < Segment.end;
         ++SlotIt) {
      unsigned MaskIndex = SlotIt - Slots.begin();
      if (MachineOperand::clobbersPhysReg(Bits[MaskIndex], PhysReg))
        return true;
    }
  }
  return false;
}

LiveRegMatrix::InterferenceKind VentusSIMTInterference::checkInterference(
    const LiveInterval &VirtReg, MCRegister PhysReg,
    LiveRegMatrix &Matrix) {
  CachedShadow Shadow;
  Shadow.Range = computeShadowRange(VirtReg, Shadow.Allocator);
  if (Shadow.Range.empty())
    return LiveRegMatrix::IK_Free;
  if (hasRegMaskInterference(Shadow.Range, PhysReg))
    return LiveRegMatrix::IK_RegMask;
  if (hasRegUnitInterference(Shadow.Range, PhysReg))
    return LiveRegMatrix::IK_RegUnit;
  if (hasVirtualInterference(Shadow.Range, PhysReg, Matrix))
    // Greedy can only enumerate eviction victims for VirtReg's ordinary live
    // range, not this temporary shadow range, so report shadow virtual conflicts
    // as non-evictable.
    return LiveRegMatrix::IK_RegUnit;
  return LiveRegMatrix::IK_Free;
}

void VentusSIMTInterference::insertShadow(const LiveInterval &LI,
                                          MCRegister PhysReg,
                                          const LiveRange &Shadow,
                                          LiveRegMatrix &Matrix) const {
  if (Shadow.empty())
    return;
  for (MCRegUnitIterator Unit(PhysReg, TRI); Unit.isValid(); ++Unit)
    Matrix.getLiveUnions()[*Unit].unify(LI, Shadow);
}

void VentusSIMTInterference::assign(const LiveInterval &VirtReg,
                                    MCRegister PhysReg,
                                    LiveRegMatrix &Matrix) {
  CachedShadow Shadow;
  Shadow.Range = computeShadowRange(VirtReg, Shadow.Allocator);
  if (Shadow.Range.empty())
    return;
  insertShadow(VirtReg, PhysReg, Shadow.Range, Matrix);
  AssignedShadowRegs.insert(VirtReg.reg());
}

bool VentusSIMTInterference::unassign(const LiveInterval &VirtReg, MCRegister,
                                      LiveRegMatrix &) {
  return AssignedShadowRegs.erase(VirtReg.reg());
}

} // end namespace

namespace llvm {

std::unique_ptr<RegAllocExtraInterference> createVentusSIMTInterference() {
  return std::make_unique<VentusSIMTInterference>();
}

} // end namespace llvm
