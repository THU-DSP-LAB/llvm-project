//===-- RISCV.h - Top-level interface for RISCV -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// RISC-V back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCV_H
#define LLVM_LIB_TARGET_RISCV_RISCV_H

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class RISCVRegisterBankInfo;
class RISCVSubtarget;
class RISCVTargetMachine;
class AsmPrinter;
class FunctionPass;
class InstructionSelector;
class MCInst;
class MCOperand;
class MachineInstr;
class MachineOperand;
class PassRegistry;

FunctionPass *createRISCVCodeGenPreparePass();
void initializeRISCVCodeGenPreparePass(PassRegistry &);

bool lowerRISCVMachineInstrToMCInst(const MachineInstr *MI, MCInst &OutMI,
                                    AsmPrinter &AP);

bool lowerRISCVMachineOperandToMCOperand(const MachineOperand &MO,
                                         MCOperand &MCOp, const AsmPrinter &AP);

FunctionPass *createRISCVISelDag(RISCVTargetMachine &TM,
                                 CodeGenOpt::Level OptLevel);

FunctionPass *createRISCVMakeCompressibleOptPass();
void initializeRISCVMakeCompressibleOptPass(PassRegistry &);

FunctionPass *createRISCVGatherScatterLoweringPass();
void initializeRISCVGatherScatterLoweringPass(PassRegistry &);

FunctionPass *createRISCVSExtWRemovalPass();
void initializeRISCVSExtWRemovalPass(PassRegistry &);

FunctionPass *createRISCVMergeBaseOffsetOptPass();
void initializeRISCVMergeBaseOffsetOptPass(PassRegistry &);

FunctionPass *createRISCVExpandPseudoPass();
void initializeRISCVExpandPseudoPass(PassRegistry &);

FunctionPass *createRISCVPreRAExpandPseudoPass();
void initializeRISCVPreRAExpandPseudoPass(PassRegistry &);

FunctionPass *createRISCVExpandAtomicPseudoPass();
void initializeRISCVExpandAtomicPseudoPass(PassRegistry &);

FunctionPass *createRISCVRedundantCopyEliminationPass();
void initializeRISCVRedundantCopyEliminationPass(PassRegistry &);

FunctionPass *createVentusRegextInsertionPass();
void initializeVentusRegextInsertionPass(PassRegistry &);

FunctionPass *createVentusVVInstrConversionPass();
void initializeVentusVVInstrConversionPass(PassRegistry &);

FunctionPass *createVentusLegalizeLoadPass();
void initializeVentusLegalizeLoadPass(PassRegistry &);

FunctionPass *createVentusInsertJoinToVBranchPass();
void initializeVentusInsertJoinToVBranchPass(PassRegistry &);

InstructionSelector *createRISCVInstructionSelector(const RISCVTargetMachine &,
                                                    RISCVSubtarget &,
                                                    RISCVRegisterBankInfo &);
}


/// OpenCL uses address spaces to differentiate between
/// various memory regions on the hardware. On the CPU
/// all of the address spaces point to the same memory,
/// however on the GPU, each address space points to
/// a separate piece of memory that is unique from other
/// memory locations.
namespace RISCVAS {
  enum : unsigned {
    // The maximum value for flat, generic, local, private, constant and region.
    MAX_VENTUS_ADDRESS = 5,

    FLAT_ADDRESS = 0,     ///< Address space for flat memory.
    GLOBAL_ADDRESS = 1,   ///< Address space for global memory
    CONSTANT_ADDRESS = 4, ///< Address space for constant memory
    LOCAL_ADDRESS = 3,    ///< Address space for local memory.
    PRIVATE_ADDRESS = 5,  ///< Address space for private memory.

    // Some places use this if the address space can't be determined.
    UNKNOWN_ADDRESS_SPACE = ~0u,
  };
}

/// Because there are two stacks in ventus, we need to add a VGPRSpill according
/// to TargetStackID, and we also need to modify register spill action by
/// dividing two stacks, SGPRSpill && VGPRSpill
namespace RISCVStackID {
enum Value {
  Default = 0,
  SGPRSpill = 1,
  ScalableVector = 2,
  WasmLocal = 3,
  VGPRSpill = 4,
  NoAlloc = 255
};
}

#endif
