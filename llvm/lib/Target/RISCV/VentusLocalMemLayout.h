//===-- VentusLocalMemLayout.h - Ventus local memory layout -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_VENTUSLOCALMEMLAYOUT_H
#define LLVM_LIB_TARGET_RISCV_VENTUSLOCALMEMLAYOUT_H

#include "llvm/ADT/DenseMap.h"
#include <cstdint>

namespace llvm {

class DataLayout;
class GlobalVariable;
class Module;

DenseMap<const GlobalVariable *, uint64_t>
computeVentusLocalMemStaticOffsets(const Module &M, const DataLayout &DL);

uint64_t getVentusLocalMemStaticOffset(const GlobalVariable &GV,
                                       const DataLayout &DL);

uint64_t getVentusLocalMemStaticEndOffset(const GlobalVariable &GV,
                                          const DataLayout &DL);

uint64_t getVentusLocalMemStaticAlignValue(const GlobalVariable &GV,
                                           const DataLayout &DL);

} // namespace llvm

#endif
