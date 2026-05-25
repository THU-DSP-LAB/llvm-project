//===-- VentusRegisterDomain.cpp - Ventus register domains ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VentusRegisterDomain.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"

using namespace llvm;

bool llvm::isVentusSGPRClass(const TargetRegisterClass *RC) {
  return RC && RISCVRegisterInfo::isSGPRClass(RC);
}

bool llvm::isVentusVGPRClass(const TargetRegisterClass *RC) {
  return RC && RISCVRegisterInfo::isVGPRClass(RC);
}

bool llvm::isGPRLikeScalarClass(const TargetRegisterClass *RC) {
  return RC && (RISCV::GPRRegClass.hasSubClassEq(RC) ||
                RISCV::FPR16RegClass.hasSubClassEq(RC) ||
                RISCV::FPR32RegClass.hasSubClassEq(RC) ||
                RISCV::FPR64RegClass.hasSubClassEq(RC) ||
                RISCV::GPRF16RegClass.hasSubClassEq(RC) ||
                RISCV::GPRF32RegClass.hasSubClassEq(RC) ||
                RISCV::GPRF64RegClass.hasSubClassEq(RC) ||
                RC->hasSuperClassEq(&RISCV::GPRRegClass) ||
                RC->hasSuperClassEq(&RISCV::FPR16RegClass) ||
                RC->hasSuperClassEq(&RISCV::FPR32RegClass) ||
                RC->hasSuperClassEq(&RISCV::FPR64RegClass) ||
                RC->hasSuperClassEq(&RISCV::GPRF16RegClass) ||
                RC->hasSuperClassEq(&RISCV::GPRF32RegClass) ||
                RC->hasSuperClassEq(&RISCV::GPRF64RegClass));
}

const TargetRegisterClass *
llvm::getRegClassForDomain(MVT VT, VentusRegDomain Domain,
                           const RISCVSubtarget &ST) {
  if (!ST.isVentusGPGPU())
    return nullptr;

  if (Domain == VentusRegDomain::Divergent &&
      (VT == MVT::i32 || VT == MVT::f16 || VT == MVT::f32))
    return &RISCV::VGPRRegClass;

  if (VT == MVT::f16)
    return &RISCV::GPRF16RegClass;
  if (VT == MVT::f32)
    return &RISCV::GPRF32RegClass;
  if (VT == MVT::f64)
    return &RISCV::GPRF64RegClass;

  if (VT == MVT::i32)
    return &RISCV::GPRRegClass;

  return nullptr;
}

bool llvm::requiresExplicitBroadcast(const TargetRegisterClass *DstRC,
                                     const TargetRegisterClass *SrcRC) {
  return isVentusVGPRClass(DstRC) && isGPRLikeScalarClass(SrcRC);
}

bool llvm::isLegalCopyDirection(const TargetRegisterClass *DstRC,
                                const TargetRegisterClass *SrcRC) {
  if (!DstRC || !SrcRC)
    return true;

  if (isGPRLikeScalarClass(DstRC) && isVentusVGPRClass(SrcRC))
    return false;

  return true;
}
