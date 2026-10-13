//===-- VentusGenericAddressSpaceNarrowing.cpp ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VentusGenericAddressSpaceSpecialization.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicsRISCV.h"

using namespace llvm;
using namespace llvm::ventus;

static void copyCallProperties(CallBase &NewCB, const CallBase &OldCB) {
  NewCB.setCallingConv(OldCB.getCallingConv());
  NewCB.setAttributes(OldCB.getAttributes());
  cast<CallInst>(NewCB).setTailCallKind(
      cast<CallInst>(OldCB).getTailCallKind());
  NewCB.setDebugLoc(OldCB.getDebugLoc());
  NewCB.copyMetadata(OldCB);
}

static void eraseDeadInstructions(SmallVectorImpl<Instruction *> &DeadInsts) {
  SmallPtrSet<Instruction *, 16> DeadSet;
  for (Instruction *I : DeadInsts)
    if (I)
      DeadSet.insert(I);

  bool Pruned = false;
  do {
    Pruned = false;
    SmallVector<Instruction *, 16> Candidates;
    for (Instruction *I : DeadSet)
      Candidates.push_back(I);
    for (Instruction *I : Candidates) {
      bool HasLiveUser = false;
      for (User *U : I->users()) {
        auto *UserI = dyn_cast<Instruction>(U);
        if (UserI && DeadSet.contains(UserI))
          continue;
        HasLiveUser = true;
        break;
      }
      if (!HasLiveUser)
        continue;
      DeadSet.erase(I);
      Pruned = true;
    }
  } while (Pruned);

  SmallVector<Instruction *, 16> ToErase;
  for (Instruction *I : DeadSet)
    ToErase.push_back(I);

  for (Instruction *I : ToErase)
    I->dropAllReferences();
  for (Instruction *I : reverse(ToErase)) {
    if (!I->getParent())
      continue;
    I->eraseFromParent();
  }
  DeadInsts.clear();
}

bool NarrowingContext::run() {
  SmallVector<AddrSpaceCastInst *, 16> Casts;
  for (Instruction &I : instructions(F))
    if (auto *ASC = dyn_cast<AddrSpaceCastInst>(&I))
      if (ASC->getSrcAddressSpace() == ASPrivate &&
          ASC->getDestAddressSpace() == ASFlat)
        Casts.push_back(ASC);

  for (AddrSpaceCastInst *ASC : Casts)
    Changed |= processPrivateToFlatCast(*ASC);

  eraseDeadInstructions(DeadInsts);
  return Changed;
}

bool NarrowingContext::processPrivateToFlatCast(AddrSpaceCastInst &ASC) {
  NarrowValues[&ASC] = ASC.getPointerOperand();
  bool LocalChanged = processNarrowValueUsers(&ASC);
  if (ASC.use_empty() || LocalChanged)
    DeadInsts.push_back(&ASC);
  return LocalChanged;
}

bool NarrowingContext::processNarrowValueUsers(Value *FlatV) {
  if (ProcessedValues.contains(FlatV))
    return false;
  if (!ProcessingValues.insert(FlatV).second)
    return false;

  bool LocalChanged = false;
  SmallVector<User *, 16> Users(FlatV->user_begin(), FlatV->user_end());

  for (User *U : Users) {
    if (isDroppableIntrinsicUser(U)) {
      if (auto *I = dyn_cast<Instruction>(U)) {
        DeadInsts.push_back(I);
        LocalChanged = true;
      }
      continue;
    }

    if (isSupportedVAListIntrinsicUser(U, FlatV))
      continue;

    auto *I = dyn_cast<Instruction>(U);
    if (!I)
      diagnose(
          *FlatV,
          "private-derived generic pointer escapes through a constant user");

    Value *Narrow = getOrCreateNarrow(FlatV, I);

    if (auto *GEP = dyn_cast<GetElementPtrInst>(I)) {
      (void)getOrCreateNarrow(GEP, GEP);
      LocalChanged |= processNarrowValueUsers(GEP);
      DeadInsts.push_back(GEP);
      continue;
    }

    if (auto *LI = dyn_cast<LoadInst>(I)) {
      if (LI->getPointerOperand() != FlatV)
        diagnose(*LI, "private-derived generic pointer used as load value");
      replacePointerOperand(*LI, FlatV, Narrow);
      LocalChanged = true;
      continue;
    }

    if (auto *SI = dyn_cast<StoreInst>(I)) {
      if (SI->getValueOperand() == FlatV)
        diagnose(*SI, "private-derived generic pointer stored to memory");
      if (SI->getPointerOperand() == FlatV) {
        replacePointerOperand(*SI, FlatV, Narrow);
        LocalChanged = true;
        continue;
      }
    }

    if (auto *MI = dyn_cast<MemIntrinsic>(I)) {
      if (RewrittenInsts.contains(MI))
        continue;
      LocalChanged |= rewriteMemIntrinsic(*MI, FlatV, Narrow);
      continue;
    }

    if (auto *CB = dyn_cast<CallBase>(I)) {
      if (RewrittenInsts.contains(CB))
        continue;
      LocalChanged |= rewriteCall(*CB, FlatV, Narrow);
      continue;
    }

    if (auto *PN = dyn_cast<PHINode>(I)) {
      (void)getOrCreateNarrow(PN, PN);
      LocalChanged |= processNarrowValueUsers(PN);
      DeadInsts.push_back(PN);
      continue;
    }

    if (auto *Sel = dyn_cast<SelectInst>(I)) {
      (void)getOrCreateNarrow(Sel, Sel);
      LocalChanged |= processNarrowValueUsers(Sel);
      DeadInsts.push_back(Sel);
      continue;
    }

    if (auto *Cmp = dyn_cast<ICmpInst>(I)) {
      if (Cmp->getOperand(0) != FlatV && Cmp->getOperand(1) != FlatV)
        continue;
      LocalChanged |= rewritePointerCompare(*Cmp, FlatV);
      continue;
    }

    if (auto *ASC = dyn_cast<AddrSpaceCastInst>(I)) {
      if (ASC->getDestAddressSpace() == ASPrivate) {
        ASC->replaceAllUsesWith(Narrow);
        DeadInsts.push_back(ASC);
        LocalChanged = true;
        continue;
      }
      diagnose(*ASC, "unsupported private-derived generic pointer cast");
    }

    if (isa<BitCastInst>(I))
      diagnose(*I, "unsupported private-derived generic pointer cast");
    if (isa<PtrToIntInst>(I) || isa<IntToPtrInst>(I))
      diagnose(*I, "private-derived generic pointer converted through integer");
    if (isa<ReturnInst>(I))
      diagnose(*I, "private-derived generic pointer returned");

    diagnose(*I, "unsupported private-derived generic pointer use");
  }

  ProcessingValues.erase(FlatV);
  ProcessedValues.insert(FlatV);
  return LocalChanged;
}

Value *NarrowingContext::getOrCreateNarrow(Value *V,
                                           Instruction *InsertBefore) {
  auto It = NarrowValues.find(V);
  if (It != NarrowValues.end())
    return It->second;

  if (isPrivatePointer(V->getType()))
    return V;

  if (auto *ASC = dyn_cast<AddrSpaceCastInst>(V)) {
    if (ASC->getSrcAddressSpace() == ASPrivate &&
        ASC->getDestAddressSpace() == ASFlat) {
      Value *Narrow = ASC->getPointerOperand();
      NarrowValues[V] = Narrow;
      return Narrow;
    }
  }

  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    Value *Base = getOrCreateNarrow(GEP->getPointerOperand(), GEP);
    IRBuilder<> B(GEP);
    SmallVector<Value *, 8> Indices(GEP->indices());
    auto *NewGEP = cast<GetElementPtrInst>(B.CreateGEP(
        GEP->getSourceElementType(), Base, Indices, GEP->getName() + ".as5"));
    NewGEP->setIsInBounds(GEP->isInBounds());
    NewGEP->copyMetadata(*GEP);
    NarrowValues[V] = NewGEP;
    Changed = true;
    return NewGEP;
  }

  if (auto *PN = dyn_cast<PHINode>(V)) {
    auto *NewPN =
        PHINode::Create(PointerType::get(F.getContext(), ASPrivate),
                        PN->getNumIncomingValues(), PN->getName() + ".as5", PN);
    NarrowValues[V] = NewPN;
    for (unsigned I = 0, E = PN->getNumIncomingValues(); I != E; ++I) {
      Value *Incoming = PN->getIncomingValue(I);
      if (!isKnownPrivateDerived(Incoming))
        diagnose(*PN, "mixed-AS phi for private-derived generic pointer");
      NewPN->addIncoming(getOrCreateNarrow(Incoming, PN),
                         PN->getIncomingBlock(I));
    }
    NewPN->copyMetadata(*PN);
    Changed = true;
    return NewPN;
  }

  if (auto *Sel = dyn_cast<SelectInst>(V)) {
    Value *TrueV = Sel->getTrueValue();
    Value *FalseV = Sel->getFalseValue();
    if (!isKnownPrivateDerived(TrueV) || !isKnownPrivateDerived(FalseV))
      diagnose(*Sel, "mixed-AS select for private-derived generic pointer");
    IRBuilder<> B(Sel);
    Value *NewSel =
        B.CreateSelect(Sel->getCondition(), getOrCreateNarrow(TrueV, Sel),
                       getOrCreateNarrow(FalseV, Sel), Sel->getName() + ".as5");
    if (auto *NewSelI = dyn_cast<Instruction>(NewSel))
      NewSelI->copyMetadata(*Sel);
    NarrowValues[V] = NewSel;
    Changed = true;
    return NewSel;
  }

  diagnose(*V, "cannot construct private addrspace equivalent");
}

void NarrowingContext::replacePointerOperand(Instruction &I, Value *OldPtr,
                                             Value *NewPtr) {
  for (Use &U : I.operands())
    if (U.get() == OldPtr) {
      U.set(NewPtr);
      return;
    }
  llvm_unreachable("old pointer operand not found");
}

bool NarrowingContext::rewritePointerCompare(ICmpInst &Cmp, Value *Source) {
  Value *OldLHS = Cmp.getOperand(0);
  Value *OldRHS = Cmp.getOperand(1);
  Type *PrivatePtrTy = PointerType::get(F.getContext(), ASPrivate);
  auto narrowCompareOperand = [&](Value *Operand) -> Value * {
    if (Operand == Source || isKnownPrivateDerived(Operand))
      return getOrCreateNarrow(Operand, &Cmp);
    if (isa<ConstantPointerNull>(Operand))
      return ConstantPointerNull::get(cast<PointerType>(PrivatePtrTy));
    diagnose(Cmp, "mixed-AS compare for private-derived generic pointer");
  };

  if (!isFlatPointer(OldLHS->getType()) || !isFlatPointer(OldRHS->getType()))
    diagnose(Cmp, "non-flat pointer compare for private-derived generic "
                  "pointer");

  Value *NewLHS = narrowCompareOperand(OldLHS);
  Value *NewRHS = narrowCompareOperand(OldRHS);
  Cmp.setOperand(0, NewLHS);
  Cmp.setOperand(1, NewRHS);
  return true;
}

bool NarrowingContext::rewriteCall(CallBase &CB, Value *OldPtr, Value *NewPtr) {
  if (isa<MemIntrinsic>(&CB))
    return rewriteMemIntrinsic(cast<MemIntrinsic>(CB), OldPtr, NewPtr);
  if (!isa<CallInst>(CB))
    diagnose(
        CB,
        "invoke/callbr cannot be rewritten for private pointer specialization");

  Function *Callee = CB.getCalledFunction();
  if (!Callee)
    diagnose(CB, "private-derived generic pointer passed to indirect call");
  if (!Callee->isVarArg() && Callee->getName() == "_Z17wait_group_eventsiP9ocl_event" &&
      Callee->getReturnType()->isVoidTy() && Callee->arg_size() == 2 &&
      Callee->getArg(0)->getType()->isIntegerTy(32) &&
      isFlatPointer(Callee->getArg(1)->getType())) {
    IRBuilder<> B(&CB);
    CallInst *NewCall = B.CreateIntrinsic(
        Intrinsic::riscv_ventus_barrier, {}, {B.getInt32(3)});
    NewCall->setDebugLoc(CB.getDebugLoc());
    NewCall->copyMetadata(CB);
    RewrittenInsts.insert(cast<Instruction>(&CB));
    DeadInsts.push_back(cast<Instruction>(&CB));
    Changed = true;
    return true;
  }
  if (Callee->isIntrinsic())
    diagnose(CB,
             "unsupported intrinsic consumes private-derived generic pointer");
  if (CB.isMustTailCall())
    diagnose(
        CB,
        "musttail call cannot be rewritten for private pointer specialization");

  SmallVector<unsigned, 16> ParamAS(Callee->arg_size(), ASFlat);
  for (unsigned I = 0, E = CB.arg_size(); I != E; ++I) {
    Value *Arg = CB.getArgOperand(I);
    if (!isKnownPrivateDerived(Arg))
      continue;
    if (!isFlatPointer(Arg->getType()))
      continue;
    if (I >= Callee->arg_size())
      diagnose(CB,
               "private-derived generic pointer passed to variadic argument");
    if (!isFlatPointer(Callee->getFunctionType()->getParamType(I)))
      diagnose(
          CB,
          "private-derived generic pointer passed to non-flat pointer "
          "parameter");
    if (hasABISensitivePointerAttrs(CB.getAttributes().getParamAttrs(I)))
      diagnose(CB, "ABI-sensitive pointer attribute on private-derived call "
                   "argument");
    ParamAS[I] = ASPrivate;
  }

  if (all_of(ParamAS, [](unsigned AS) { return AS == ASFlat; }))
    return false;

  Function *Clone = Pass.getOrCreateSpecializedFunction(*Callee, ParamAS);
  SmallVector<Value *, 16> Args;
  Args.reserve(CB.arg_size());
  for (unsigned I = 0, E = CB.arg_size(); I != E; ++I) {
    Value *Arg = CB.getArgOperand(I);
    if (I < ParamAS.size() && ParamAS[I] == ASPrivate) {
      Args.push_back(getOrCreateNarrow(Arg, &CB));
    } else {
      Args.push_back(Arg);
    }
  }

  IRBuilder<> B(&CB);
  SmallVector<OperandBundleDef, 1> OpBundles;
  CB.getOperandBundlesAsDefs(OpBundles);
  CallInst *NewCall =
      B.CreateCall(Clone->getFunctionType(), Clone, Args, OpBundles,
                   CB.getName());
  copyCallProperties(*NewCall, CB);
  if (!CB.use_empty())
    CB.replaceAllUsesWith(NewCall);
  RewrittenInsts.insert(cast<Instruction>(&CB));
  DeadInsts.push_back(cast<Instruction>(&CB));
  Changed = true;
  return true;
}

bool NarrowingContext::rewriteMemIntrinsic(MemIntrinsic &MI, Value *OldPtr,
                                           Value *NewPtr) {
  IRBuilder<> B(&MI);

  if (MI.getRawDest() == OldPtr) {
    if (auto *MSI = dyn_cast<MemSetInst>(&MI)) {
      CallInst *NewMI =
          B.CreateMemSet(NewPtr, MSI->getValue(), MSI->getLength(),
                         MSI->getDestAlign(), MSI->isVolatile());
      NewMI->copyMetadata(MI);
      RewrittenInsts.insert(&MI);
      DeadInsts.push_back(&MI);
      Changed = true;
      return true;
    }
    if (auto *MTI = dyn_cast<MemTransferInst>(&MI)) {
      Value *Src = MTI->getRawSource();
      if (isKnownPrivateDerived(Src))
        Src = getOrCreateNarrow(Src, &MI);
      CallInst *NewMI = nullptr;
      if (isa<MemCpyInst>(MTI)) {
        NewMI = B.CreateMemCpy(NewPtr, MTI->getDestAlign(), Src,
                               MTI->getSourceAlign(), MTI->getLength(),
                               MTI->isVolatile());
      } else if (isa<MemMoveInst>(MTI)) {
        NewMI = B.CreateMemMove(NewPtr, MTI->getDestAlign(), Src,
                                MTI->getSourceAlign(), MTI->getLength(),
                                MTI->isVolatile());
      }
      if (!NewMI)
        diagnose(MI, "unsupported memory transfer intrinsic");
      NewMI->copyMetadata(MI);
      RewrittenInsts.insert(&MI);
      DeadInsts.push_back(&MI);
      Changed = true;
      return true;
    }
  }

  if (auto *MTI = dyn_cast<MemTransferInst>(&MI)) {
    if (MTI->getRawSource() == OldPtr) {
      Value *Dst = MTI->getRawDest();
      if (isKnownPrivateDerived(Dst))
        Dst = getOrCreateNarrow(Dst, &MI);
      CallInst *NewMI = nullptr;
      if (isa<MemCpyInst>(MTI)) {
        NewMI = B.CreateMemCpy(Dst, MTI->getDestAlign(), NewPtr,
                               MTI->getSourceAlign(), MTI->getLength(),
                               MTI->isVolatile());
      } else if (isa<MemMoveInst>(MTI)) {
        NewMI = B.CreateMemMove(Dst, MTI->getDestAlign(), NewPtr,
                                MTI->getSourceAlign(), MTI->getLength(),
                                MTI->isVolatile());
      }
      if (!NewMI)
        diagnose(MI, "unsupported memory transfer intrinsic");
      NewMI->copyMetadata(MI);
      RewrittenInsts.insert(&MI);
      DeadInsts.push_back(&MI);
      Changed = true;
      return true;
    }
  }

  diagnose(MI, "private-derived generic pointer used by unsupported memory "
               "intrinsic operand");
}

bool NarrowingContext::isKnownPrivateDerived(Value *V) {
  SmallPtrSet<Value *, 16> Visiting;
  return isKnownPrivateDerived(V, Visiting);
}

bool NarrowingContext::isKnownPrivateDerived(
    Value *V, SmallPtrSetImpl<Value *> &Visiting) {
  if (isPrivatePointer(V->getType()))
    return true;
  if (NarrowValues.count(V))
    return true;
  if (isPrivateToFlatCast(V))
    return true;
  if (!Visiting.insert(V).second)
    return true;
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    bool Result = isKnownPrivateDerived(GEP->getPointerOperand(), Visiting);
    Visiting.erase(V);
    return Result;
  }
  if (auto *PN = dyn_cast<PHINode>(V)) {
    for (Value *Incoming : PN->incoming_values()) {
      if (!isKnownPrivateDerived(Incoming, Visiting)) {
        Visiting.erase(V);
        return false;
      }
    }
    Visiting.erase(V);
    return true;
  }
  if (auto *Sel = dyn_cast<SelectInst>(V)) {
    bool Result = isKnownPrivateDerived(Sel->getTrueValue(), Visiting) &&
                  isKnownPrivateDerived(Sel->getFalseValue(), Visiting);
    Visiting.erase(V);
    return Result;
  }
  Visiting.erase(V);
  return false;
}

[[noreturn]] void NarrowingContext::diagnose(Value &V, StringRef Reason) {
  fatal(V, Reason);
}
