//===-- VentusAlwaysInlinePass.cpp - Force Function Inlining --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This pass marks functions for inlining in Ventus code. Specifically:
/// 1. Functions accessing LOCAL memory (addrspace(3)) are marked as always_inline
/// 2. Under stress-calls mode, non-kernel functions are marked as noinline
/// 3. Otherwise, non-kernel functions are marked as always_inline
/// 4. Function aliases are replaced with their targets and optionally removed
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVSubtarget.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define VENTUS_ALWAYS_INLINE "Ventus Inline All Functions"
#define DEBUG_TYPE "ventus-always-inline"

namespace {

static cl::opt<bool> StressCalls("ventus-stress-function-calls", cl::Hidden,
                                 cl::desc("Force all functions to be noinline"),
                                 cl::init(false));

class VentusAlwaysInline : public ModulePass {
  bool GlobalOpt;

public:
  static char ID;

  VentusAlwaysInline(bool GlobalOpt = false)
      : ModulePass(ID), GlobalOpt(GlobalOpt) {}

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

  StringRef getPassName() const override { return VENTUS_ALWAYS_INLINE; }
};

} // End anonymous namespace

char VentusAlwaysInline::ID = 0;

static void
recursivelyVisitUsers(GlobalValue &GV,
                      SmallPtrSetImpl<Function *> &FuncsToAlwaysInline) {
  SmallVector<User *, 16> Stack(GV.users());
  SmallPtrSet<const Value *, 8> Visited;

  while (!Stack.empty()) {
    User *U = Stack.pop_back_val();
    if (!Visited.insert(U).second)
      continue;

    if (Instruction *I = dyn_cast<Instruction>(U)) {
      Function *F = I->getParent()->getParent();
      if (F->getCallingConv() != CallingConv::VENTUS_KERNEL) {
        F->removeFnAttr(Attribute::NoInline);
        FuncsToAlwaysInline.insert(F);
        Stack.push_back(F);
      }
      continue;
    }

    append_range(Stack, U->users());
  }
}

static bool alwaysInlineImpl(Module &M, bool GlobalOpt) {
  std::vector<GlobalAlias *> AliasesToRemove;

  SmallPtrSet<Function *, 8> FuncsToAlwaysInline;
  SmallPtrSet<Function *, 8> FuncsToNoInline;

  Triple TT(M.getTargetTriple());

  for (GlobalAlias &A : M.aliases()) {
    if (Function *F = dyn_cast<Function>(A.getAliasee())) {
      if (TT.getArch() == Triple::riscv32 &&
          A.getLinkage() != GlobalValue::InternalLinkage)
        continue;
      A.replaceAllUsesWith(F);
      AliasesToRemove.push_back(&A);
    }
    // FIXME: If the aliasee isn't a function, it's some kind of constant expr
    // cast that won't be inlined through.
  }

  if (GlobalOpt) {
    for (GlobalAlias *A : AliasesToRemove) {
      A->eraseFromParent();
    }
  }

  for (GlobalVariable &GV : M.globals()) {
    unsigned AS = GV.getAddressSpace();
    if (AS == RISCVAS::LOCAL_ADDRESS) {
      recursivelyVisitUsers(GV, FuncsToAlwaysInline);
    }
  }

  auto IncompatAttr =
      StressCalls ? Attribute::AlwaysInline : Attribute::NoInline;

  for (Function &F : M) {
    if (!F.isDeclaration() && !F.use_empty() &&
        !F.hasFnAttribute(IncompatAttr) &&
        F.getCallingConv() != CallingConv::VENTUS_KERNEL) {
      if (StressCalls) {
        if (!FuncsToAlwaysInline.count(&F))
          FuncsToNoInline.insert(&F);
      } else
        FuncsToAlwaysInline.insert(&F);
    }
  }

  for (Function *F : FuncsToAlwaysInline)
    F->addFnAttr(Attribute::AlwaysInline);

  for (Function *F : FuncsToNoInline)
    F->addFnAttr(Attribute::NoInline);

  return !FuncsToAlwaysInline.empty() || !FuncsToNoInline.empty();
}

bool VentusAlwaysInline::runOnModule(Module &M) {
  return alwaysInlineImpl(M, GlobalOpt);
}

INITIALIZE_PASS(VentusAlwaysInline, "ventus-always-inline",
                VENTUS_ALWAYS_INLINE, false, false)

namespace llvm {
ModulePass *createVentusAlwaysInlinePass(bool GlobalOpt) {
  return new VentusAlwaysInline(GlobalOpt);
}

PreservedAnalyses VentusAlwaysInlinePass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  alwaysInlineImpl(M, GlobalOpt);
  return PreservedAnalyses::all();
}
} // end namespace llvm
