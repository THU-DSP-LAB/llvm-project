//===-- VentusMMAUtils.h - Ventus MMA utilities ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_VENTUSMMAUTILS_H
#define LLVM_LIB_TARGET_RISCV_VENTUSMMAUTILS_H

#include "llvm/CodeGen/MachineBasicBlock.h"

namespace llvm {

class RISCVInstrInfo;

bool hasVentusDedicatedRegextHandling(const RISCVInstrInfo &TII,
                                      unsigned Opcode);

bool expandVentusCustomPseudo(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              const RISCVInstrInfo &TII);

} // namespace llvm

#endif // LLVM_LIB_TARGET_RISCV_VENTUSMMAUTILS_H
