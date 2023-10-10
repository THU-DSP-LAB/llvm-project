//===-- RISCVTargetTransformInfo.cpp - RISC-V specific TTI ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVTargetTransformInfo.h"
#include "MCTargetDesc/RISCVMatInt.h"
#include "RISCVMachineFunctionInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/CostTable.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/CallingConv.h"
#include <cmath>
#include <optional>
using namespace llvm;

#define DEBUG_TYPE "riscvtti"


InstructionCost RISCVTTIImpl::getIntImmCost(const APInt &Imm, Type *Ty,
                                            TTI::TargetCostKind CostKind) {
  assert(Ty->isIntegerTy() &&
         "getIntImmCost can only estimate cost of materialising integers");

  // We have a Zero register, so 0 is always free.
  if (Imm == 0)
    return TTI::TCC_Free;

  // Otherwise, we check how many instructions it will take to materialise.
  const DataLayout &DL = getDataLayout();
  return RISCVMatInt::getIntMatCost(Imm, DL.getTypeSizeInBits(Ty),
                                    getST()->getFeatureBits());
}

// Look for patterns of shift followed by AND that can be turned into a pair of
// shifts. We won't need to materialize an immediate for the AND so these can
// be considered free.
static bool canUseShiftPair(Instruction *Inst, const APInt &Imm) {
  uint64_t Mask = Imm.getZExtValue();
  auto *BO = dyn_cast<BinaryOperator>(Inst->getOperand(0));
  if (!BO || !BO->hasOneUse())
    return false;

  if (BO->getOpcode() != Instruction::Shl)
    return false;

  if (!isa<ConstantInt>(BO->getOperand(1)))
    return false;

  unsigned ShAmt = cast<ConstantInt>(BO->getOperand(1))->getZExtValue();
  // (and (shl x, c2), c1) will be matched to (srli (slli x, c2+c3), c3) if c1
  // is a mask shifted by c2 bits with c3 leading zeros.
  if (isShiftedMask_64(Mask)) {
    unsigned Trailing = countTrailingZeros(Mask);
    if (ShAmt == Trailing)
      return true;
  }

  return false;
}

InstructionCost RISCVTTIImpl::getIntImmCostInst(unsigned Opcode, unsigned Idx,
                                                const APInt &Imm, Type *Ty,
                                                TTI::TargetCostKind CostKind,
                                                Instruction *Inst) {
  assert(Ty->isIntegerTy() &&
         "getIntImmCost can only estimate cost of materialising integers");

  // We have a Zero register, so 0 is always free.
  if (Imm == 0)
    return TTI::TCC_Free;

  // Some instructions in RISC-V can take a 12-bit immediate. Some of these are
  // commutative, in others the immediate comes from a specific argument index.
  bool Takes12BitImm = false;
  unsigned ImmArgIdx = ~0U;

  switch (Opcode) {
  case Instruction::GetElementPtr:
    // Never hoist any arguments to a GetElementPtr. CodeGenPrepare will
    // split up large offsets in GEP into better parts than ConstantHoisting
    // can.
    return TTI::TCC_Free;
  case Instruction::And:
    // zext.h
    if (Imm == UINT64_C(0xffff) && ST->hasStdExtZbb())
      return TTI::TCC_Free;
    // zext.w
    if (Imm == UINT64_C(0xffffffff) && ST->hasStdExtZba())
      return TTI::TCC_Free;
    if (Inst && Idx == 1 && Imm.getBitWidth() <= ST->getXLen() &&
        canUseShiftPair(Inst, Imm))
      return TTI::TCC_Free;
    [[fallthrough]];
  case Instruction::Add:
  case Instruction::Or:
  case Instruction::Xor:
    Takes12BitImm = true;
    break;
  case Instruction::Mul:
    // Negated power of 2 is a shift and a negate.
    if (Imm.isNegatedPowerOf2())
      return TTI::TCC_Free;
    // FIXME: There is no MULI instruction.
    Takes12BitImm = true;
    break;
  case Instruction::Sub:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
    Takes12BitImm = true;
    ImmArgIdx = 1;
    break;
  default:
    break;
  }

  if (Takes12BitImm) {
    // Check immediate is the correct argument...
    if (Instruction::isCommutative(Opcode) || Idx == ImmArgIdx) {
      // ... and fits into the 12-bit immediate.
      if (Imm.getMinSignedBits() <= 64 &&
          getTLI()->isLegalAddImmediate(Imm.getSExtValue())) {
        return TTI::TCC_Free;
      }
    }

    // Otherwise, use the full materialisation cost.
    return getIntImmCost(Imm, Ty, CostKind);
  }

  // By default, prevent hoisting.
  return TTI::TCC_Free;
}

InstructionCost
RISCVTTIImpl::getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx,
                                  const APInt &Imm, Type *Ty,
                                  TTI::TargetCostKind CostKind) {
  // Prevent hoisting in unknown cases.
  return TTI::TCC_Free;
}

TargetTransformInfo::PopcntSupportKind
RISCVTTIImpl::getPopcntSupport(unsigned TyWidth) {
  assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
  return ST->hasStdExtZbb() ? TTI::PSK_FastHardware : TTI::PSK_Software;
}

InstructionCost RISCVTTIImpl::getStoreImmCost(Type *Ty,
                                              TTI::OperandValueInfo OpInfo,
                                              TTI::TargetCostKind CostKind) {
  assert(OpInfo.isConstant() && "non constant operand?");
  if (!isa<VectorType>(Ty))
    // FIXME: We need to account for immediate materialization here, but doing
    // a decent job requires more knowledge about the immediate than we
    // currently have here.
    return 0;

  if (OpInfo.isUniform())
    // vmv.x.i, vmv.v.x, or vfmv.v.f
    // We ignore the cost of the scalar constant materialization to be consistent
    // with how we treat scalar constants themselves just above.
    return 1;

  // Add a cost of address generation + the cost of the vector load. The
  // address is expected to be a PC relative offset to a constant pool entry
  // using auipc/addi.
  return 2 + getMemoryOpCost(Instruction::Load, Ty, DL.getABITypeAlign(Ty),
                             /*AddressSpace=*/0, CostKind);
}


InstructionCost RISCVTTIImpl::getMemoryOpCost(unsigned Opcode, Type *Src,
                                              MaybeAlign Alignment,
                                              unsigned AddressSpace,
                                              TTI::TargetCostKind CostKind,
                                              TTI::OperandValueInfo OpInfo,
                                              const Instruction *I) {
  InstructionCost Cost = 0;
  if (Opcode == Instruction::Store && OpInfo.isConstant())
    Cost += getStoreImmCost(Src, OpInfo, CostKind);
  return Cost + BaseT::getMemoryOpCost(Opcode, Src, Alignment, AddressSpace,
                                       CostKind, OpInfo, I);
}

InstructionCost RISCVTTIImpl::getCmpSelInstrCost(unsigned Opcode, Type *ValTy,
                                                 Type *CondTy,
                                                 CmpInst::Predicate VecPred,
                                                 TTI::TargetCostKind CostKind,
                                                 const Instruction *I) {
  if (CostKind != TTI::TCK_RecipThroughput)
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                     I);

  if (isa<FixedVectorType>(ValTy) && !ST->useRVVForFixedLengthVectors())
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                     I);

  // Skip if scalar size of ValTy is bigger than ELEN.
  if (ValTy->isVectorTy() && ValTy->getScalarSizeInBits() > ST->getELEN())
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                     I);

  if (Opcode == Instruction::Select && ValTy->isVectorTy()) {
    std::pair<InstructionCost, MVT> LT = getTypeLegalizationCost(ValTy);
    if (CondTy->isVectorTy()) {
      if (ValTy->getScalarSizeInBits() == 1) {
        // vmandn.mm v8, v8, v9
        // vmand.mm v9, v0, v9
        // vmor.mm v0, v9, v8
        return LT.first * 3;
      }
      // vselect and max/min are supported natively.
      return LT.first * 1;
    }

    if (ValTy->getScalarSizeInBits() == 1) {
      //  vmv.v.x v9, a0
      //  vmsne.vi v9, v9, 0
      //  vmandn.mm v8, v8, v9
      //  vmand.mm v9, v0, v9
      //  vmor.mm v0, v9, v8
      return LT.first * 5;
    }

    // vmv.v.x v10, a0
    // vmsne.vi v0, v10, 0
    // vmerge.vvm v8, v9, v8, v0
    return LT.first * 3;
  }

  return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind, I);
}

InstructionCost RISCVTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueInfo Op1Info, TTI::OperandValueInfo Op2Info,
    ArrayRef<const Value *> Args, const Instruction *CxtI) {

  // TODO: Handle more cost kinds.
  if (CostKind != TTI::TCK_RecipThroughput)
    return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                         Args, CxtI);

  if (isa<FixedVectorType>(Ty) && !ST->useRVVForFixedLengthVectors())
    return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                         Args, CxtI);

  // Skip if scalar size of Ty is bigger than ELEN.
  if (isa<VectorType>(Ty) && Ty->getScalarSizeInBits() > ST->getELEN())
    return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                         Args, CxtI);

  // TODO: Handle scalar type.
  return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                       Args, CxtI);
}

void RISCVTTIImpl::getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                                           TTI::UnrollingPreferences &UP,
                                           OptimizationRemarkEmitter *ORE) {
  // TODO: More tuning on benchmarks and metrics with changes as needed
  //       would apply to all settings below to enable performance.


  if (ST->enableDefaultUnroll())
    return BasicTTIImplBase::getUnrollingPreferences(L, SE, UP, ORE);

  // Enable Upper bound unrolling universally, not dependant upon the conditions
  // below.
  UP.UpperBound = true;

  // Disable loop unrolling for Oz and Os.
  UP.OptSizeThreshold = 0;
  UP.PartialOptSizeThreshold = 0;
  if (L->getHeader()->getParent()->hasOptSize())
    return;

  SmallVector<BasicBlock *, 4> ExitingBlocks;
  L->getExitingBlocks(ExitingBlocks);
  LLVM_DEBUG(dbgs() << "Loop has:\n"
                    << "Blocks: " << L->getNumBlocks() << "\n"
                    << "Exit blocks: " << ExitingBlocks.size() << "\n");

  // Only allow another exit other than the latch. This acts as an early exit
  // as it mirrors the profitability calculation of the runtime unroller.
  if (ExitingBlocks.size() > 2)
    return;

  // Limit the CFG of the loop body for targets with a branch predictor.
  // Allowing 4 blocks permits if-then-else diamonds in the body.
  if (L->getNumBlocks() > 4)
    return;

  // Don't unroll vectorized loops, including the remainder loop
  if (getBooleanLoopAttribute(L, "llvm.loop.isvectorized"))
    return;

  // Scan the loop: don't unroll loops with calls as this could prevent
  // inlining.
  InstructionCost Cost = 0;
  for (auto *BB : L->getBlocks()) {
    for (auto &I : *BB) {
      // Initial setting - Don't unroll loops containing vectorized
      // instructions.
      if (I.getType()->isVectorTy())
        return;

      if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
        if (const Function *F = cast<CallBase>(I).getCalledFunction()) {
          if (!isLoweredToCall(F))
            continue;
        }
        return;
      }

      SmallVector<const Value *> Operands(I.operand_values());
      Cost += getInstructionCost(&I, Operands,
                                 TargetTransformInfo::TCK_SizeAndLatency);
    }
  }

  LLVM_DEBUG(dbgs() << "Cost of loop: " << Cost << "\n");

  UP.Partial = true;
  UP.Runtime = true;
  UP.UnrollRemainder = true;
  UP.UnrollAndJam = true;
  UP.UnrollAndJamInnerLoopThreshold = 60;

  // Force unrolling small loops can be very useful because of the branch
  // taken cost of the backedge.
  if (Cost < 12)
    UP.Force = true;
}

void RISCVTTIImpl::getPeelingPreferences(Loop *L, ScalarEvolution &SE,
                                         TTI::PeelingPreferences &PP) {
  BaseT::getPeelingPreferences(L, SE, PP);
}

unsigned RISCVTTIImpl::getRegUsageForType(Type *Ty) {
  TypeSize Size = DL.getTypeSizeInBits(Ty);
  if (Ty->isVectorTy()) {
    if (Size.isScalable() && ST->hasVInstructions())
      return divideCeil(Size.getKnownMinValue(), RISCV::RVVBitsPerBlock);

    if (ST->useRVVForFixedLengthVectors())
      return divideCeil(Size, ST->getRealMinVLen());
  }

  return BaseT::getRegUsageForType(Ty);
}

bool RISCVTTIImpl::isSourceOfDivergence(const Value *V) const {
  if (const Argument *A = dyn_cast<Argument>(V)) {
    // auto &Func = A->getParent()->getFunction();
    // if (Func.getFunction().getCallingConv() == CallingConv::SPIR_KERNEL ||
    //     Func.getFunction().getCallingConv() == CallingConv::VENTUS_KERNEL)
    //   return false;
    return true;
  }

  // Loads from the private memory are divergent, because
  // threads can execute the load instruction with the same inputs and get
  // different results.
  //
  // All other loads are not divergent, because if threads issue loads with the
  // same arguments, they will always get the same result.
  if (const LoadInst *Load = dyn_cast<LoadInst>(V))
    return Load->getPointerAddressSpace() == RISCVAS::PRIVATE_ADDRESS;

  // Atomics are divergent because they are executed sequentially: when an
  // atomic operation refers to the same address in each thread, then each
  // thread after the first sees the value written by the previous thread as
  // original value.
  if (isa<AtomicRMWInst>(V) || isa<AtomicCmpXchgInst>(V))
    return true;

  if (const IntrinsicInst *Intrinsic = dyn_cast<IntrinsicInst>(V)) {
    if (Intrinsic->getIntrinsicID() == Intrinsic::read_register)
    //   return isReadRegisterSourceOfDivergence(Intrinsic);
    // return AMDGPU::isIntrinsicSourceOfDivergence(Intrinsic->getIntrinsicID());
      return true;
  }

  // Assume all function calls are a source of divergence.
  if (const CallInst *CI = dyn_cast<CallInst>(V)) {
    if (CI->isInlineAsm() && isa<IntrinsicInst>(CI))
      return RISCVII::isIntrinsicSourceOfDivergence(
                  cast<IntrinsicInst>(CI)->getIntrinsicID());
    return true;
  }

  // Assume all function calls are a source of divergence.
  if (isa<InvokeInst>(V))
    return true;

  return false;
}
