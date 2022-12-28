//===-- VentusCallingConv.td - Calling Conventions RISCV ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Ventus GPGPU architecture.
//
//===----------------------------------------------------------------------===//

// The Ventus GPGPU calling convention is handled with custom code in
// RISCVISelLowering.cpp (CC_Ventus).

def CSR_ILP32_LP64
    : CalleeSavedRegs<(add X1, X3, X8, X9, (sequence "X%u", 18, 27),
                      (sequence "V%u", 32, 47))>;

def CSR_ILP32F_LP64F
    : CalleeSavedRegs<(add CSR_ILP32_LP64,
                       F8_F, F9_F, (sequence "F%u_F", 18, 27))>;

def CSR_ILP32D_LP64D
    : CalleeSavedRegs<(add CSR_ILP32_LP64,
                       F8_D, F9_D, (sequence "F%u_D", 18, 27))>;

// Needed for implementation of RISCVRegisterInfo::getNoPreservedMask()
def CSR_NoRegs : CalleeSavedRegs<(add)>;
