//===-- VentusLocalMemLayout.cpp - Ventus local memory layout -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VentusLocalMemLayout.h"
#include "RISCV.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Alignment.h"

using namespace llvm;

static Align getVentusLocalMemStaticSizeAlign() { return Align(4); }

static Align getVentusLocalMemStaticAlign(const GlobalVariable &GV,
                                          const DataLayout &DL) {
  const Align ABIAlign = DL.getABITypeAlign(GV.getValueType());
  Align Alignment = ABIAlign.value() <= 4 ? Align(4) : ABIAlign;
  if (MaybeAlign ExplicitAlign = GV.getAlign())
    Alignment = std::max(Alignment, *ExplicitAlign);
  return Alignment;
}

uint64_t llvm::getVentusLocalMemStaticAlignValue(const GlobalVariable &GV,
                                                 const DataLayout &DL) {
  return getVentusLocalMemStaticAlign(GV, DL).value();
}

DenseMap<const GlobalVariable *, uint64_t>
llvm::computeVentusLocalMemStaticOffsets(const Module &M,
                                         const DataLayout &DL) {
  SmallVector<const GlobalVariable *, 8> LocalGlobals;
  for (const GlobalVariable &GV : M.globals()) {
    if (GV.getAddressSpace() != RISCVAS::LOCAL_ADDRESS ||
        GV.isDeclarationForLinker() || !GV.getValueType()->isSized())
      continue;
    LocalGlobals.push_back(&GV);
  }

  llvm::sort(LocalGlobals,
             [](const GlobalVariable *LHS, const GlobalVariable *RHS) {
               return LHS->getName() < RHS->getName();
             });

  DenseMap<const GlobalVariable *, uint64_t> Offsets;
  uint64_t Offset = 0;
  for (const GlobalVariable *GV : LocalGlobals) {
    Offset = alignTo(Offset, getVentusLocalMemStaticAlign(*GV, DL));
    Offsets[GV] = Offset;
    Offset += DL.getTypeAllocSize(GV->getValueType());
  }
  return Offsets;
}

uint64_t llvm::getVentusLocalMemStaticOffset(const GlobalVariable &GV,
                                             const DataLayout &DL) {
  const DenseMap<const GlobalVariable *, uint64_t> Offsets =
      computeVentusLocalMemStaticOffsets(*GV.getParent(), DL);
  auto It = Offsets.find(&GV);
  assert(It != Offsets.end() &&
         "requested local-memory static offset for a global that is not laid "
         "out in Ventus LDS static storage");
  return It->second;
}

uint64_t llvm::getVentusLocalMemStaticEndOffset(const GlobalVariable &GV,
                                                const DataLayout &DL) {
  const uint64_t Offset = getVentusLocalMemStaticOffset(GV, DL);
  return alignTo(Offset + DL.getTypeAllocSize(GV.getValueType()),
                 getVentusLocalMemStaticSizeAlign());
}
