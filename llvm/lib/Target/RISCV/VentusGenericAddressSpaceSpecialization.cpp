//===-- VentusGenericAddressSpaceSpecialization.cpp ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// OpenCL 2.0 generic pointer parameters erase addrspace(5) private provenance
// at helper call boundaries. Ventus private pointers are raw private stack
// offsets, so letting them reach flat addrspace(0) memory operations silently
// selects ordinary global/local addressing and miscompiles. This pass rewrites
// statically provable private-derived generic flows back to addrspace(5), and
// rejects unresolved flows before code generation.
//
//===----------------------------------------------------------------------===//

#include "VentusGenericAddressSpaceSpecialization.h"
#include "RISCVTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;
using namespace llvm::ventus;

#define DEBUG_TYPE "ventus-generic-as-specialization"

static cl::opt<bool> DisableVentusGenericASSpecialization(
    "disable-ventus-generic-as-specialization",
    cl::desc("Disable Ventus private-to-generic address space specialization"),
    cl::init(false), cl::Hidden);

bool llvm::ventus::isFlatPointer(Type *Ty) {
  return Ty->isPointerTy() && Ty->getPointerAddressSpace() == ASFlat;
}

bool llvm::ventus::isPrivatePointer(Type *Ty) {
  return Ty->isPointerTy() && Ty->getPointerAddressSpace() == ASPrivate;
}

bool llvm::ventus::isPrivateToFlatCast(Value *V) {
  auto *ASC = dyn_cast<AddrSpaceCastInst>(V);
  return ASC && ASC->getSrcAddressSpace() == ASPrivate &&
         ASC->getDestAddressSpace() == ASFlat;
}

std::string llvm::ventus::describeValue(Value &V) {
  std::string S;
  raw_string_ostream OS(S);
  V.print(OS);
  return OS.str();
}

[[noreturn]] void llvm::ventus::fatal(Value &V, Twine Reason) {
  report_fatal_error(
      Twine("Ventus generic address space specialization failed: ") + Reason +
          "\n  value: " + describeValue(V),
      false);
}

bool llvm::ventus::isDroppableIntrinsicUser(User *U) {
  if (auto *II = dyn_cast<IntrinsicInst>(U)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::lifetime_start:
    case Intrinsic::lifetime_end:
    case Intrinsic::invariant_start:
    case Intrinsic::invariant_end:
    case Intrinsic::dbg_declare:
    case Intrinsic::dbg_value:
    case Intrinsic::dbg_assign:
      return true;
    default:
      return false;
    }
  }
  return false;
}

bool llvm::ventus::isSupportedVAListIntrinsicUser(User *U, Value *Source) {
  auto *II = dyn_cast<IntrinsicInst>(U);
  if (!II)
    return false;

  switch (II->getIntrinsicID()) {
  case Intrinsic::vastart:
  case Intrinsic::vaend:
    return II->getArgOperand(0) == Source;
  default:
    return false;
  }
}

static bool isVentusKernel(const Function &F) {
  return F.getCallingConv() == CallingConv::VENTUS_KERNEL ||
         F.getCallingConv() == CallingConv::SPIR_KERNEL;
}

bool llvm::ventus::hasABISensitivePointerAttrs(AttributeSet Attrs) {
  return Attrs.hasAttribute(Attribute::ByVal) ||
         Attrs.hasAttribute(Attribute::ByRef) ||
         Attrs.hasAttribute(Attribute::StructRet) ||
         Attrs.hasAttribute(Attribute::InAlloca) ||
         Attrs.hasAttribute(Attribute::Preallocated);
}

static std::string makeSpecKey(Function &F, ArrayRef<unsigned> ParamAS) {
  std::string Key = F.getName().str();
  for (unsigned I = 0, E = ParamAS.size(); I != E; ++I) {
    if (ParamAS[I] == ASFlat)
      continue;
    Key += "|";
    Key += utostr(I);
    Key += "=";
    Key += utostr(ParamAS[I]);
  }
  return Key;
}

static std::string makeCloneName(Function &F, ArrayRef<unsigned> ParamAS) {
  std::string Name = F.getName().str();
  for (unsigned I = 0, E = ParamAS.size(); I != E; ++I) {
    if (ParamAS[I] == ASFlat)
      continue;
    Name += ".ventus.as";
    Name += utostr(ParamAS[I]);
    Name += "_p";
    Name += utostr(I);
  }
  return Name;
}

static GlobalValue::LinkageTypes getSpecializedCloneLinkage(Function &F) {
  if (F.hasAvailableExternallyLinkage())
    return GlobalValue::InternalLinkage;
  return F.getLinkage();
}

bool VentusGenericAddressSpaceSpecialization::isEnabledForModule(
    Module &M) const {
  if (DisableVentusGenericASSpecialization)
    return false;

  if (auto *TPC = getAnalysisIfAvailable<TargetPassConfig>()) {
    const TargetMachine &TM = TPC->getTM<TargetMachine>();
    if (TM.getTargetCPU() != "ventus-gpgpu")
      return false;
    return true;
  }

  Triple TT(M.getTargetTriple());
  return TT.isRISCV();
}

bool VentusGenericAddressSpaceSpecialization::runOnModule(Module &M) {
  if (!isEnabledForModule(M))
    return false;

  bool Changed = false;
  bool RoundChanged = false;
  do {
    RoundChanged = false;
    SmallVector<Function *, 16> Worklist;
    for (Function &F : M)
      if (!F.isDeclaration())
        Worklist.push_back(&F);

    for (Function *F : Worklist)
      RoundChanged |= narrowFunction(*F);

    Changed |= RoundChanged;
  } while (RoundChanged);

  verifyNoUnresolvedPrivateGenericFlow(M);
  return Changed;
}

bool VentusGenericAddressSpaceSpecialization::narrowFunction(Function &F) {
  return NarrowingContext(*this, F).run();
}

Function *
VentusGenericAddressSpaceSpecialization::getOrCreateSpecializedFunction(
    Function &Callee, ArrayRef<unsigned> ParamAS) {
  assert(ParamAS.size() == Callee.arg_size());
  std::string Key = makeSpecKey(Callee, ParamAS);
  auto Cached = CloneCache.find(Key);
  if (Cached != CloneCache.end())
    return Cached->second;

  if (Callee.isDeclaration())
    fatal(Callee, "private-derived generic pointer passed to external call");
  if (Callee.isVarArg())
    fatal(Callee, "private-derived generic pointer passed to variadic call");
  if (isVentusKernel(Callee))
    fatal(Callee,
          "kernel entry points are not cloned for pointer specialization");
  if (!InProgressSpecializations.insert(&Callee).second)
    fatal(Callee, "recursive private-derived generic helper specialization");
  auto RemoveInProgress =
      make_scope_exit([&] { InProgressSpecializations.erase(&Callee); });

  FunctionType *OldFTy = Callee.getFunctionType();
  SmallVector<Type *, 16> NewParamTys;
  NewParamTys.reserve(OldFTy->getNumParams());

  unsigned ArgNo = 0;
  for (Type *ParamTy : OldFTy->params()) {
    if (ParamAS[ArgNo] == ASPrivate) {
      if (!isFlatPointer(ParamTy))
        fatal(Callee, "only flat pointer helper parameters can be specialized");
      if (hasABISensitivePointerAttrs(
              Callee.getAttributes().getParamAttrs(ArgNo)))
        fatal(Callee, "ABI-sensitive pointer attribute on specialized helper "
                      "parameter");
      NewParamTys.push_back(PointerType::get(Callee.getContext(), ASPrivate));
    } else {
      NewParamTys.push_back(ParamTy);
    }
    ++ArgNo;
  }

  FunctionType *NewFTy = FunctionType::get(OldFTy->getReturnType(), NewParamTys,
                                           OldFTy->isVarArg());
  Function *Clone =
      Function::Create(NewFTy, getSpecializedCloneLinkage(Callee),
                       Callee.getAddressSpace(), makeCloneName(Callee, ParamAS),
                       Callee.getParent());
  Clone->copyAttributesFrom(&Callee);
  Clone->setCallingConv(Callee.getCallingConv());
  Clone->setDSOLocal(Callee.isDSOLocal());

  ValueToValueMapTy VMap;
  auto NewArgIt = Clone->arg_begin();
  for (Argument &OldArg : Callee.args()) {
    Argument *NewArg = &*NewArgIt++;
    NewArg->setName(OldArg.getName());
    if (ParamAS[OldArg.getArgNo()] == ASPrivate) {
      auto *Bridge = new AddrSpaceCastInst(NewArg, OldArg.getType(),
                                           NewArg->getName() + ".flat");
      VMap[&OldArg] = Bridge;
    } else {
      VMap[&OldArg] = NewArg;
    }
  }

  SmallVector<ReturnInst *, 8> Returns;
  CloneFunctionInto(Clone, &Callee, VMap,
                    CloneFunctionChangeType::LocalChangesOnly, Returns);

  for (Argument &OldArg : Callee.args()) {
    if (ParamAS[OldArg.getArgNo()] == ASFlat)
      continue;
    auto *Bridge = cast<Instruction>(static_cast<Value *>(VMap[&OldArg]));
    Bridge->insertBefore(
        &*Clone->getEntryBlock().getFirstNonPHIOrDbgOrAlloca());
  }

  CloneCache[Key] = Clone;
  narrowFunction(*Clone);
  return Clone;
}

static bool isResolvedUse(User *U, Value *Source) {
  if (isDroppableIntrinsicUser(U))
    return true;
  if (isSupportedVAListIntrinsicUser(U, Source))
    return true;
  if (auto *I = dyn_cast<Instruction>(U)) {
    if (auto *ASC = dyn_cast<AddrSpaceCastInst>(I))
      return ASC->getDestAddressSpace() == ASPrivate;
    if (auto *GEP = dyn_cast<GetElementPtrInst>(I))
      return GEP->getType()->getPointerAddressSpace() == ASPrivate;
    if (auto *LI = dyn_cast<LoadInst>(I))
      return LI->getPointerOperand() == Source &&
             LI->getPointerAddressSpace() == ASPrivate;
    if (auto *SI = dyn_cast<StoreInst>(I))
      return SI->getPointerOperand() == Source &&
             SI->getPointerAddressSpace() == ASPrivate;
    if (auto *MI = dyn_cast<MemIntrinsic>(I))
      return MI->getDestAddressSpace() == ASPrivate ||
             (isa<MemTransferInst>(MI) &&
              cast<MemTransferInst>(MI)->getSourceAddressSpace() == ASPrivate);
  }
  return false;
}

void VentusGenericAddressSpaceSpecialization::
    verifyNoUnresolvedPrivateGenericFlow(Module &M) {
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    for (Instruction &I : instructions(F)) {
      if (!isPrivateToFlatCast(&I))
        continue;

      for (User *U : I.users()) {
        if (!isResolvedUse(U, &I))
          fatal(*U, "unresolved private-derived generic pointer flow remains");
      }
    }
  }
}

char VentusGenericAddressSpaceSpecialization::ID = 0;

INITIALIZE_PASS(VentusGenericAddressSpaceSpecialization, DEBUG_TYPE,
                "Ventus generic address space specialization", false, false)

char &llvm::VentusGenericAddressSpaceSpecializationID =
    VentusGenericAddressSpaceSpecialization::ID;

ModulePass *llvm::createVentusGenericAddressSpaceSpecializationPass() {
  return new VentusGenericAddressSpaceSpecialization();
}
