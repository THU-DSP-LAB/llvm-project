//===-- VentusRegisterDomain.h - Ventus register domains --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_VENTUSREGISTERDOMAIN_H
#define LLVM_LIB_TARGET_RISCV_VENTUSREGISTERDOMAIN_H

#include "llvm/CodeGen/ValueTypes.h"

namespace llvm {

class RISCVSubtarget;
class TargetRegisterClass;

enum class VentusRegDomain {
  Uniform,
  Divergent,
};

bool isVentusSGPRClass(const TargetRegisterClass *RC);
bool isVentusVGPRClass(const TargetRegisterClass *RC);
bool isGPRLikeScalarClass(const TargetRegisterClass *RC);

const TargetRegisterClass *getRegClassForDomain(MVT VT,
                                                VentusRegDomain Domain,
                                                const RISCVSubtarget &ST);

bool isLegalCopyDirection(const TargetRegisterClass *DstRC,
                          const TargetRegisterClass *SrcRC);
bool requiresExplicitBroadcast(const TargetRegisterClass *DstRC,
                               const TargetRegisterClass *SrcRC);

} // namespace llvm

#endif
