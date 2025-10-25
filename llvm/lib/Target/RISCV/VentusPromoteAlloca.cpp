//===-- VentusPromoteAlloca.cpp - Promote Allocas -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass eliminates allocas by converting them into vectors.
// Based on AMDGPU and AIGCGPU implementations.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVSubtarget.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#define DEBUG_TYPE "ventus-promote-alloca"

using namespace llvm;

namespace {

static cl::opt<bool> DisablePromoteAllocaToVector(
    "disable-ventus-promote-alloca-to-vector",
    cl::desc("Disable promote alloca to vector for Ventus"), cl::init(false));

static cl::opt<unsigned> PromoteAllocaToVectorLimit(
    "ventus-promote-alloca-to-vector-limit",
    cl::desc("Maximum byte size to consider promote alloca to vector"),
    cl::init(256));

// TODO: promote larger vector to LDS


class VentusPromoteAlloca : public FunctionPass {
public:
  static char ID;

  VentusPromoteAlloca() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "Ventus Promote Alloca"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    FunctionPass::getAnalysisUsage(AU);
  }
};

class VentusPromoteAllocaImpl {
private:
  const TargetMachine &TM;
  Module *Mod = nullptr;
  const DataLayout *DL = nullptr;
  unsigned MaxVGPRs = 256;

  bool tryPromoteAllocaToVector(AllocaInst &I);

public:
  VentusPromoteAllocaImpl(TargetMachine &TM) : TM(TM) {}
  bool run(Function &F);
};

} // end anonymous namespace

char VentusPromoteAlloca::ID = 0;

INITIALIZE_PASS(VentusPromoteAlloca, DEBUG_TYPE,
                "Ventus promote alloca to vector", false, false)

char &llvm::VentusPromoteAllocaID = VentusPromoteAlloca::ID;

bool VentusPromoteAlloca::runOnFunction(Function &F) {

  if (auto *TPC = getAnalysisIfAvailable<TargetPassConfig>()) {
    return VentusPromoteAllocaImpl(TPC->getTM<TargetMachine>()).run(F);
  }
  return false;
}

bool VentusPromoteAllocaImpl::run(Function &F) {
  Mod = F.getParent();
  DL = &Mod->getDataLayout();

  bool Changed = false;
  BasicBlock &EntryBB = *F.begin();

  SmallVector<AllocaInst *, 16> Allocas;
  for (Instruction &I : EntryBB) {
    if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
      if (!AI->isStaticAlloca() || AI->isArrayAllocation())
        continue;
      // Only handle allocas in private address space
      if (AI->getAddressSpace() == RISCVAS::PRIVATE_ADDRESS)
        Allocas.push_back(AI);
    }
  }

  for (AllocaInst *AI : Allocas) {
    if (tryPromoteAllocaToVector(*AI))
      Changed = true;
  }

  return Changed;
}

static void collectAllocaUses(AllocaInst &Alloca,
                              SmallVectorImpl<Use *> &Uses) {
  SmallVector<Instruction *, 4> WorkList({&Alloca});
  while (!WorkList.empty()) {
    auto *Cur = WorkList.pop_back_val();
    for (auto &U : Cur->uses()) {
      Uses.push_back(&U);
      if (isa<GetElementPtrInst>(U.getUser()))
        WorkList.push_back(cast<Instruction>(U.getUser()));
    }
  }
}

static Value *calculateVectorIndex(
    Value *Ptr, const std::map<GetElementPtrInst *, Value *> &GEPIdx) {
  auto *GEP = dyn_cast<GetElementPtrInst>(Ptr->stripPointerCasts());
  if (!GEP)
    return ConstantInt::getNullValue(Type::getInt32Ty(Ptr->getContext()));

  auto I = GEPIdx.find(GEP);
  assert(I != GEPIdx.end() && "Must have entry for GEP!");
  return I->second;
}

static Value *GEPToVectorIndex(GetElementPtrInst *GEP, AllocaInst *Alloca,
                               Type *VecElemTy, const DataLayout &DL) {
  unsigned BW = DL.getIndexTypeSizeInBits(GEP->getType());
  MapVector<Value *, APInt> VarOffsets;
  APInt ConstOffset(BW, 0);
  if (GEP->getPointerOperand()->stripPointerCasts() != Alloca ||
      !GEP->collectOffset(DL, BW, VarOffsets, ConstOffset))
    return nullptr;

  unsigned VecElemSize = DL.getTypeAllocSize(VecElemTy);
  if (VarOffsets.size() > 1)
    return nullptr;

  if (VarOffsets.size() == 1) {
    const auto &VarOffset = VarOffsets.front();
    if (!ConstOffset.isZero() || VarOffset.second != VecElemSize)
      return nullptr;
    return VarOffset.first;
  }

  APInt Quot;
  uint64_t Rem;
  APInt::udivrem(ConstOffset, VecElemSize, Quot, Rem);
  if (Rem != 0)
    return nullptr;

  return ConstantInt::get(GEP->getContext(), Quot);
}

static bool isSupportedAccessType(FixedVectorType *VecTy, Type *AccessTy,
                                  const DataLayout &DL) {
  if (isa<FixedVectorType>(AccessTy)) {
    TypeSize AccTS = DL.getTypeStoreSize(AccessTy);
    TypeSize VecTS = DL.getTypeStoreSize(VecTy->getElementType());
    return AccTS.isKnownMultipleOf(VecTS);
  }
  return CastInst::isBitOrNoopPointerCastable(VecTy->getElementType(),
                                              AccessTy, DL);
}

static Value *promoteAllocaUserToVector(
    Instruction *Inst, const DataLayout &DL, FixedVectorType *VectorTy,
    unsigned VecStoreSize, unsigned ElementSize,
    std::map<GetElementPtrInst *, Value *> &GEPVectorIdx, Value *CurVal,
    SmallVectorImpl<LoadInst *> &DeferredLoads) {

  IRBuilder<> Builder(Inst);
  Type *VecEltTy = VectorTy->getElementType();

  const auto GetOrLoadCurrentVectorValue = [&]() -> Value * {
    if (CurVal)
      return CurVal;
    LoadInst *Dummy =
        Builder.CreateLoad(VectorTy, PoisonValue::get(Builder.getPtrTy()),
                          "promotealloca.dummyload");
    DeferredLoads.push_back(Dummy);
    return Dummy;
  };

  switch (Inst->getOpcode()) {
  case Instruction::Load: {
    if (!CurVal) {
      DeferredLoads.push_back(cast<LoadInst>(Inst));
      return nullptr;
    }
    Value *Index = calculateVectorIndex(
        cast<LoadInst>(Inst)->getPointerOperand(), GEPVectorIdx);
    Type *AccessTy = Inst->getType();
    TypeSize AccessSize = DL.getTypeStoreSize(AccessTy);

    // Loading full vector
    if (Constant *CI = dyn_cast<Constant>(Index)) {
      if (CI->isZeroValue() && AccessSize == VecStoreSize) {
        Value *NewVal = Builder.CreateBitOrPointerCast(CurVal, AccessTy);
        Inst->replaceAllUsesWith(NewVal);
        return nullptr;
      }
    }

    // Loading single element
    Value *ExtractElement = Builder.CreateExtractElement(CurVal, Index);
    if (AccessTy != VecEltTy)
      ExtractElement = Builder.CreateBitOrPointerCast(ExtractElement, AccessTy);
    Inst->replaceAllUsesWith(ExtractElement);
    return nullptr;
  }
  case Instruction::Store: {
    StoreInst *SI = cast<StoreInst>(Inst);
    Value *Index = calculateVectorIndex(SI->getPointerOperand(), GEPVectorIdx);
    Value *Val = SI->getValueOperand();
    Type *AccessTy = Val->getType();
    TypeSize AccessSize = DL.getTypeStoreSize(AccessTy);

    // Storing full vector
    if (Constant *CI = dyn_cast<Constant>(Index)) {
      if (CI->isZeroValue() && AccessSize == VecStoreSize) {
        return Builder.CreateBitOrPointerCast(Val, VectorTy);
      }
    }

    // Storing single element
    if (Val->getType() != VecEltTy)
      Val = Builder.CreateBitOrPointerCast(Val, VecEltTy);
    return Builder.CreateInsertElement(GetOrLoadCurrentVectorValue(), Val,
                                      Index);
  }
  default:
    llvm_unreachable("Unsupported instruction in promoteAllocaUserToVector");
  }
}

template <typename InstContainer>
static void forEachWorkListItem(const InstContainer &WorkList,
                                std::function<void(Instruction *)> Fn) {
  DenseMap<BasicBlock *, SmallDenseSet<Instruction *>> UsesByBlock;
  for (Instruction *User : WorkList)
    UsesByBlock[User->getParent()].insert(User);

  for (Instruction *User : WorkList) {
    BasicBlock *BB = User->getParent();
    auto &BlockUses = UsesByBlock[BB];
    if (BlockUses.empty())
      continue;

    if (BlockUses.size() == 1) {
      Fn(User);
      continue;
    }

    for (Instruction &Inst : *BB) {
      if (!BlockUses.contains(&Inst))
        continue;
      Fn(&Inst);
    }
    BlockUses.clear();
  }
}

bool VentusPromoteAllocaImpl::tryPromoteAllocaToVector(AllocaInst &Alloca) {
  LLVM_DEBUG(dbgs() << "Trying to promote to vector: " << Alloca << '\n');

  if (DisablePromoteAllocaToVector) {
    LLVM_DEBUG(dbgs() << "  Promotion disabled\n");
    return false;
  }

  Type *AllocaTy = Alloca.getAllocatedType();
  auto *VectorTy = dyn_cast<FixedVectorType>(AllocaTy);

  // Convert array to vector
  if (auto *ArrayTy = dyn_cast<ArrayType>(AllocaTy)) {
    if (VectorType::isValidElementType(ArrayTy->getElementType()) &&
        ArrayTy->getNumElements() > 0)
      VectorTy = FixedVectorType::get(ArrayTy->getElementType(),
                                     ArrayTy->getNumElements());
  }

  if (!VectorTy) {
    LLVM_DEBUG(dbgs() << "  Cannot convert type to vector\n");
    return false;
  }

  // Check size limit
  unsigned AllocaSize = DL->getTypeSizeInBits(AllocaTy);
  if (AllocaSize > PromoteAllocaToVectorLimit * 8) {
    LLVM_DEBUG(dbgs() << "  Alloca too big: " << AllocaSize << " bits\n");
    return false;
  }

  if (VectorTy->getNumElements() > 16 || VectorTy->getNumElements() < 1) {
    LLVM_DEBUG(dbgs() << "  Invalid vector size\n");
    return false;
  }

  // Collect and validate uses
  std::map<GetElementPtrInst *, Value *> GEPVectorIdx;
  SmallVector<Instruction *> WorkList;
  SmallVector<Instruction *> UsersToRemove;
  SmallVector<Use *, 8> Uses;
  collectAllocaUses(Alloca, Uses);

  Type *VecEltTy = VectorTy->getElementType();
  unsigned ElementSize = DL->getTypeSizeInBits(VecEltTy) / 8;

  const auto RejectUser = [&](Instruction *Inst, const char *Msg) {
    LLVM_DEBUG(dbgs() << "  Cannot promote: " << Msg << "\n"
                      << "    " << *Inst << "\n");
    return false;
  };

  for (auto *U : Uses) {
    Instruction *Inst = cast<Instruction>(U->getUser());

    if (Value *Ptr = getLoadStorePointerOperand(Inst)) {
      if (isa<StoreInst>(Inst) &&
          U->getOperandNo() != StoreInst::getPointerOperandIndex())
        return RejectUser(Inst, "storing pointer");

      Type *AccessTy = getLoadStoreType(Inst);
      bool IsSimple = isa<LoadInst>(Inst) ? cast<LoadInst>(Inst)->isSimple()
                                          : cast<StoreInst>(Inst)->isSimple();
      if (!IsSimple)
        return RejectUser(Inst, "not simple load/store");

      Ptr = Ptr->stripPointerCasts();
      if (Ptr == &Alloca &&
          DL->getTypeStoreSize(Alloca.getAllocatedType()) ==
              DL->getTypeStoreSize(AccessTy)) {
        WorkList.push_back(Inst);
        continue;
      }

      if (!isSupportedAccessType(VectorTy, AccessTy, *DL))
        return RejectUser(Inst, "unsupported access type");

      WorkList.push_back(Inst);
      continue;
    }

    if (auto *GEP = dyn_cast<GetElementPtrInst>(Inst)) {
      Value *Index = GEPToVectorIndex(GEP, &Alloca, VecEltTy, *DL);
      if (!Index)
        return RejectUser(Inst, "cannot compute GEP index");
      GEPVectorIdx[GEP] = Index;
      UsersToRemove.push_back(Inst);
      continue;
    }

    if (isAssumeLikeIntrinsic(Inst)) {
      UsersToRemove.push_back(Inst);
      continue;
    }

    if (isa<ICmpInst>(Inst) && all_of(Inst->users(), [](User *U) {
          return isAssumeLikeIntrinsic(cast<Instruction>(U));
        })) {
      UsersToRemove.push_back(Inst);
      continue;
    }

    return RejectUser(Inst, "unhandled user");
  }

  LLVM_DEBUG(dbgs() << "  Converting to vector: " << *VectorTy << '\n');

  const unsigned VecStoreSize = DL->getTypeStoreSize(VectorTy);

  // Use SSAUpdater for cross-block promotion
  SSAUpdater Updater;
  Updater.Initialize(VectorTy, "promotealloca");
  Updater.AddAvailableValue(Alloca.getParent(), UndefValue::get(VectorTy));

  SmallVector<LoadInst *, 4> DeferredLoads;

  // First pass: process instructions with known values
  forEachWorkListItem(WorkList, [&](Instruction *I) {
    BasicBlock *BB = I->getParent();
    Value *Result = promoteAllocaUserToVector(
        I, *DL, VectorTy, VecStoreSize, ElementSize, GEPVectorIdx,
        Updater.FindValueForBlock(BB), DeferredLoads);
    if (Result)
      Updater.AddAvailableValue(BB, Result);
  });

  // Second pass: process deferred loads
  forEachWorkListItem(DeferredLoads, [&](Instruction *I) {
    SmallVector<LoadInst *, 0> NewDLs;
    BasicBlock *BB = I->getParent();
    Value *Result = promoteAllocaUserToVector(
        I, *DL, VectorTy, VecStoreSize, ElementSize, GEPVectorIdx,
        Updater.GetValueInMiddleOfBlock(BB), NewDLs);
    if (Result)
      Updater.AddAvailableValue(BB, Result);
    assert(NewDLs.empty() && "No more deferred loads!");
  });

  // Delete instructions
  DenseSet<Instruction *> InstsToDelete(WorkList.begin(), WorkList.end());
  InstsToDelete.insert(DeferredLoads.begin(), DeferredLoads.end());
  for (Instruction *I : InstsToDelete) {
    assert(I->use_empty());
    I->eraseFromParent();
  }

  for (Instruction *I : reverse(UsersToRemove)) {
    I->dropDroppableUses();
    assert(I->use_empty());
    I->eraseFromParent();
  }

  assert(Alloca.use_empty());
  Alloca.eraseFromParent();

  return true;
}


FunctionPass *llvm::createVentusPromoteAllocaPass() {
  return new VentusPromoteAlloca();
}
