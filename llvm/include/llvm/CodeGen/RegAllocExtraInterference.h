//===- RegAllocExtraInterference.h - Target RA interference -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_REGALLOCEXTRAINTERFERENCE_H
#define LLVM_CODEGEN_REGALLOCEXTRAINTERFERENCE_H

#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/MC/MCRegister.h"

namespace llvm {

class LiveInterval;
class LiveIntervals;
class MachineFunction;
class VirtRegMap;

/// Target-provided register allocation interference that is not represented by
/// ordinary Machine CFG liveness.
class RegAllocExtraInterference {
public:
  virtual ~RegAllocExtraInterference() = default;

  virtual void analyze(MachineFunction &MF, LiveIntervals &LIS, VirtRegMap &VRM,
                       LiveRegMatrix &Matrix) = 0;

  virtual LiveRegMatrix::InterferenceKind
  checkInterference(const LiveInterval &VirtReg, MCRegister PhysReg,
                    LiveRegMatrix &Matrix) = 0;

  virtual void assign(const LiveInterval &VirtReg, MCRegister PhysReg,
                      LiveRegMatrix &Matrix) = 0;

  virtual void unassign(const LiveInterval &VirtReg, MCRegister PhysReg,
                        LiveRegMatrix &Matrix) = 0;

  virtual void invalidateVirtRegs() = 0;
};

} // end namespace llvm

#endif // LLVM_CODEGEN_REGALLOCEXTRAINTERFERENCE_H
