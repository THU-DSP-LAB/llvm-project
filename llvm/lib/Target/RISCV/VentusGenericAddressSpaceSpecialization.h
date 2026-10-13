//===-- VentusGenericAddressSpaceSpecialization.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_VENTUSGENERICADDRESSSPACESPECIALIZATION_H
#define LLVM_LIB_TARGET_RISCV_VENTUSGENERICADDRESSSPACESPECIALIZATION_H

#include "RISCV.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"

namespace llvm {
namespace ventus {

static constexpr unsigned ASFlat = RISCVAS::FLAT_ADDRESS;
static constexpr unsigned ASPrivate = RISCVAS::PRIVATE_ADDRESS;

class VentusGenericAddressSpaceSpecialization : public ModulePass {
public:
  static char ID;

  VentusGenericAddressSpaceSpecialization() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "Ventus Generic Address Space Specialization";
  }

  bool runOnModule(Module &M) override;

  Function *getOrCreateSpecializedFunction(Function &Callee,
                                           ArrayRef<unsigned> ParamAS);

private:
  StringMap<Function *> CloneCache;
  SmallPtrSet<Function *, 8> InProgressSpecializations;

  bool isEnabledForModule(Module &M) const;
  bool narrowFunction(Function &F);
  void verifyNoUnresolvedPrivateGenericFlow(Module &M);
};

class NarrowingContext {
public:
  NarrowingContext(VentusGenericAddressSpaceSpecialization &Pass, Function &F)
      : Pass(Pass), F(F) {}

  bool run();

private:
  VentusGenericAddressSpaceSpecialization &Pass;
  Function &F;
  DenseMap<Value *, Value *> NarrowValues;
  SmallVector<Instruction *, 16> DeadInsts;
  SmallPtrSet<Instruction *, 16> RewrittenInsts;
  SmallPtrSet<Value *, 16> ProcessingValues;
  SmallPtrSet<Value *, 16> ProcessedValues;
  bool Changed = false;

  bool processPrivateToFlatCast(AddrSpaceCastInst &ASC);
  bool processNarrowValueUsers(Value *FlatV);
  Value *getOrCreateNarrow(Value *V, Instruction *InsertBefore);
  void replacePointerOperand(Instruction &I, Value *OldPtr, Value *NewPtr);
  bool rewritePointerCompare(ICmpInst &Cmp, Value *Source);
  bool rewriteCall(CallBase &CB, Value *OldPtr, Value *NewPtr);
  bool rewriteMemIntrinsic(MemIntrinsic &MI, Value *OldPtr, Value *NewPtr);
  bool isKnownPrivateDerived(Value *V);
  bool isKnownPrivateDerived(Value *V, SmallPtrSetImpl<Value *> &Visiting);
  [[noreturn]] void diagnose(Value &V, StringRef Reason);
};

bool isFlatPointer(Type *Ty);
bool isPrivatePointer(Type *Ty);
bool isPrivateToFlatCast(Value *V);
bool hasABISensitivePointerAttrs(AttributeSet Attrs);
bool isDroppableIntrinsicUser(User *U);
bool isSupportedVAListIntrinsicUser(User *U, Value *Source);
std::string describeValue(Value &V);
[[noreturn]] void fatal(Value &V, Twine Reason);

} // namespace ventus
} // namespace llvm

#endif
