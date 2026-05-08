//===-- VentusSIMTInterference.h - SIMT RA interference --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_VENTUSSIMTINTERFERENCE_H
#define LLVM_LIB_TARGET_RISCV_VENTUSSIMTINTERFERENCE_H

#include "llvm/CodeGen/RegAllocExtraInterference.h"
#include <memory>

namespace llvm {

std::unique_ptr<RegAllocExtraInterference> createVentusSIMTInterference();

} // end namespace llvm

#endif // LLVM_LIB_TARGET_RISCV_VENTUSSIMTINTERFERENCE_H
