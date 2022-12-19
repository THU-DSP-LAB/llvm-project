//===-- RISCVISelLowering.cpp - RISCV DAG Lowering Implementation  --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that RISCV uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "RISCVISelLowering.h"
#include "MCTargetDesc/RISCVMatInt.h"
#include "RISCV.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "RISCVTargetMachine.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-lower"

STATISTIC(NumTailCalls, "Number of tail calls");

static cl::opt<unsigned> ExtensionMaxWebSize(
    DEBUG_TYPE "-ext-max-web-size", cl::Hidden,
    cl::desc("Give the maximum size (in number of nodes) of the web of "
             "instructions that we will consider for VW expansion"),
    cl::init(18));

static cl::opt<bool>
    AllowSplatInVW_W(DEBUG_TYPE "-form-vw-w-with-splat", cl::Hidden,
                     cl::desc("Allow the formation of VW_W operations (e.g., "
                              "VWADD_W) with splat constants"),
                     cl::init(false));

RISCVTargetLowering::RISCVTargetLowering(const TargetMachine &TM,
                                         const RISCVSubtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {

  if (Subtarget.isRV32E())
    report_fatal_error("Codegen not yet implemented for RV32E");

  RISCVABI::ABI ABI = Subtarget.getTargetABI();
  assert(ABI != RISCVABI::ABI_Unknown && "Improperly initialised target ABI");

  if ((ABI == RISCVABI::ABI_ILP32F || ABI == RISCVABI::ABI_LP64F) &&
      !Subtarget.hasStdExtF()) {
    errs() << "Hard-float 'f' ABI can't be used for a target that "
                "doesn't support the F instruction set extension (ignoring "
                          "target-abi)\n";
    ABI = Subtarget.is64Bit() ? RISCVABI::ABI_LP64 : RISCVABI::ABI_ILP32;
  } else if ((ABI == RISCVABI::ABI_ILP32D || ABI == RISCVABI::ABI_LP64D) &&
             !Subtarget.hasStdExtD()) {
    errs() << "Hard-float 'd' ABI can't be used for a target that "
              "doesn't support the D instruction set extension (ignoring "
              "target-abi)\n";
    ABI = Subtarget.is64Bit() ? RISCVABI::ABI_LP64 : RISCVABI::ABI_ILP32;
  }

  switch (ABI) {
  default:
    report_fatal_error("Don't know how to lower this ABI");
  case RISCVABI::ABI_ILP32:
  case RISCVABI::ABI_ILP32F:
  case RISCVABI::ABI_ILP32D:
  case RISCVABI::ABI_LP64:
  case RISCVABI::ABI_LP64F:
  case RISCVABI::ABI_LP64D:
    break;
  }

  MVT XLenVT = Subtarget.getXLenVT();

  // Set up the register classes.
  addRegisterClass(XLenVT, &RISCV::GPRRegClass);

  if (Subtarget.hasStdExtZfh())
    addRegisterClass(MVT::f16, &RISCV::FPR16RegClass);
  if (Subtarget.hasStdExtF())
    addRegisterClass(MVT::f32, &RISCV::FPR32RegClass);
  if (Subtarget.hasStdExtD())
    addRegisterClass(MVT::f64, &RISCV::FPR64RegClass);

  static const MVT::SimpleValueType BoolVecVTs[] = {
      MVT::v1i1,  MVT::v2i1,  MVT::v4i1, MVT::v8i1,
      MVT::v16i1, MVT::v32i1, MVT::v64i1};
  static const MVT::SimpleValueType IntVecVTs[] = {
      MVT::v1i8,  MVT::v2i8,   MVT::v4i8,   MVT::v8i8,  MVT::v16i8,
      MVT::v32i8, MVT::v64i8,  MVT::v1i16,  MVT::v2i16, MVT::v4i16,
      MVT::v8i16, MVT::v16i16, MVT::v32i16, MVT::v1i32, MVT::v2i32,
      MVT::v4i32, MVT::v8i32,  MVT::v16i32, MVT::v1i64, MVT::v2i64,
      MVT::v4i64, MVT::v8i64};
  static const MVT::SimpleValueType F16VecVTs[] = {
      MVT::v1f16, MVT::v2f16,  MVT::v4f16,
      MVT::v8f16, MVT::v16f16, MVT::v32f16};
  static const MVT::SimpleValueType F32VecVTs[] = {
      MVT::v1f32, MVT::v2f32, MVT::v4f32, MVT::v8f32, MVT::v16f32};
  static const MVT::SimpleValueType F64VecVTs[] = {
      MVT::v1f64, MVT::v2f64, MVT::v4f64, MVT::v8f64};

  // Use RVV as fixed length vector on Ventus GPGPU
  if (Subtarget.hasVInstructions()) {
    // TODO: add more data type mapping
    addRegisterClass(MVT::i32, &RISCV::VGPRRegClass);
    addRegisterClass(MVT::f32, &RISCV::VGPRRegClass);
  }
  /*
  // Load float with int operation.
  setOperationAction(ISD::LOAD, MVT::f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::f32, MVT::i32);

  setOperationAction(ISD::LOAD, MVT::v2f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v2f32, MVT::v2i32);

  setOperationAction(ISD::LOAD, MVT::v3f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v3f32, MVT::v3i32);

  setOperationAction(ISD::LOAD, MVT::v4f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v4f32, MVT::v4i32);

  setOperationAction(ISD::LOAD, MVT::v5f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v5f32, MVT::v5i32);

  setOperationAction(ISD::LOAD, MVT::v6f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v6f32, MVT::v6i32);

  setOperationAction(ISD::LOAD, MVT::v7f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v7f32, MVT::v7i32);

  setOperationAction(ISD::LOAD, MVT::v8f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v8f32, MVT::v8i32);

  setOperationAction(ISD::LOAD, MVT::v9f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v9f32, MVT::v9i32);

  setOperationAction(ISD::LOAD, MVT::v10f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v10f32, MVT::v10i32);

  setOperationAction(ISD::LOAD, MVT::v11f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v11f32, MVT::v11i32);

  setOperationAction(ISD::LOAD, MVT::v12f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v12f32, MVT::v12i32);

  setOperationAction(ISD::LOAD, MVT::v16f32, Promote);
  AddPromotedToType(ISD::LOAD, MVT::v16f32, MVT::v16i32);
  */
  // Compute derived properties from the register classes.
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(RISCV::X2);

  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, XLenVT,
                   MVT::i1, Promote);
  // DAGCombiner can call isLoadExtLegal for types that aren't legal.
  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, MVT::i32,
                   MVT::i1, Promote);

  // TODO: add all necessary setOperationAction calls.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, XLenVT, Expand);

  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC, XLenVT, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Custom);
  setOperationAction(ISD::SELECT_CC, XLenVT, Expand);

  setCondCodeAction(ISD::SETLE, XLenVT, Expand);
  setCondCodeAction(ISD::SETGT, XLenVT, Custom);
  setCondCodeAction(ISD::SETGE, XLenVT, Expand);
  setCondCodeAction(ISD::SETULE, XLenVT, Expand);
  setCondCodeAction(ISD::SETUGT, XLenVT, Custom);
  setCondCodeAction(ISD::SETUGE, XLenVT, Expand);

  setOperationAction({ISD::STACKSAVE, ISD::STACKRESTORE}, MVT::Other, Expand);

  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction({ISD::VAARG, ISD::VACOPY, ISD::VAEND}, MVT::Other, Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  setOperationAction(ISD::EH_DWARF_CFA, MVT::i32, Custom);

  if (!Subtarget.hasStdExtZbb())
    setOperationAction(ISD::SIGN_EXTEND_INREG, {MVT::i8, MVT::i16}, Expand);

  if (Subtarget.is64Bit()) {
    setOperationAction(ISD::EH_DWARF_CFA, MVT::i64, Custom);

    setOperationAction(ISD::LOAD, MVT::i32, Custom);

    setOperationAction({ISD::ADD, ISD::SUB, ISD::SHL, ISD::SRA, ISD::SRL},
                       MVT::i32, Custom);

    setOperationAction({ISD::UADDO, ISD::USUBO, ISD::UADDSAT, ISD::USUBSAT},
                       MVT::i32, Custom);
  } else {
    setLibcallName(
        {RTLIB::SHL_I128, RTLIB::SRL_I128, RTLIB::SRA_I128, RTLIB::MUL_I128},
        nullptr);
    setLibcallName(RTLIB::MULO_I64, nullptr);
  }

  if (!Subtarget.hasStdExtM() && !Subtarget.hasStdExtZmmul()) {
    setOperationAction({ISD::MUL, ISD::MULHS, ISD::MULHU}, XLenVT, Expand);
  } else {
    if (Subtarget.is64Bit()) {
      setOperationAction(ISD::MUL, {MVT::i32, MVT::i128}, Custom);
    } else {
      setOperationAction(ISD::MUL, MVT::i64, Custom);
    }
  }

  if (!Subtarget.hasStdExtM()) {
    setOperationAction({ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM},
                       XLenVT, Expand);
  } else {
    if (Subtarget.is64Bit()) {
      setOperationAction({ISD::SDIV, ISD::UDIV, ISD::UREM},
                          {MVT::i8, MVT::i16, MVT::i32}, Custom);
    }
  }

  setOperationAction(
      {ISD::SDIVREM, ISD::UDIVREM, ISD::SMUL_LOHI, ISD::UMUL_LOHI}, XLenVT,
      Expand);

  setOperationAction({ISD::SHL_PARTS, ISD::SRL_PARTS, ISD::SRA_PARTS}, XLenVT,
                     Custom);

  if (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtZbkb()) {
    if (Subtarget.is64Bit())
      setOperationAction({ISD::ROTL, ISD::ROTR}, MVT::i32, Custom);
  } else {
    setOperationAction({ISD::ROTL, ISD::ROTR}, XLenVT, Expand);
  }

  // With Zbb we have an XLen rev8 instruction, but not GREVI. So we'll
  // pattern match it directly in isel.
  setOperationAction(ISD::BSWAP, XLenVT,
                     (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtZbkb())
                         ? Legal
                         : Expand);
  // Zbkb can use rev8+brev8 to implement bitreverse.
  setOperationAction(ISD::BITREVERSE, XLenVT,
                     Subtarget.hasStdExtZbkb() ? Custom : Expand);

  if (Subtarget.hasStdExtZbb()) {
    setOperationAction({ISD::SMIN, ISD::SMAX, ISD::UMIN, ISD::UMAX}, XLenVT,
                       Legal);

    if (Subtarget.is64Bit())
      setOperationAction(
          {ISD::CTTZ, ISD::CTTZ_ZERO_UNDEF, ISD::CTLZ, ISD::CTLZ_ZERO_UNDEF},
          MVT::i32, Custom);
  } else {
    setOperationAction({ISD::CTTZ, ISD::CTLZ, ISD::CTPOP}, XLenVT, Expand);
  }

  if (Subtarget.is64Bit())
    setOperationAction(ISD::ABS, MVT::i32, Custom);

  setOperationAction(ISD::SELECT, XLenVT, Custom);

  static const unsigned FPLegalNodeTypes[] = {
      ISD::FMINNUM,        ISD::FMAXNUM,       ISD::LRINT,
      ISD::LLRINT,         ISD::LROUND,        ISD::LLROUND,
      ISD::STRICT_LRINT,   ISD::STRICT_LLRINT, ISD::STRICT_LROUND,
      ISD::STRICT_LLROUND, ISD::STRICT_FMA,    ISD::STRICT_FADD,
      ISD::STRICT_FSUB,    ISD::STRICT_FMUL,   ISD::STRICT_FDIV,
      ISD::STRICT_FSQRT,   ISD::STRICT_FSETCC, ISD::STRICT_FSETCCS};

  static const ISD::CondCode FPCCToExpand[] = {
      ISD::SETOGT, ISD::SETOGE, ISD::SETONE, ISD::SETUEQ, ISD::SETUGT,
      ISD::SETUGE, ISD::SETULT, ISD::SETULE, ISD::SETUNE, ISD::SETGT,
      ISD::SETGE,  ISD::SETNE,  ISD::SETO,   ISD::SETUO};

  static const unsigned FPOpToExpand[] = {
      ISD::FSIN, ISD::FCOS,       ISD::FSINCOS,   ISD::FPOW,
      ISD::FREM, ISD::FP16_TO_FP, ISD::FP_TO_FP16};

  if (Subtarget.hasStdExtZfh())
    setOperationAction(ISD::BITCAST, MVT::i16, Custom);

  if (Subtarget.hasStdExtZfh()) {
    setOperationAction(FPLegalNodeTypes, MVT::f16, Legal);
    setOperationAction(ISD::FCEIL, MVT::f16, Custom);
    setOperationAction(ISD::FFLOOR, MVT::f16, Custom);
    setOperationAction(ISD::FTRUNC, MVT::f16, Custom);
    setOperationAction(ISD::FRINT, MVT::f16, Custom);
    setOperationAction(ISD::FROUND, MVT::f16, Custom);
    setOperationAction(ISD::FROUNDEVEN, MVT::f16, Custom);
    setOperationAction(ISD::STRICT_FP_ROUND, MVT::f16, Legal);
    setOperationAction(ISD::STRICT_FP_EXTEND, MVT::f32, Legal);
    setCondCodeAction(FPCCToExpand, MVT::f16, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f16, Expand);
    setOperationAction(ISD::SELECT, MVT::f16, Custom);
    setOperationAction(ISD::BR_CC, MVT::f16, Expand);

    setOperationAction({ISD::FREM, ISD::FNEARBYINT, ISD::FPOW, ISD::FPOWI,
                        ISD::FCOS, ISD::FSIN, ISD::FSINCOS, ISD::FEXP,
                        ISD::FEXP2, ISD::FLOG, ISD::FLOG2, ISD::FLOG10},
                       MVT::f16, Promote);

    // FIXME: Need to promote f16 STRICT_* to f32 libcalls, but we don't have
    // complete support for all operations in LegalizeDAG.
    setOperationAction({ISD::STRICT_FCEIL, ISD::STRICT_FFLOOR,
                        ISD::STRICT_FNEARBYINT, ISD::STRICT_FRINT,
                        ISD::STRICT_FROUND, ISD::STRICT_FROUNDEVEN,
                        ISD::STRICT_FTRUNC},
                       MVT::f16, Promote);

    // We need to custom promote this.
    if (Subtarget.is64Bit())
      setOperationAction(ISD::FPOWI, MVT::i32, Custom);
  }

  if (Subtarget.hasStdExtF()) {
    setOperationAction(FPLegalNodeTypes, MVT::f32, Legal);
    setOperationAction(ISD::FCEIL, MVT::f32, Custom);
    setOperationAction(ISD::FFLOOR, MVT::f32, Custom);
    setOperationAction(ISD::FTRUNC, MVT::f32, Custom);
    setOperationAction(ISD::FRINT, MVT::f32, Custom);
    setOperationAction(ISD::FROUND, MVT::f32, Custom);
    setOperationAction(ISD::FROUNDEVEN, MVT::f32, Custom);
    setCondCodeAction(FPCCToExpand, MVT::f32, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f32, Expand);
    setOperationAction(ISD::SELECT, MVT::f32, Custom);
    setOperationAction(ISD::BR_CC, MVT::f32, Expand);
    setOperationAction(FPOpToExpand, MVT::f32, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f32, MVT::f16, Expand);
    setTruncStoreAction(MVT::f32, MVT::f16, Expand);
  }

  if (Subtarget.hasStdExtF() && Subtarget.is64Bit())
    setOperationAction(ISD::BITCAST, MVT::i32, Custom);

  if (Subtarget.hasStdExtD()) {
    setOperationAction(FPLegalNodeTypes, MVT::f64, Legal);
    if (Subtarget.is64Bit()) {
      setOperationAction(ISD::FCEIL, MVT::f64, Custom);
      setOperationAction(ISD::FFLOOR, MVT::f64, Custom);
      setOperationAction(ISD::FTRUNC, MVT::f64, Custom);
      setOperationAction(ISD::FRINT, MVT::f64, Custom);
      setOperationAction(ISD::FROUND, MVT::f64, Custom);
      setOperationAction(ISD::FROUNDEVEN, MVT::f64, Custom);
    }
    setOperationAction(ISD::STRICT_FP_ROUND, MVT::f32, Legal);
    setOperationAction(ISD::STRICT_FP_EXTEND, MVT::f64, Legal);
    setCondCodeAction(FPCCToExpand, MVT::f64, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f64, Expand);
    setOperationAction(ISD::SELECT, MVT::f64, Custom);
    setOperationAction(ISD::BR_CC, MVT::f64, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::f32, Expand);
    setTruncStoreAction(MVT::f64, MVT::f32, Expand);
    setOperationAction(FPOpToExpand, MVT::f64, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::f16, Expand);
    setTruncStoreAction(MVT::f64, MVT::f16, Expand);
  }

  if (Subtarget.is64Bit())
    setOperationAction({ISD::FP_TO_UINT, ISD::FP_TO_SINT,
                        ISD::STRICT_FP_TO_UINT, ISD::STRICT_FP_TO_SINT},
                       MVT::i32, Custom);

  if (Subtarget.hasStdExtF()) {
    setOperationAction({ISD::FP_TO_UINT_SAT, ISD::FP_TO_SINT_SAT}, XLenVT,
                       Custom);

    setOperationAction({ISD::STRICT_FP_TO_UINT, ISD::STRICT_FP_TO_SINT,
                        ISD::STRICT_UINT_TO_FP, ISD::STRICT_SINT_TO_FP},
                       XLenVT, Legal);

    setOperationAction(ISD::FLT_ROUNDS_, XLenVT, Custom);
    setOperationAction(ISD::SET_ROUNDING, MVT::Other, Custom);
  }

  setOperationAction({ISD::GlobalAddress, ISD::BlockAddress, ISD::ConstantPool,
                      ISD::JumpTable},
                     XLenVT, Custom);

  setOperationAction(ISD::GlobalTLSAddress, XLenVT, Custom);

  if (Subtarget.is64Bit())
    setOperationAction(ISD::Constant, MVT::i64, Custom);

  // TODO: On M-mode only targets, the cycle[h] CSR may not be present.
  // Unfortunately this can't be determined just from the ISA naming string.
  setOperationAction(ISD::READCYCLECOUNTER, MVT::i64,
                     Subtarget.is64Bit() ? Legal : Custom);

  setOperationAction({ISD::TRAP, ISD::DEBUGTRAP}, MVT::Other, Legal);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);
  if (Subtarget.is64Bit())
    setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::i32, Custom);

  if (Subtarget.hasStdExtA()) {
    setMaxAtomicSizeInBitsSupported(Subtarget.getXLen());
    setMinCmpXchgSizeInBits(32);
  } else if (Subtarget.hasForcedAtomics()) {
    setMaxAtomicSizeInBitsSupported(Subtarget.getXLen());
  } else {
    setMaxAtomicSizeInBitsSupported(0);
  }

  setBooleanContents(ZeroOrOneBooleanContent);

  if (Subtarget.hasVInstructions()) {
    setBooleanVectorContents(ZeroOrOneBooleanContent);

    // RVV intrinsics may have illegal operands.
    // We also need to custom legalize vmv.x.s.
    setOperationAction({ISD::INTRINSIC_WO_CHAIN, ISD::INTRINSIC_W_CHAIN},
                       {MVT::i8, MVT::i16}, Custom);
    if (Subtarget.is64Bit())
      setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::i32, Custom);
    else
      setOperationAction({ISD::INTRINSIC_WO_CHAIN, ISD::INTRINSIC_W_CHAIN},
                         MVT::i64, Custom);

    setOperationAction({ISD::INTRINSIC_W_CHAIN, ISD::INTRINSIC_VOID},
                       MVT::Other, Custom);

    for (MVT VT : BoolVecVTs) {
      if (!isTypeLegal(VT))
        continue;

      setOperationAction(ISD::SELECT, VT, Custom);

      // RVV has native int->float & float->int conversions where the
      // element type sizes are within one power-of-two of each other. Any
      // wider distances between type sizes have to be lowered as sequences
      // which progressively narrow the gap in stages.
      setOperationAction(
          {ISD::SINT_TO_FP, ISD::UINT_TO_FP, ISD::FP_TO_SINT, ISD::FP_TO_UINT},
          VT, Custom);
      setOperationAction({ISD::FP_TO_SINT_SAT, ISD::FP_TO_UINT_SAT}, VT,
                         Custom);

      // Expand all extending loads to types larger than this, and truncating
      // stores from types larger than this.
      for (MVT OtherVT : MVT::integer_scalable_vector_valuetypes()) {
        setTruncStoreAction(OtherVT, VT, Expand);
        setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, OtherVT,
                         VT, Expand);
      }

      setOperationAction({ISD::VP_FP_TO_SINT, ISD::VP_FP_TO_UINT,
                          ISD::VP_TRUNCATE, ISD::VP_SETCC},
                         VT, Custom);
      setOperationAction(ISD::VECTOR_REVERSE, VT, Custom);

      setOperationPromotedToType(
          ISD::VECTOR_SPLICE, VT,
          MVT::getVectorVT(MVT::i8, VT.getVectorElementCount()));
    }

    for (MVT VT : IntVecVTs) {
      if (!isTypeLegal(VT))
        continue;

      setOperationAction(ISD::SPLAT_VECTOR, VT, Legal);
      setOperationAction(ISD::SPLAT_VECTOR_PARTS, VT, Custom);

      setOperationAction({ISD::SMIN, ISD::SMAX, ISD::UMIN, ISD::UMAX}, VT,
                         Legal);

      setOperationAction({ISD::ROTL, ISD::ROTR}, VT, Expand);

      setOperationAction({ISD::CTTZ, ISD::CTLZ, ISD::CTPOP}, VT, Expand);

      setOperationAction(ISD::BSWAP, VT, Expand);
      setOperationAction(ISD::VP_BSWAP, VT, Expand);

      setOperationAction(
          {ISD::SADDSAT, ISD::UADDSAT, ISD::SSUBSAT, ISD::USUBSAT}, VT, Legal);

      setOperationAction(ISD::SELECT, VT, Custom);
      setOperationAction(ISD::SELECT_CC, VT, Expand);
    }

    // Expand various CCs to best match the RVV ISA, which natively supports UNE
    // but no other unordered comparisons, and supports all ordered comparisons
    // except ONE. Additionally, we expand GT,OGT,GE,OGE for optimization
    // purposes; they are expanded to their swapped-operand CCs (LT,OLT,LE,OLE),
    // and we pattern-match those back to the "original", swapping operands once
    // more. This way we catch both operations and both "vf" and "fv" forms with
    // fewer patterns.
    static const ISD::CondCode VFPCCToExpand[] = {
        ISD::SETO,   ISD::SETONE, ISD::SETUEQ, ISD::SETUGT,
        ISD::SETUGE, ISD::SETULT, ISD::SETULE, ISD::SETUO,
        ISD::SETGT,  ISD::SETOGT, ISD::SETGE,  ISD::SETOGE,
    };

    // Sets common operation actions on RVV floating-point vector types.
    const auto SetCommonVFPActions = [&](MVT VT) {
      // RVV has native FP_ROUND & FP_EXTEND conversions where the element type
      // sizes are within one power-of-two of each other. Therefore conversions
      // between vXf16 and vXf64 must be lowered as sequences which convert via
      // vXf32.
      setOperationAction({ISD::FP_ROUND, ISD::FP_EXTEND}, VT, Custom);
      // Expand various condition codes (explained above).
      setCondCodeAction(VFPCCToExpand, VT, Expand);

      setOperationAction({ISD::FMINNUM, ISD::FMAXNUM}, VT, Legal);

      setOperationAction(
          {ISD::FTRUNC, ISD::FCEIL, ISD::FFLOOR, ISD::FROUND, ISD::FROUNDEVEN},
          VT, Custom);

      // Expand FP operations that need libcalls.
      setOperationAction(ISD::FREM, VT, Expand);
      setOperationAction(ISD::FPOW, VT, Expand);
      setOperationAction(ISD::FCOS, VT, Expand);
      setOperationAction(ISD::FSIN, VT, Expand);
      setOperationAction(ISD::FSINCOS, VT, Expand);
      setOperationAction(ISD::FEXP, VT, Expand);
      setOperationAction(ISD::FEXP2, VT, Expand);
      setOperationAction(ISD::FLOG, VT, Expand);
      setOperationAction(ISD::FLOG2, VT, Expand);
      setOperationAction(ISD::FLOG10, VT, Expand);
      setOperationAction(ISD::FRINT, VT, Expand);
      setOperationAction(ISD::FNEARBYINT, VT, Expand);

      setOperationAction(ISD::FCOPYSIGN, VT, Legal);

      setOperationAction({ISD::LOAD, ISD::STORE}, VT, Custom);

      setOperationAction(ISD::SELECT, VT, Custom);
      setOperationAction(ISD::SELECT_CC, VT, Expand);
    };

    // Sets common extload/truncstore actions on RVV floating-point vector
    // types.
    const auto SetCommonVFPExtLoadTruncStoreActions =
        [&](MVT VT, ArrayRef<MVT::SimpleValueType> SmallerVTs) {
          for (auto SmallVT : SmallerVTs) {
            setTruncStoreAction(VT, SmallVT, Expand);
            setLoadExtAction(ISD::EXTLOAD, VT, SmallVT, Expand);
          }
        };

    if (Subtarget.hasVInstructionsF16()) {
      for (MVT VT : F16VecVTs) {
        if (!isTypeLegal(VT))
          continue;
        SetCommonVFPActions(VT);
      }
    }

    if (Subtarget.hasVInstructionsF32()) {
      for (MVT VT : F32VecVTs) {
        if (!isTypeLegal(VT))
          continue;
        SetCommonVFPActions(VT);
        SetCommonVFPExtLoadTruncStoreActions(VT, F16VecVTs);
      }
    }

    if (Subtarget.hasVInstructionsF64()) {
      for (MVT VT : F64VecVTs) {
        if (!isTypeLegal(VT))
          continue;
        SetCommonVFPActions(VT);
        SetCommonVFPExtLoadTruncStoreActions(VT, F16VecVTs);
        SetCommonVFPExtLoadTruncStoreActions(VT, F32VecVTs);
      }
    }

    for (MVT VT : MVT::integer_fixedlen_vector_valuetypes()) {
      // By default everything must be expanded.
      for (unsigned Op = 0; Op < ISD::BUILTIN_OP_END; ++Op)
        setOperationAction(Op, VT, Expand);
      for (MVT OtherVT : MVT::integer_fixedlen_vector_valuetypes()) {
        setTruncStoreAction(VT, OtherVT, Expand);
        setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD},
                          OtherVT, VT, Expand);
      }

      setOperationAction(
          {ISD::SADDSAT, ISD::UADDSAT, ISD::SSUBSAT, ISD::USUBSAT}, VT,
          Custom);

      setOperationAction(ISD::VSELECT, VT, Custom);
      setOperationAction(ISD::SELECT_CC, VT, Expand);

      setOperationAction(
          {ISD::ANY_EXTEND, ISD::SIGN_EXTEND, ISD::ZERO_EXTEND}, VT, Custom);


      // Lower CTLZ_ZERO_UNDEF and CTTZ_ZERO_UNDEF if we have a floating point
      // type that can represent the value exactly.
      if (VT.getVectorElementType() != MVT::i64) {
        MVT FloatEltVT =
            VT.getVectorElementType() == MVT::i32 ? MVT::f64 : MVT::f32;
        EVT FloatVT =
            MVT::getVectorVT(FloatEltVT, VT.getVectorElementCount());
        if (isTypeLegal(FloatVT))
          setOperationAction({ISD::CTLZ_ZERO_UNDEF, ISD::CTTZ_ZERO_UNDEF}, VT,
                              Custom);
      }
    }

    for (MVT VT : MVT::fp_fixedlen_vector_valuetypes()) {
      // By default everything must be expanded.
      for (unsigned Op = 0; Op < ISD::BUILTIN_OP_END; ++Op)
        setOperationAction(Op, VT, Expand);
      for (MVT OtherVT : MVT::fp_fixedlen_vector_valuetypes()) {
        setLoadExtAction(ISD::EXTLOAD, OtherVT, VT, Expand);
        setTruncStoreAction(VT, OtherVT, Expand);
      }


      setOperationAction({ISD::FP_ROUND, ISD::FP_EXTEND}, VT, Custom);

      setOperationAction({ISD::FTRUNC, ISD::FCEIL, ISD::FFLOOR, ISD::FROUND,
                          ISD::FROUNDEVEN},
                          VT, Custom);

      setCondCodeAction(VFPCCToExpand, VT, Expand);

      setOperationAction(ISD::SELECT_CC, VT, Expand);

      setOperationAction(ISD::BITCAST, VT, Custom);
    }

    // Custom-legalize bitcasts from fixed-length vectors to scalar types.
    setOperationAction(ISD::BITCAST, {MVT::i8, MVT::i16, MVT::i32, MVT::i64},
                        Custom);
    if (Subtarget.hasStdExtZfh())
      setOperationAction(ISD::BITCAST, MVT::f16, Custom);
    if (Subtarget.hasStdExtF())
      setOperationAction(ISD::BITCAST, MVT::f32, Custom);
    if (Subtarget.hasStdExtD())
      setOperationAction(ISD::BITCAST, MVT::f64, Custom);
  }

  if (Subtarget.hasForcedAtomics()) {
    // Set atomic rmw/cas operations to expand to force __sync libcalls.
    setOperationAction(
        {ISD::ATOMIC_CMP_SWAP, ISD::ATOMIC_SWAP, ISD::ATOMIC_LOAD_ADD,
         ISD::ATOMIC_LOAD_SUB, ISD::ATOMIC_LOAD_AND, ISD::ATOMIC_LOAD_OR,
         ISD::ATOMIC_LOAD_XOR, ISD::ATOMIC_LOAD_NAND, ISD::ATOMIC_LOAD_MIN,
         ISD::ATOMIC_LOAD_MAX, ISD::ATOMIC_LOAD_UMIN, ISD::ATOMIC_LOAD_UMAX},
        XLenVT, Expand);
  }

  // Function alignments.
  const Align FunctionAlignment(Subtarget.hasStdExtCOrZca() ? 2 : 4);
  setMinFunctionAlignment(FunctionAlignment);
  setPrefFunctionAlignment(FunctionAlignment);

  setMinimumJumpTableEntries(5);

  // Jumps are expensive, compared to logic
  setJumpIsExpensive();

  setTargetDAGCombine({ISD::INTRINSIC_WO_CHAIN, ISD::ADD, ISD::SUB, ISD::AND,
                       ISD::OR, ISD::XOR, ISD::SETCC});
  if (Subtarget.is64Bit())
    setTargetDAGCombine(ISD::SRA);

  if (Subtarget.hasStdExtF())
    setTargetDAGCombine({ISD::FADD, ISD::FMAXNUM, ISD::FMINNUM});

  if (Subtarget.hasStdExtZbb())
    setTargetDAGCombine({ISD::UMAX, ISD::UMIN, ISD::SMAX, ISD::SMIN});

  if (Subtarget.hasStdExtZbs() && Subtarget.is64Bit())
    setTargetDAGCombine(ISD::TRUNCATE);

  if (Subtarget.hasStdExtZbkb())
    setTargetDAGCombine(ISD::BITREVERSE);
  if (Subtarget.hasStdExtZfh())
    setTargetDAGCombine(ISD::SIGN_EXTEND_INREG);
  if (Subtarget.hasStdExtF())
    setTargetDAGCombine({ISD::ZERO_EXTEND, ISD::FP_TO_SINT, ISD::FP_TO_UINT,
                         ISD::FP_TO_SINT_SAT, ISD::FP_TO_UINT_SAT});
  if (Subtarget.hasVInstructions())
    setTargetDAGCombine({ISD::FCOPYSIGN, ISD::MGATHER, ISD::MSCATTER,
                         ISD::VP_GATHER, ISD::VP_SCATTER, ISD::SRA, ISD::SRL,
                         ISD::SHL, ISD::STORE, ISD::SPLAT_VECTOR});
  if (Subtarget.useRVVForFixedLengthVectors())
    setTargetDAGCombine(ISD::BITCAST);

  setLibcallName(RTLIB::FPEXT_F16_F32, "__extendhfsf2");
  setLibcallName(RTLIB::FPROUND_F32_F16, "__truncsfhf2");
}

EVT RISCVTargetLowering::getSetCCResultType(const DataLayout &DL,
                                            LLVMContext &Context,
                                            EVT VT) const {
  if (!VT.isVector())
    return getPointerTy(DL);
  if (Subtarget.hasVInstructions() &&
      (VT.isScalableVector() || Subtarget.useRVVForFixedLengthVectors()))
    return EVT::getVectorVT(Context, MVT::i1, VT.getVectorElementCount());
  return VT.changeVectorElementTypeToInteger();
}

bool RISCVTargetLowering::getTgtMemIntrinsic(IntrinsicInfo &Info,
                                             const CallInst &I,
                                             MachineFunction &MF,
                                             unsigned Intrinsic) const {
  auto &DL = I.getModule()->getDataLayout();
  switch (Intrinsic) {
  default:
    return false;
  case Intrinsic::riscv_masked_atomicrmw_xchg_i32:
  case Intrinsic::riscv_masked_atomicrmw_add_i32:
  case Intrinsic::riscv_masked_atomicrmw_sub_i32:
  case Intrinsic::riscv_masked_atomicrmw_nand_i32:
  case Intrinsic::riscv_masked_atomicrmw_max_i32:
  case Intrinsic::riscv_masked_atomicrmw_min_i32:
  case Intrinsic::riscv_masked_atomicrmw_umax_i32:
  case Intrinsic::riscv_masked_atomicrmw_umin_i32:
  case Intrinsic::riscv_masked_cmpxchg_i32:
    Info.opc = ISD::INTRINSIC_W_CHAIN;
    Info.memVT = MVT::i32;
    Info.ptrVal = I.getArgOperand(0);
    Info.offset = 0;
    Info.align = Align(4);
    Info.flags = MachineMemOperand::MOLoad | MachineMemOperand::MOStore |
                 MachineMemOperand::MOVolatile;
    return true;
  case Intrinsic::riscv_masked_strided_load:
    Info.opc = ISD::INTRINSIC_W_CHAIN;
    Info.ptrVal = I.getArgOperand(1);
    Info.memVT = getValueType(DL, I.getType()->getScalarType());
    Info.align = Align(DL.getTypeSizeInBits(I.getType()->getScalarType()) / 8);
    Info.size = MemoryLocation::UnknownSize;
    Info.flags |= MachineMemOperand::MOLoad;
    return true;
  case Intrinsic::riscv_masked_strided_store:
    Info.opc = ISD::INTRINSIC_VOID;
    Info.ptrVal = I.getArgOperand(1);
    Info.memVT =
        getValueType(DL, I.getArgOperand(0)->getType()->getScalarType());
    Info.align = Align(
        DL.getTypeSizeInBits(I.getArgOperand(0)->getType()->getScalarType()) /
        8);
    Info.size = MemoryLocation::UnknownSize;
    Info.flags |= MachineMemOperand::MOStore;
    return true;
  case Intrinsic::riscv_seg2_load:
  case Intrinsic::riscv_seg3_load:
  case Intrinsic::riscv_seg4_load:
  case Intrinsic::riscv_seg5_load:
  case Intrinsic::riscv_seg6_load:
  case Intrinsic::riscv_seg7_load:
  case Intrinsic::riscv_seg8_load:
    Info.opc = ISD::INTRINSIC_W_CHAIN;
    Info.ptrVal = I.getArgOperand(0);
    Info.memVT =
        getValueType(DL, I.getType()->getStructElementType(0)->getScalarType());
    Info.align =
        Align(DL.getTypeSizeInBits(
                  I.getType()->getStructElementType(0)->getScalarType()) /
              8);
    Info.size = MemoryLocation::UnknownSize;
    Info.flags |= MachineMemOperand::MOLoad;
    return true;
  }
}

bool RISCVTargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                const AddrMode &AM, Type *Ty,
                                                unsigned AS,
                                                Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // RVV instructions only support register addressing.
  if (Subtarget.hasVInstructions() && isa<VectorType>(Ty))
    return AM.HasBaseReg && AM.Scale == 0 && !AM.BaseOffs;

  // Require a 12-bit signed offset.
  if (!isInt<12>(AM.BaseOffs))
    return false;

  switch (AM.Scale) {
  case 0: // "r+i" or just "i", depending on HasBaseReg.
    break;
  case 1:
    if (!AM.HasBaseReg) // allow "r+i".
      break;
    return false; // disallow "r+r" or "r+r+i".
  default:
    return false;
  }

  return true;
}

bool RISCVTargetLowering::isLegalICmpImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

bool RISCVTargetLowering::isLegalAddImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

// On RV32, 64-bit integers are split into their high and low parts and held
// in two different registers, so the trunc is free since the low register can
// just be used.
// FIXME: Should we consider i64->i32 free on RV64 to match the EVT version of
// isTruncateFree?
bool RISCVTargetLowering::isTruncateFree(Type *SrcTy, Type *DstTy) const {
  if (Subtarget.is64Bit() || !SrcTy->isIntegerTy() || !DstTy->isIntegerTy())
    return false;
  unsigned SrcBits = SrcTy->getPrimitiveSizeInBits();
  unsigned DestBits = DstTy->getPrimitiveSizeInBits();
  return (SrcBits == 64 && DestBits == 32);
}

bool RISCVTargetLowering::isTruncateFree(EVT SrcVT, EVT DstVT) const {
  // We consider i64->i32 free on RV64 since we have good selection of W
  // instructions that make promoting operations back to i64 free in many cases.
  if (SrcVT.isVector() || DstVT.isVector() || !SrcVT.isInteger() ||
      !DstVT.isInteger())
    return false;
  unsigned SrcBits = SrcVT.getSizeInBits();
  unsigned DestBits = DstVT.getSizeInBits();
  return (SrcBits == 64 && DestBits == 32);
}

bool RISCVTargetLowering::isZExtFree(SDValue Val, EVT VT2) const {
  // Zexts are free if they can be combined with a load.
  // Don't advertise i32->i64 zextload as being free for RV64. It interacts
  // poorly with type legalization of compares preferring sext.
  if (auto *LD = dyn_cast<LoadSDNode>(Val)) {
    EVT MemVT = LD->getMemoryVT();
    if ((MemVT == MVT::i8 || MemVT == MVT::i16) &&
        (LD->getExtensionType() == ISD::NON_EXTLOAD ||
         LD->getExtensionType() == ISD::ZEXTLOAD))
      return true;
  }

  return TargetLowering::isZExtFree(Val, VT2);
}

bool RISCVTargetLowering::isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const {
  return Subtarget.is64Bit() && SrcVT == MVT::i32 && DstVT == MVT::i64;
}

bool RISCVTargetLowering::signExtendConstant(const ConstantInt *CI) const {
  return Subtarget.is64Bit() && CI->getType()->isIntegerTy(32);
}

bool RISCVTargetLowering::isCheapToSpeculateCttz(Type *Ty) const {
  return Subtarget.hasStdExtZbb();
}

bool RISCVTargetLowering::isCheapToSpeculateCtlz(Type *Ty) const {
  return Subtarget.hasStdExtZbb();
}

bool RISCVTargetLowering::isMaskAndCmp0FoldingBeneficial(
    const Instruction &AndI) const {
  // We expect to be able to match a bit extraction instruction if the Zbs
  // extension is supported and the mask is a power of two. However, we
  // conservatively return false if the mask would fit in an ANDI instruction,
  // on the basis that it's possible the sinking+duplication of the AND in
  // CodeGenPrepare triggered by this hook wouldn't decrease the instruction
  // count and would increase code size (e.g. ANDI+BNEZ => BEXTI+BNEZ).
  if (!Subtarget.hasStdExtZbs())
    return false;
  ConstantInt *Mask = dyn_cast<ConstantInt>(AndI.getOperand(1));
  if (!Mask)
    return false;
  return !Mask->getValue().isSignedIntN(12) && Mask->getValue().isPowerOf2();
}

bool RISCVTargetLowering::hasAndNotCompare(SDValue Y) const {
  EVT VT = Y.getValueType();

  // FIXME: Support vectors once we have tests.
  if (VT.isVector())
    return false;

  return (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtZbkb()) &&
         !isa<ConstantSDNode>(Y);
}

bool RISCVTargetLowering::hasBitTest(SDValue X, SDValue Y) const {
  // Zbs provides BEXT[_I], which can be used with SEQZ/SNEZ as a bit test.
  if (Subtarget.hasStdExtZbs())
    return X.getValueType().isScalarInteger();
  // We can use ANDI+SEQZ/SNEZ as a bit test. Y contains the bit position.
  auto *C = dyn_cast<ConstantSDNode>(Y);
  return C && C->getAPIntValue().ule(10);
}

bool RISCVTargetLowering::shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                                            Type *Ty) const {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getIntegerBitWidth();
  if (BitSize > Subtarget.getXLen())
    return false;

  // Fast path, assume 32-bit immediates are cheap.
  int64_t Val = Imm.getSExtValue();
  if (isInt<32>(Val))
    return true;

  // A constant pool entry may be more aligned thant he load we're trying to
  // replace. If we don't support unaligned scalar mem, prefer the constant
  // pool.
  // TODO: Can the caller pass down the alignment?
  if (!Subtarget.enableUnalignedScalarMem())
    return true;

  // Prefer to keep the load if it would require many instructions.
  // This uses the same threshold we use for constant pools but doesn't
  // check useConstantPoolForLargeInts.
  // TODO: Should we keep the load only when we're definitely going to emit a
  // constant pool?

  RISCVMatInt::InstSeq Seq =
      RISCVMatInt::generateInstSeq(Val, Subtarget.getFeatureBits());
  return Seq.size() <= Subtarget.getMaxBuildIntsCost();
}

bool RISCVTargetLowering::
    shouldProduceAndByConstByHoistingConstFromShiftsLHSOfAnd(
        SDValue X, ConstantSDNode *XC, ConstantSDNode *CC, SDValue Y,
        unsigned OldShiftOpcode, unsigned NewShiftOpcode,
        SelectionDAG &DAG) const {
  // One interesting pattern that we'd want to form is 'bit extract':
  //   ((1 >> Y) & 1) ==/!= 0
  // But we also need to be careful not to try to reverse that fold.

  // Is this '((1 >> Y) & 1)'?
  if (XC && OldShiftOpcode == ISD::SRL && XC->isOne())
    return false; // Keep the 'bit extract' pattern.

  // Will this be '((1 >> Y) & 1)' after the transform?
  if (NewShiftOpcode == ISD::SRL && CC->isOne())
    return true; // Do form the 'bit extract' pattern.

  // If 'X' is a constant, and we transform, then we will immediately
  // try to undo the fold, thus causing endless combine loop.
  // So only do the transform if X is not a constant. This matches the default
  // implementation of this function.
  return !XC;
}

bool RISCVTargetLowering::canSplatOperand(unsigned Opcode, int Operand) const {
  switch (Opcode) {
  case Instruction::Add:
  case Instruction::Sub:
  case Instruction::Mul:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
  case Instruction::FAdd:
  case Instruction::FSub:
  case Instruction::FMul:
  case Instruction::FDiv:
  case Instruction::ICmp:
  case Instruction::FCmp:
    return true;
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem:
    return Operand == 1;
  default:
    return false;
  }
}


bool RISCVTargetLowering::canSplatOperand(Instruction *I, int Operand) const {
  if (!I->getType()->isVectorTy() || !Subtarget.hasVInstructions())
    return false;

  if (canSplatOperand(I->getOpcode(), Operand))
    return true;

  auto *II = dyn_cast<IntrinsicInst>(I);
  if (!II)
    return false;

  switch (II->getIntrinsicID()) {
  case Intrinsic::fma:
  case Intrinsic::vp_fma:
    return Operand == 0 || Operand == 1;
  case Intrinsic::vp_shl:
  case Intrinsic::vp_lshr:
  case Intrinsic::vp_ashr:
  case Intrinsic::vp_udiv:
  case Intrinsic::vp_sdiv:
  case Intrinsic::vp_urem:
  case Intrinsic::vp_srem:
    return Operand == 1;
    // These intrinsics are commutative.
  case Intrinsic::vp_add:
  case Intrinsic::vp_mul:
  case Intrinsic::vp_and:
  case Intrinsic::vp_or:
  case Intrinsic::vp_xor:
  case Intrinsic::vp_fadd:
  case Intrinsic::vp_fmul:
    // These intrinsics have 'vr' versions.
  case Intrinsic::vp_sub:
  case Intrinsic::vp_fsub:
  case Intrinsic::vp_fdiv:
    return Operand == 0 || Operand == 1;
  default:
    return false;
  }
}

/// Check if sinking \p I's operands to I's basic block is profitable, because
/// the operands can be folded into a target instruction, e.g.
/// splats of scalars can fold into vector instructions.
bool RISCVTargetLowering::shouldSinkOperands(
    Instruction *I, SmallVectorImpl<Use *> &Ops) const {
  using namespace llvm::PatternMatch;

  if (!I->getType()->isVectorTy() || !Subtarget.hasVInstructions())
    return false;

  for (auto OpIdx : enumerate(I->operands())) {
    if (!canSplatOperand(I, OpIdx.index()))
      continue;

    Instruction *Op = dyn_cast<Instruction>(OpIdx.value().get());
    // Make sure we are not already sinking this operand
    if (!Op || any_of(Ops, [&](Use *U) { return U->get() == Op; }))
      continue;

    // We are looking for a splat that can be sunk.
    if (!match(Op, m_Shuffle(m_InsertElt(m_Undef(), m_Value(), m_ZeroInt()),
                             m_Undef(), m_ZeroMask())))
      continue;

    // All uses of the shuffle should be sunk to avoid duplicating it across gpr
    // and vector registers
    for (Use &U : Op->uses()) {
      Instruction *Insn = cast<Instruction>(U.getUser());
      if (!canSplatOperand(Insn, U.getOperandNo()))
        return false;
    }

    Ops.push_back(&Op->getOperandUse(0));
    Ops.push_back(&OpIdx.value());
  }
  return true;
}

bool RISCVTargetLowering::shouldScalarizeBinop(SDValue VecOp) const {
  unsigned Opc = VecOp.getOpcode();

  // Assume target opcodes can't be scalarized.
  // TODO - do we have any exceptions?
  if (Opc >= ISD::BUILTIN_OP_END)
    return false;

  // If the vector op is not supported, try to convert to scalar.
  EVT VecVT = VecOp.getValueType();
  if (!isOperationLegalOrCustomOrPromote(Opc, VecVT))
    return true;

  // If the vector op is supported, but the scalar op is not, the transform may
  // not be worthwhile.
  EVT ScalarVT = VecVT.getScalarType();
  return isOperationLegalOrCustomOrPromote(Opc, ScalarVT);
}

bool RISCVTargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  // In order to maximise the opportunity for common subexpression elimination,
  // keep a separate ADD node for the global address offset instead of folding
  // it in the global address node. Later peephole optimisations may choose to
  // fold it back in when profitable.
  return false;
}

bool RISCVTargetLowering::isFPImmLegal(const APFloat &Imm, EVT VT,
                                       bool ForCodeSize) const {
  // FIXME: Change to Zfhmin once f16 becomes a legal type with Zfhmin.
  if (VT == MVT::f16 && !Subtarget.hasStdExtZfh())
    return false;
  if (VT == MVT::f32 && !Subtarget.hasStdExtF())
    return false;
  if (VT == MVT::f64 && !Subtarget.hasStdExtD())
    return false;
  return Imm.isZero();
}

// TODO: This is very conservative.
bool RISCVTargetLowering::isExtractSubvectorCheap(EVT ResVT, EVT SrcVT,
                                                  unsigned Index) const {
  if (!isOperationLegalOrCustom(ISD::EXTRACT_SUBVECTOR, ResVT))
    return false;

  // Only support extracting a fixed from a fixed vector for now.
  if (ResVT.isScalableVector() || SrcVT.isScalableVector())
    return false;

  unsigned ResElts = ResVT.getVectorNumElements();
  unsigned SrcElts = SrcVT.getVectorNumElements();

  // Convervatively only handle extracting half of a vector.
  // TODO: Relax this.
  if ((ResElts * 2) != SrcElts)
    return false;

  // The smallest type we can slide is i8.
  // TODO: We can extract index 0 from a mask vector without a slide.
  if (ResVT.getVectorElementType() == MVT::i1)
    return false;

  // Slide can support arbitrary index, but we only treat vslidedown.vi as
  // cheap.
  if (Index >= 32)
    return false;

  // TODO: We can do arbitrary slidedowns, but for now only support extracting
  // the upper half of a vector until we have more test coverage.
  return Index == 0 || Index == ResElts;
}

bool RISCVTargetLowering::hasBitPreservingFPLogic(EVT VT) const {
  return (VT == MVT::f16 && Subtarget.hasStdExtZfh()) ||
         (VT == MVT::f32 && Subtarget.hasStdExtF()) ||
         (VT == MVT::f64 && Subtarget.hasStdExtD());
}

MVT RISCVTargetLowering::getRegisterTypeForCallingConv(LLVMContext &Context,
                                                      CallingConv::ID CC,
                                                      EVT VT) const {
  // Use f32 to pass f16 if it is legal and Zfh is not enabled.
  // We might still end up using a GPR but that will be decided based on ABI.
  // FIXME: Change to Zfhmin once f16 becomes a legal type with Zfhmin.
  if (VT == MVT::f16 && Subtarget.hasStdExtF() && !Subtarget.hasStdExtZfh())
    return MVT::f32;

  MVT SVT = VT.getSimpleVT();
  if (Subtarget.getCPU() == "ventus-gpgpu" && SVT.isVector())
    return SVT;

  return TargetLowering::getRegisterTypeForCallingConv(Context, CC, VT);
}

unsigned RISCVTargetLowering::getNumRegistersForCallingConv(LLVMContext &Context,
                                                           CallingConv::ID CC,
                                                           EVT VT) const {
  // Use f32 to pass f16 if it is legal and Zfh is not enabled.
  // We might still end up using a GPR but that will be decided based on ABI.
  // FIXME: Change to Zfhmin once f16 becomes a legal type with Zfhmin.
  if (VT == MVT::f16 && Subtarget.hasStdExtF() && !Subtarget.hasStdExtZfh())
    return 1;

  if (Subtarget.getCPU() == "ventus-gpgpu")
    return 1;

  return TargetLowering::getNumRegistersForCallingConv(Context, CC, VT);
}

// Changes the condition code and swaps operands if necessary, so the SetCC
// operation matches one of the comparisons supported directly by branches
// in the RISC-V ISA. May adjust compares to favor compare with 0 over compare
// with 1/-1.
static void translateSetCCForBranch(const SDLoc &DL, SDValue &LHS, SDValue &RHS,
                                    ISD::CondCode &CC, SelectionDAG &DAG) {
  // If this is a single bit test that can't be handled by ANDI, shift the
  // bit to be tested to the MSB and perform a signed compare with 0.
  if (isIntEqualitySetCC(CC) && isNullConstant(RHS) &&
      LHS.getOpcode() == ISD::AND && LHS.hasOneUse() &&
      isa<ConstantSDNode>(LHS.getOperand(1))) {
    uint64_t Mask = LHS.getConstantOperandVal(1);
    if (isPowerOf2_64(Mask) && !isInt<12>(Mask)) {
      CC = CC == ISD::SETEQ ? ISD::SETGE : ISD::SETLT;
      unsigned ShAmt = LHS.getValueSizeInBits() - 1 - Log2_64(Mask);
      LHS = LHS.getOperand(0);
      if (ShAmt != 0)
        LHS = DAG.getNode(ISD::SHL, DL, LHS.getValueType(), LHS,
                          DAG.getConstant(ShAmt, DL, LHS.getValueType()));
      return;
    }
  }

  if (auto *RHSC = dyn_cast<ConstantSDNode>(RHS)) {
    int64_t C = RHSC->getSExtValue();
    switch (CC) {
    default: break;
    case ISD::SETGT:
      // Convert X > -1 to X >= 0.
      if (C == -1) {
        RHS = DAG.getConstant(0, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      break;
    case ISD::SETLT:
      // Convert X < 1 to 0 <= X.
      if (C == 1) {
        RHS = LHS;
        LHS = DAG.getConstant(0, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      break;
    }
  }

  switch (CC) {
  default:
    break;
  case ISD::SETGT:
  case ISD::SETLE:
  case ISD::SETUGT:
  case ISD::SETULE:
    CC = ISD::getSetCCSwappedOperands(CC);
    std::swap(LHS, RHS);
    break;
  }
}

static RISCVFPRndMode::RoundingMode matchRoundingOp(unsigned Opc) {
  switch (Opc) {
  case ISD::FROUNDEVEN:
  case ISD::VP_FROUNDEVEN:
    return RISCVFPRndMode::RNE;
  case ISD::FTRUNC:
  case ISD::VP_FROUNDTOZERO:
    return RISCVFPRndMode::RTZ;
  case ISD::FFLOOR:
  case ISD::VP_FFLOOR:
    return RISCVFPRndMode::RDN;
  case ISD::FCEIL:
  case ISD::VP_FCEIL:
    return RISCVFPRndMode::RUP;
  case ISD::FROUND:
  case ISD::VP_FROUND:
    return RISCVFPRndMode::RMM;
  case ISD::FRINT:
    return RISCVFPRndMode::DYN;
  }

  return RISCVFPRndMode::Invalid;
}

static std::optional<uint64_t> getExactInteger(const APFloat &APF,
                                               uint32_t BitWidth) {
  APSInt ValInt(BitWidth, !APF.isNegative());
  // We use an arbitrary rounding mode here. If a floating-point is an exact
  // integer (e.g., 1.0), the rounding mode does not affect the output value. If
  // the rounding mode changes the output value, then it is not an exact
  // integer.
  RoundingMode ArbitraryRM = RoundingMode::TowardZero;
  bool IsExact;
  // If it is out of signed integer range, it will return an invalid operation.
  // If it is not an exact integer, IsExact is false.
  if ((APF.convertToInteger(ValInt, ArbitraryRM, &IsExact) ==
       APFloatBase::opInvalidOp) ||
      !IsExact)
    return std::nullopt;
  return ValInt.extractBitsAsZExtValue(BitWidth, 0);
}

// Lower CTLZ_ZERO_UNDEF or CTTZ_ZERO_UNDEF by converting to FP and extracting
// the exponent.
static SDValue lowerCTLZ_CTTZ_ZERO_UNDEF(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  unsigned EltSize = VT.getScalarSizeInBits();
  SDValue Src = Op.getOperand(0);
  SDLoc DL(Op);

  // We need a FP type that can represent the value.
  // TODO: Use f16 for i8 when possible?
  MVT FloatEltVT = EltSize == 32 ? MVT::f64 : MVT::f32;
  MVT FloatVT = MVT::getVectorVT(FloatEltVT, VT.getVectorElementCount());

  // Legal types should have been checked in the RISCVTargetLowering
  // constructor.
  // TODO: Splitting may make sense in some cases.
  assert(DAG.getTargetLoweringInfo().isTypeLegal(FloatVT) &&
         "Expected legal float type!");

  // For CTTZ_ZERO_UNDEF, we need to extract the lowest set bit using X & -X.
  // The trailing zero count is equal to log2 of this single bit value.
  if (Op.getOpcode() == ISD::CTTZ_ZERO_UNDEF) {
    SDValue Neg = DAG.getNegative(Src, DL, VT);
    Src = DAG.getNode(ISD::AND, DL, VT, Src, Neg);
  }

  // We have a legal FP type, convert to it.
  SDValue FloatVal = DAG.getNode(ISD::UINT_TO_FP, DL, FloatVT, Src);
  // Bitcast to integer and shift the exponent to the LSB.
  EVT IntVT = FloatVT.changeVectorElementTypeToInteger();
  SDValue Bitcast = DAG.getBitcast(IntVT, FloatVal);
  unsigned ShiftAmt = FloatEltVT == MVT::f64 ? 52 : 23;
  SDValue Shift = DAG.getNode(ISD::SRL, DL, IntVT, Bitcast,
                              DAG.getConstant(ShiftAmt, DL, IntVT));
  // Truncate back to original type to allow vnsrl.
  SDValue Trunc = DAG.getNode(ISD::TRUNCATE, DL, VT, Shift);
  // The exponent contains log2 of the value in biased form.
  unsigned ExponentBias = FloatEltVT == MVT::f64 ? 1023 : 127;

  // For trailing zeros, we just need to subtract the bias.
  if (Op.getOpcode() == ISD::CTTZ_ZERO_UNDEF)
    return DAG.getNode(ISD::SUB, DL, VT, Trunc,
                       DAG.getConstant(ExponentBias, DL, VT));

  // For leading zeros, we need to remove the bias and convert from log2 to
  // leading zeros. We can do this by subtracting from (Bias + (EltSize - 1)).
  unsigned Adjust = ExponentBias + (EltSize - 1);
  return DAG.getNode(ISD::SUB, DL, VT, DAG.getConstant(Adjust, DL, VT), Trunc);
}

static SDValue lowerConstant(SDValue Op, SelectionDAG &DAG,
                             const RISCVSubtarget &Subtarget) {
  assert(Op.getValueType() == MVT::i64 && "Unexpected VT");

  int64_t Imm = cast<ConstantSDNode>(Op)->getSExtValue();

  // All simm32 constants should be handled by isel.
  // NOTE: The getMaxBuildIntsCost call below should return a value >= 2 making
  // this check redundant, but small immediates are common so this check
  // should have better compile time.
  if (isInt<32>(Imm))
    return Op;

  // We only need to cost the immediate, if constant pool lowering is enabled.
  if (!Subtarget.useConstantPoolForLargeInts())
    return Op;

  RISCVMatInt::InstSeq Seq =
      RISCVMatInt::generateInstSeq(Imm, Subtarget.getFeatureBits());
  if (Seq.size() <= Subtarget.getMaxBuildIntsCost())
    return Op;

  // Expand to a constant pool using the default expansion code.
  return SDValue();
}

SDValue RISCVTargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    report_fatal_error("unimplemented operand");
  case ISD::GlobalAddress:
    return lowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return lowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return lowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return lowerJumpTable(Op, DAG);
  case ISD::GlobalTLSAddress:
    return lowerGlobalTLSAddress(Op, DAG);
  case ISD::Constant:
    return lowerConstant(Op, DAG, Subtarget);
  case ISD::SELECT:
    return lowerSELECT(Op, DAG);
  case ISD::BRCOND:
    return lowerBRCOND(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::FRAMEADDR:
    return lowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return lowerRETURNADDR(Op, DAG);
  case ISD::SHL_PARTS:
    return lowerShiftLeftParts(Op, DAG);
  case ISD::SRA_PARTS:
    return lowerShiftRightParts(Op, DAG, true);
  case ISD::SRL_PARTS:
    return lowerShiftRightParts(Op, DAG, false);
  case ISD::BITCAST: {
    SDLoc DL(Op);
    EVT VT = Op.getValueType();
    SDValue Op0 = Op.getOperand(0);
    EVT Op0VT = Op0.getValueType();
    MVT XLenVT = Subtarget.getXLenVT();
    if (VT == MVT::f16 && Op0VT == MVT::i16 && Subtarget.hasStdExtZfh()) {
      SDValue NewOp0 = DAG.getNode(ISD::ANY_EXTEND, DL, XLenVT, Op0);
      SDValue FPConv = DAG.getNode(RISCVISD::FMV_H_X, DL, MVT::f16, NewOp0);
      return FPConv;
    }
    if (VT == MVT::f32 && Op0VT == MVT::i32 && Subtarget.is64Bit() &&
        Subtarget.hasStdExtF()) {
      SDValue NewOp0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, Op0);
      SDValue FPConv =
          DAG.getNode(RISCVISD::FMV_W_X_RV64, DL, MVT::f32, NewOp0);
      return FPConv;
    }

    // Consider other scalar<->scalar casts as legal if the types are legal.
    // Otherwise expand them.
    if (!VT.isVector() && !Op0VT.isVector()) {
      if (isTypeLegal(VT) && isTypeLegal(Op0VT))
        return Op;
      return SDValue();
    }

    assert(0 && "TODO: vALU!!");
    return SDValue();
  }
  case ISD::INTRINSIC_WO_CHAIN:
    return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  case ISD::BITREVERSE: {
    MVT VT = Op.getSimpleValueType();
    SDLoc DL(Op);
    assert(Subtarget.hasStdExtZbkb() && "Unexpected custom legalization");
    assert(Op.getOpcode() == ISD::BITREVERSE && "Unexpected opcode");
    // Expand bitreverse to a bswap(rev8) followed by brev8.
    SDValue BSwap = DAG.getNode(ISD::BSWAP, DL, VT, Op.getOperand(0));
    return DAG.getNode(RISCVISD::BREV8, DL, VT, BSwap);
  }
  case ISD::FPOWI: {
    // Custom promote f16 powi with illegal i32 integer type on RV64. Once
    // promoted this will be legalized into a libcall by LegalizeIntegerTypes.
    if (Op.getValueType() == MVT::f16 && Subtarget.is64Bit() &&
        Op.getOperand(1).getValueType() == MVT::i32) {
      SDLoc DL(Op);
      SDValue Op0 = DAG.getNode(ISD::FP_EXTEND, DL, MVT::f32, Op.getOperand(0));
      SDValue Powi =
          DAG.getNode(ISD::FPOWI, DL, MVT::f32, Op0, Op.getOperand(1));
      return DAG.getNode(ISD::FP_ROUND, DL, MVT::f16, Powi,
                         DAG.getIntPtrConstant(0, DL, /*isTarget=*/true));
    }
    return SDValue();
  }
  case ISD::SELECT_CC: {
    // This occurs because we custom legalize SETGT and SETUGT for setcc. That
    // causes LegalizeDAG to think we need to custom legalize select_cc. Expand
    // into separate SETCC+SELECT_CC just like LegalizeDAG.
    SDValue Tmp1 = Op.getOperand(0);
    SDValue Tmp2 = Op.getOperand(1);
    SDValue True = Op.getOperand(2);
    SDValue False = Op.getOperand(3);
    EVT VT = Op.getValueType();
    SDValue CC = Op.getOperand(4);
    EVT CmpVT = Tmp1.getValueType();
    EVT CCVT =
        getSetCCResultType(DAG.getDataLayout(), *DAG.getContext(), CmpVT);
    SDLoc DL(Op);
    SDValue Cond =
        DAG.getNode(ISD::SETCC, DL, CCVT, Tmp1, Tmp2, CC, Op->getFlags());
    return DAG.getSelect(DL, VT, Cond, True, False);
  }
  case ISD::CTLZ_ZERO_UNDEF:
  case ISD::CTTZ_ZERO_UNDEF:
    return lowerCTLZ_CTTZ_ZERO_UNDEF(Op, DAG);
  case ISD::FLT_ROUNDS_:
    return lowerGET_ROUNDING(Op, DAG);
  case ISD::SET_ROUNDING:
    return lowerSET_ROUNDING(Op, DAG);
  case ISD::EH_DWARF_CFA:
    return lowerEH_DWARF_CFA(Op, DAG);
  }
}

static SDValue getTargetNode(GlobalAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                   Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

template <class NodeTy>
SDValue RISCVTargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                     bool IsLocal) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());

  if (isPositionIndependent()) {
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    if (IsLocal)
      // Use PC-relative addressing to access the symbol. This generates the
      // pattern (PseudoLLA sym), which expands to (addi (auipc %pcrel_hi(sym))
      // %pcrel_lo(auipc)).
      return DAG.getNode(RISCVISD::LLA, DL, Ty, Addr);

    // Use PC-relative addressing to access the GOT for this symbol, then load
    // the address from the GOT. This generates the pattern (PseudoLA sym),
    // which expands to (ld (addi (auipc %got_pcrel_hi(sym)) %pcrel_lo(auipc))).
    MachineFunction &MF = DAG.getMachineFunction();
    MachineMemOperand *MemOp = MF.getMachineMemOperand(
        MachinePointerInfo::getGOT(MF),
        MachineMemOperand::MOLoad | MachineMemOperand::MODereferenceable |
            MachineMemOperand::MOInvariant,
        LLT(Ty.getSimpleVT()), Align(Ty.getFixedSizeInBits() / 8));
    SDValue Load =
        DAG.getMemIntrinsicNode(RISCVISD::LA, DL, DAG.getVTList(Ty, MVT::Other),
                                {DAG.getEntryNode(), Addr}, Ty, MemOp);
    return Load;
  }

  switch (getTargetMachine().getCodeModel()) {
  default:
    report_fatal_error("Unsupported code model for lowering");
  case CodeModel::Small: {
    // Generate a sequence for accessing addresses within the first 2 GiB of
    // address space. This generates the pattern (addi (lui %hi(sym)) %lo(sym)).
    SDValue AddrHi = getTargetNode(N, DL, Ty, DAG, RISCVII::MO_HI);
    SDValue AddrLo = getTargetNode(N, DL, Ty, DAG, RISCVII::MO_LO);
    SDValue MNHi = DAG.getNode(RISCVISD::HI, DL, Ty, AddrHi);
    return DAG.getNode(RISCVISD::ADD_LO, DL, Ty, MNHi, AddrLo);
  }
  case CodeModel::Medium: {
    // Generate a sequence for accessing addresses within any 2GiB range within
    // the address space. This generates the pattern (PseudoLLA sym), which
    // expands to (addi (auipc %pcrel_hi(sym)) %pcrel_lo(auipc)).
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    return DAG.getNode(RISCVISD::LLA, DL, Ty, Addr);
  }
  }
}

SDValue RISCVTargetLowering::lowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  assert(N->getOffset() == 0 && "unexpected offset in global node");
  return getAddr(N, DAG, N->getGlobal()->isDSOLocal());
}

SDValue RISCVTargetLowering::lowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  BlockAddressSDNode *N = cast<BlockAddressSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue RISCVTargetLowering::lowerConstantPool(SDValue Op,
                                               SelectionDAG &DAG) const {
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue RISCVTargetLowering::lowerJumpTable(SDValue Op,
                                            SelectionDAG &DAG) const {
  JumpTableSDNode *N = cast<JumpTableSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue RISCVTargetLowering::getStaticTLSAddr(GlobalAddressSDNode *N,
                                              SelectionDAG &DAG,
                                              bool UseGOT) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  const GlobalValue *GV = N->getGlobal();
  MVT XLenVT = Subtarget.getXLenVT();

  if (UseGOT) {
    // Use PC-relative addressing to access the GOT for this TLS symbol, then
    // load the address from the GOT and add the thread pointer. This generates
    // the pattern (PseudoLA_TLS_IE sym), which expands to
    // (ld (auipc %tls_ie_pcrel_hi(sym)) %pcrel_lo(auipc)).
    SDValue Addr = DAG.getTargetGlobalAddress(GV, DL, Ty, 0, 0);
    MachineFunction &MF = DAG.getMachineFunction();
    MachineMemOperand *MemOp = MF.getMachineMemOperand(
        MachinePointerInfo::getGOT(MF),
        MachineMemOperand::MOLoad | MachineMemOperand::MODereferenceable |
            MachineMemOperand::MOInvariant,
        LLT(Ty.getSimpleVT()), Align(Ty.getFixedSizeInBits() / 8));
    SDValue Load = DAG.getMemIntrinsicNode(
        RISCVISD::LA_TLS_IE, DL, DAG.getVTList(Ty, MVT::Other),
        {DAG.getEntryNode(), Addr}, Ty, MemOp);

    // Add the thread pointer.
    SDValue TPReg = DAG.getRegister(RISCV::X4, XLenVT);
    return DAG.getNode(ISD::ADD, DL, Ty, Load, TPReg);
  }

  // Generate a sequence for accessing the address relative to the thread
  // pointer, with the appropriate adjustment for the thread pointer offset.
  // This generates the pattern
  // (add (add_tprel (lui %tprel_hi(sym)) tp %tprel_add(sym)) %tprel_lo(sym))
  SDValue AddrHi =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, RISCVII::MO_TPREL_HI);
  SDValue AddrAdd =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, RISCVII::MO_TPREL_ADD);
  SDValue AddrLo =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, RISCVII::MO_TPREL_LO);

  SDValue MNHi = DAG.getNode(RISCVISD::HI, DL, Ty, AddrHi);
  SDValue TPReg = DAG.getRegister(RISCV::X4, XLenVT);
  SDValue MNAdd =
      DAG.getNode(RISCVISD::ADD_TPREL, DL, Ty, MNHi, TPReg, AddrAdd);
  return DAG.getNode(RISCVISD::ADD_LO, DL, Ty, MNAdd, AddrLo);
}

SDValue RISCVTargetLowering::getDynamicTLSAddr(GlobalAddressSDNode *N,
                                               SelectionDAG &DAG) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  IntegerType *CallTy = Type::getIntNTy(*DAG.getContext(), Ty.getSizeInBits());
  const GlobalValue *GV = N->getGlobal();

  // Use a PC-relative addressing mode to access the global dynamic GOT address.
  // This generates the pattern (PseudoLA_TLS_GD sym), which expands to
  // (addi (auipc %tls_gd_pcrel_hi(sym)) %pcrel_lo(auipc)).
  SDValue Addr = DAG.getTargetGlobalAddress(GV, DL, Ty, 0, 0);
  SDValue Load = DAG.getNode(RISCVISD::LA_TLS_GD, DL, Ty, Addr);

  // Prepare argument list to generate call.
  ArgListTy Args;
  ArgListEntry Entry;
  Entry.Node = Load;
  Entry.Ty = CallTy;
  Args.push_back(Entry);

  // Setup call to __tls_get_addr.
  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL)
      .setChain(DAG.getEntryNode())
      .setLibCallee(CallingConv::C, CallTy,
                    DAG.getExternalSymbol("__tls_get_addr", Ty),
                    std::move(Args));

  return LowerCallTo(CLI).first;
}

SDValue RISCVTargetLowering::lowerGlobalTLSAddress(SDValue Op,
                                                   SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  assert(N->getOffset() == 0 && "unexpected offset in global node");

  TLSModel::Model Model = getTargetMachine().getTLSModel(N->getGlobal());

  if (DAG.getMachineFunction().getFunction().getCallingConv() ==
      CallingConv::GHC)
    report_fatal_error("In GHC calling convention TLS is not supported");

  SDValue Addr;
  switch (Model) {
  case TLSModel::LocalExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/false);
    break;
  case TLSModel::InitialExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/true);
    break;
  case TLSModel::LocalDynamic:
  case TLSModel::GeneralDynamic:
    Addr = getDynamicTLSAddr(N, DAG);
    break;
  }

  return Addr;
}

SDValue RISCVTargetLowering::lowerSELECT(SDValue Op, SelectionDAG &DAG) const {
  SDValue CondV = Op.getOperand(0);
  SDValue TrueV = Op.getOperand(1);
  SDValue FalseV = Op.getOperand(2);
  SDLoc DL(Op);
  MVT VT = Op.getSimpleValueType();
  MVT XLenVT = Subtarget.getXLenVT();

  // Lower vector SELECTs to VSELECTs by splatting the condition.
  if (VT.isVector()) {
    MVT SplatCondVT = VT.changeVectorElementType(MVT::i1);
    SDValue CondSplat = DAG.getSplat(SplatCondVT, DL, CondV);
    return DAG.getNode(ISD::VSELECT, DL, VT, CondSplat, TrueV, FalseV);
  }

  if (!Subtarget.hasShortForwardBranchOpt()) {
    // (select c, -1, y) -> -c | y
    if (isAllOnesConstant(TrueV)) {
      SDValue Neg = DAG.getNegative(CondV, DL, VT);
      return DAG.getNode(ISD::OR, DL, VT, Neg, FalseV);
    }
    // (select c, y, -1) -> (c-1) | y
    if (isAllOnesConstant(FalseV)) {
      SDValue Neg = DAG.getNode(ISD::ADD, DL, VT, CondV,
                                DAG.getAllOnesConstant(DL, VT));
      return DAG.getNode(ISD::OR, DL, VT, Neg, TrueV);
    }

    // (select c, 0, y) -> (c-1) & y
    if (isNullConstant(TrueV)) {
      SDValue Neg = DAG.getNode(ISD::ADD, DL, VT, CondV,
                                DAG.getAllOnesConstant(DL, VT));
      return DAG.getNode(ISD::AND, DL, VT, Neg, FalseV);
    }
    // (select c, y, 0) -> -c & y
    if (isNullConstant(FalseV)) {
      SDValue Neg = DAG.getNegative(CondV, DL, VT);
      return DAG.getNode(ISD::AND, DL, VT, Neg, TrueV);
    }
  }

  // If the CondV is the output of a SETCC node which operates on XLenVT inputs,
  // then merge the SETCC node into the lowered RISCVISD::SELECT_CC to take
  // advantage of the integer compare+branch instructions. i.e.:
  // (select (setcc lhs, rhs, cc), truev, falsev)
  // -> (riscvisd::select_cc lhs, rhs, cc, truev, falsev)
  if (CondV.getOpcode() == ISD::SETCC &&
      CondV.getOperand(0).getSimpleValueType() == XLenVT) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    ISD::CondCode CCVal = cast<CondCodeSDNode>(CondV.getOperand(2))->get();

    // Special case for a select of 2 constants that have a diffence of 1.
    // Normally this is done by DAGCombine, but if the select is introduced by
    // type legalization or op legalization, we miss it. Restricting to SETLT
    // case for now because that is what signed saturating add/sub need.
    // FIXME: We don't need the condition to be SETLT or even a SETCC,
    // but we would probably want to swap the true/false values if the condition
    // is SETGE/SETLE to avoid an XORI.
    if (isa<ConstantSDNode>(TrueV) && isa<ConstantSDNode>(FalseV) &&
        CCVal == ISD::SETLT) {
      const APInt &TrueVal = cast<ConstantSDNode>(TrueV)->getAPIntValue();
      const APInt &FalseVal = cast<ConstantSDNode>(FalseV)->getAPIntValue();
      if (TrueVal - 1 == FalseVal)
        return DAG.getNode(ISD::ADD, DL, VT, CondV, FalseV);
      if (TrueVal + 1 == FalseVal)
        return DAG.getNode(ISD::SUB, DL, VT, FalseV, CondV);
    }

    translateSetCCForBranch(DL, LHS, RHS, CCVal, DAG);
    // 1 < x ? x : 1 -> 0 < x ? x : 1
    if (isOneConstant(LHS) &&
        (CCVal == ISD::SETLT || CCVal == ISD::SETULT) && RHS == TrueV &&
        LHS == FalseV) {
      LHS = DAG.getConstant(0, DL, VT);
      // 0 <u x is the same as x != 0.
      if (CCVal == ISD::SETULT) {
        std::swap(LHS, RHS);
        CCVal = ISD::SETNE;
      }
    }

    // x <s -1 ? x : -1 -> x <s 0 ? x : -1
    if (isAllOnesConstant(RHS) && CCVal == ISD::SETLT && LHS == TrueV &&
        RHS == FalseV) {
      RHS = DAG.getConstant(0, DL, VT);
    }

    SDValue TargetCC = DAG.getCondCode(CCVal);

    if (isa<ConstantSDNode>(TrueV) && !isa<ConstantSDNode>(FalseV)) {
      // (select (setcc lhs, rhs, CC), constant, falsev)
      // -> (select (setcc lhs, rhs, InverseCC), falsev, constant)
      std::swap(TrueV, FalseV);
      TargetCC =
          DAG.getCondCode(ISD::getSetCCInverse(CCVal, LHS.getValueType()));
    }

    SDValue Ops[] = {LHS, RHS, TargetCC, TrueV, FalseV};
    return DAG.getNode(RISCVISD::SELECT_CC, DL, VT, Ops);
  }

  // Otherwise:
  // (select condv, truev, falsev)
  // -> (riscvisd::select_cc condv, zero, setne, truev, falsev)
  SDValue Zero = DAG.getConstant(0, DL, XLenVT);
  SDValue SetNE = DAG.getCondCode(ISD::SETNE);

  SDValue Ops[] = {CondV, Zero, SetNE, TrueV, FalseV};

  return DAG.getNode(RISCVISD::SELECT_CC, DL, VT, Ops);
}

SDValue RISCVTargetLowering::lowerBRCOND(SDValue Op, SelectionDAG &DAG) const {
  SDValue CondV = Op.getOperand(1);
  SDLoc DL(Op);
  MVT XLenVT = Subtarget.getXLenVT();

  if (CondV.getOpcode() == ISD::SETCC &&
      CondV.getOperand(0).getValueType() == XLenVT) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    ISD::CondCode CCVal = cast<CondCodeSDNode>(CondV.getOperand(2))->get();

    translateSetCCForBranch(DL, LHS, RHS, CCVal, DAG);

    SDValue TargetCC = DAG.getCondCode(CCVal);
    return DAG.getNode(RISCVISD::BR_CC, DL, Op.getValueType(), Op.getOperand(0),
                       LHS, RHS, TargetCC, Op.getOperand(2));
  }

  return DAG.getNode(RISCVISD::BR_CC, DL, Op.getValueType(), Op.getOperand(0),
                     CondV, DAG.getConstant(0, DL, XLenVT),
                     DAG.getCondCode(ISD::SETNE), Op.getOperand(2));
}

SDValue RISCVTargetLowering::lowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  RISCVMachineFunctionInfo *FuncInfo = MF.getInfo<RISCVMachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(MF.getDataLayout()));

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue RISCVTargetLowering::lowerFRAMEADDR(SDValue Op,
                                            SelectionDAG &DAG) const {
  const RISCVRegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setFrameAddressIsTaken(true);
  Register FrameReg = RI.getFrameRegister(MF);
  int XLenInBytes = Subtarget.getXLen() / 8;

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), DL, FrameReg, VT);
  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  while (Depth--) {
    int Offset = -(XLenInBytes * 2);
    SDValue Ptr = DAG.getNode(ISD::ADD, DL, VT, FrameAddr,
                              DAG.getIntPtrConstant(Offset, DL));
    FrameAddr =
        DAG.getLoad(VT, DL, DAG.getEntryNode(), Ptr, MachinePointerInfo());
  }
  return FrameAddr;
}

SDValue RISCVTargetLowering::lowerRETURNADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  const RISCVRegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setReturnAddressIsTaken(true);
  MVT XLenVT = Subtarget.getXLenVT();
  int XLenInBytes = Subtarget.getXLen() / 8;

  if (verifyReturnAddressArgumentIsConstant(Op, DAG))
    return SDValue();

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  if (Depth) {
    int Off = -XLenInBytes;
    SDValue FrameAddr = lowerFRAMEADDR(Op, DAG);
    SDValue Offset = DAG.getConstant(Off, DL, VT);
    return DAG.getLoad(VT, DL, DAG.getEntryNode(),
                       DAG.getNode(ISD::ADD, DL, VT, FrameAddr, Offset),
                       MachinePointerInfo());
  }

  // Return the value of the return address register, marking it an implicit
  // live-in.
  Register Reg = MF.addLiveIn(RI.getRARegister(), getRegClassFor(XLenVT));
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, XLenVT);
}

SDValue RISCVTargetLowering::lowerShiftLeftParts(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // if Shamt-XLEN < 0: // Shamt < XLEN
  //   Lo = Lo << Shamt
  //   Hi = (Hi << Shamt) | ((Lo >>u 1) >>u (XLEN-1 ^ Shamt))
  // else:
  //   Lo = 0
  //   Hi = Lo << (Shamt-XLEN)

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::XOR, DL, VT, Shamt, XLenMinus1);

  SDValue LoTrue = DAG.getNode(ISD::SHL, DL, VT, Lo, Shamt);
  SDValue ShiftRight1Lo = DAG.getNode(ISD::SRL, DL, VT, Lo, One);
  SDValue ShiftRightLo =
      DAG.getNode(ISD::SRL, DL, VT, ShiftRight1Lo, XLenMinus1Shamt);
  SDValue ShiftLeftHi = DAG.getNode(ISD::SHL, DL, VT, Hi, Shamt);
  SDValue HiTrue = DAG.getNode(ISD::OR, DL, VT, ShiftLeftHi, ShiftRightLo);
  SDValue HiFalse = DAG.getNode(ISD::SHL, DL, VT, Lo, ShamtMinusXLen);

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, Zero);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue RISCVTargetLowering::lowerShiftRightParts(SDValue Op, SelectionDAG &DAG,
                                                  bool IsSRA) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // SRA expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (ShAmt ^ XLEN-1))
  //     Hi = Hi >>s Shamt
  //   else:
  //     Lo = Hi >>s (Shamt-XLEN);
  //     Hi = Hi >>s (XLEN-1)
  //
  // SRL expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (ShAmt ^ XLEN-1))
  //     Hi = Hi >>u Shamt
  //   else:
  //     Lo = Hi >>u (Shamt-XLEN);
  //     Hi = 0;

  unsigned ShiftRightOp = IsSRA ? ISD::SRA : ISD::SRL;

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::XOR, DL, VT, Shamt, XLenMinus1);

  SDValue ShiftRightLo = DAG.getNode(ISD::SRL, DL, VT, Lo, Shamt);
  SDValue ShiftLeftHi1 = DAG.getNode(ISD::SHL, DL, VT, Hi, One);
  SDValue ShiftLeftHi =
      DAG.getNode(ISD::SHL, DL, VT, ShiftLeftHi1, XLenMinus1Shamt);
  SDValue LoTrue = DAG.getNode(ISD::OR, DL, VT, ShiftRightLo, ShiftLeftHi);
  SDValue HiTrue = DAG.getNode(ShiftRightOp, DL, VT, Hi, Shamt);
  SDValue LoFalse = DAG.getNode(ShiftRightOp, DL, VT, Hi, ShamtMinusXLen);
  SDValue HiFalse =
      IsSRA ? DAG.getNode(ISD::SRA, DL, VT, Hi, XLenMinus1) : Zero;

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, LoFalse);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue RISCVTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                     SelectionDAG &DAG) const {
  unsigned IntNo = Op.getConstantOperandVal(0);
  SDLoc DL(Op);
  MVT XLenVT = Subtarget.getXLenVT();

  switch (IntNo) {
  default:
    break; // Don't custom lower most intrinsics.
  case Intrinsic::thread_pointer: {
    EVT PtrVT = getPointerTy(DAG.getDataLayout());
    return DAG.getRegister(RISCV::X4, PtrVT);
  }
  case Intrinsic::riscv_orc_b:
  case Intrinsic::riscv_brev8: {
    unsigned Opc =
        IntNo == Intrinsic::riscv_brev8 ? RISCVISD::BREV8 : RISCVISD::ORC_B;
    return DAG.getNode(Opc, DL, XLenVT, Op.getOperand(1));
  }
  case Intrinsic::riscv_zip:
  case Intrinsic::riscv_unzip: {
    unsigned Opc =
        IntNo == Intrinsic::riscv_zip ? RISCVISD::ZIP : RISCVISD::UNZIP;
    return DAG.getNode(Opc, DL, XLenVT, Op.getOperand(1));
  }
  case Intrinsic::riscv_vmv_x_s:
    assert(Op.getValueType() == XLenVT && "Unexpected VT!");
    return DAG.getNode(RISCVISD::VMV_X_S, DL, Op.getValueType(),
                       Op.getOperand(1));
  case Intrinsic::riscv_vmv_v_x:
    assert(0 && "sGPR to vGPR move!");
  case Intrinsic::riscv_vfmv_v_f:
    return DAG.getNode(RISCVISD::VFMV_V_F_VL, DL, Op.getValueType(),
                       Op.getOperand(1), Op.getOperand(2), Op.getOperand(3));
  case Intrinsic::riscv_vmv_s_x:
    assert(0 && "vGPR to sGPR move!");
  }
}

SDValue RISCVTargetLowering::lowerGET_ROUNDING(SDValue Op,
                                               SelectionDAG &DAG) const {
  const MVT XLenVT = Subtarget.getXLenVT();
  SDLoc DL(Op);
  SDValue Chain = Op->getOperand(0);
  SDValue SysRegNo = DAG.getTargetConstant(
      RISCVSysReg::lookupSysRegByName("FRM")->Encoding, DL, XLenVT);
  SDVTList VTs = DAG.getVTList(XLenVT, MVT::Other);
  SDValue RM = DAG.getNode(RISCVISD::READ_CSR, DL, VTs, Chain, SysRegNo);

  // Encoding used for rounding mode in RISCV differs from that used in
  // FLT_ROUNDS. To convert it the RISCV rounding mode is used as an index in a
  // table, which consists of a sequence of 4-bit fields, each representing
  // corresponding FLT_ROUNDS mode.
  static const int Table =
      (int(RoundingMode::NearestTiesToEven) << 4 * RISCVFPRndMode::RNE) |
      (int(RoundingMode::TowardZero) << 4 * RISCVFPRndMode::RTZ) |
      (int(RoundingMode::TowardNegative) << 4 * RISCVFPRndMode::RDN) |
      (int(RoundingMode::TowardPositive) << 4 * RISCVFPRndMode::RUP) |
      (int(RoundingMode::NearestTiesToAway) << 4 * RISCVFPRndMode::RMM);

  SDValue Shift =
      DAG.getNode(ISD::SHL, DL, XLenVT, RM, DAG.getConstant(2, DL, XLenVT));
  SDValue Shifted = DAG.getNode(ISD::SRL, DL, XLenVT,
                                DAG.getConstant(Table, DL, XLenVT), Shift);
  SDValue Masked = DAG.getNode(ISD::AND, DL, XLenVT, Shifted,
                               DAG.getConstant(7, DL, XLenVT));

  return DAG.getMergeValues({Masked, Chain}, DL);
}

SDValue RISCVTargetLowering::lowerSET_ROUNDING(SDValue Op,
                                               SelectionDAG &DAG) const {
  const MVT XLenVT = Subtarget.getXLenVT();
  SDLoc DL(Op);
  SDValue Chain = Op->getOperand(0);
  SDValue RMValue = Op->getOperand(1);
  SDValue SysRegNo = DAG.getTargetConstant(
      RISCVSysReg::lookupSysRegByName("FRM")->Encoding, DL, XLenVT);

  // Encoding used for rounding mode in RISCV differs from that used in
  // FLT_ROUNDS. To convert it the C rounding mode is used as an index in
  // a table, which consists of a sequence of 4-bit fields, each representing
  // corresponding RISCV mode.
  static const unsigned Table =
      (RISCVFPRndMode::RNE << 4 * int(RoundingMode::NearestTiesToEven)) |
      (RISCVFPRndMode::RTZ << 4 * int(RoundingMode::TowardZero)) |
      (RISCVFPRndMode::RDN << 4 * int(RoundingMode::TowardNegative)) |
      (RISCVFPRndMode::RUP << 4 * int(RoundingMode::TowardPositive)) |
      (RISCVFPRndMode::RMM << 4 * int(RoundingMode::NearestTiesToAway));

  SDValue Shift = DAG.getNode(ISD::SHL, DL, XLenVT, RMValue,
                              DAG.getConstant(2, DL, XLenVT));
  SDValue Shifted = DAG.getNode(ISD::SRL, DL, XLenVT,
                                DAG.getConstant(Table, DL, XLenVT), Shift);
  RMValue = DAG.getNode(ISD::AND, DL, XLenVT, Shifted,
                        DAG.getConstant(0x7, DL, XLenVT));
  return DAG.getNode(RISCVISD::WRITE_CSR, DL, MVT::Other, Chain, SysRegNo,
                     RMValue);
}

SDValue RISCVTargetLowering::lowerEH_DWARF_CFA(SDValue Op,
                                               SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  bool isRISCV64 = Subtarget.is64Bit();
  EVT PtrVT = getPointerTy(DAG.getDataLayout());

  int FI = MF.getFrameInfo().CreateFixedObject(isRISCV64 ? 8 : 4, 0, false);
  return DAG.getFrameIndex(FI, PtrVT);
}

// Returns the opcode of the target-specific SDNode that implements the 32-bit
// form of the given Opcode.
static RISCVISD::NodeType getRISCVWOpcode(unsigned Opcode) {
  switch (Opcode) {
  default:
    llvm_unreachable("Unexpected opcode");
  case ISD::SHL:
    return RISCVISD::SLLW;
  case ISD::SRA:
    return RISCVISD::SRAW;
  case ISD::SRL:
    return RISCVISD::SRLW;
  case ISD::SDIV:
    return RISCVISD::DIVW;
  case ISD::UDIV:
    return RISCVISD::DIVUW;
  case ISD::UREM:
    return RISCVISD::REMUW;
  case ISD::ROTL:
    return RISCVISD::ROLW;
  case ISD::ROTR:
    return RISCVISD::RORW;
  }
}

// Converts the given i8/i16/i32 operation to a target-specific SelectionDAG
// node. Because i8/i16/i32 isn't a legal type for RV64, these operations would
// otherwise be promoted to i64, making it difficult to select the
// SLLW/DIVUW/.../*W later one because the fact the operation was originally of
// type i8/i16/i32 is lost.
static SDValue customLegalizeToWOp(SDNode *N, SelectionDAG &DAG,
                                   unsigned ExtOpc = ISD::ANY_EXTEND) {
  SDLoc DL(N);
  RISCVISD::NodeType WOpcode = getRISCVWOpcode(N->getOpcode());
  SDValue NewOp0 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(1));
  SDValue NewRes = DAG.getNode(WOpcode, DL, MVT::i64, NewOp0, NewOp1);
  // ReplaceNodeResults requires we maintain the same type for the return value.
  return DAG.getNode(ISD::TRUNCATE, DL, N->getValueType(0), NewRes);
}

// Converts the given 32-bit operation to a i64 operation with signed extension
// semantic to reduce the signed extension instructions.
static SDValue customLegalizeToWOpWithSExt(SDNode *N, SelectionDAG &DAG) {
  SDLoc DL(N);
  SDValue NewOp0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(1));
  SDValue NewWOp = DAG.getNode(N->getOpcode(), DL, MVT::i64, NewOp0, NewOp1);
  SDValue NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, NewWOp,
                               DAG.getValueType(MVT::i32));
  return DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes);
}

void RISCVTargetLowering::ReplaceNodeResults(SDNode *N,
                                             SmallVectorImpl<SDValue> &Results,
                                             SelectionDAG &DAG) const {
  SDLoc DL(N);
  switch (N->getOpcode()) {
  default:
    llvm_unreachable("Don't know how to custom type legalize this operation!");
  case ISD::STRICT_FP_TO_SINT:
  case ISD::STRICT_FP_TO_UINT:
  case ISD::FP_TO_SINT:
  case ISD::FP_TO_UINT: {
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    bool IsStrict = N->isStrictFPOpcode();
    bool IsSigned = N->getOpcode() == ISD::FP_TO_SINT ||
                    N->getOpcode() == ISD::STRICT_FP_TO_SINT;
    SDValue Op0 = IsStrict ? N->getOperand(1) : N->getOperand(0);
    if (getTypeAction(*DAG.getContext(), Op0.getValueType()) !=
        TargetLowering::TypeSoftenFloat) {
      if (!isTypeLegal(Op0.getValueType()))
        return;
      if (IsStrict) {
        unsigned Opc = IsSigned ? RISCVISD::STRICT_FCVT_W_RV64
                                : RISCVISD::STRICT_FCVT_WU_RV64;
        SDVTList VTs = DAG.getVTList(MVT::i64, MVT::Other);
        SDValue Res = DAG.getNode(
            Opc, DL, VTs, N->getOperand(0), Op0,
            DAG.getTargetConstant(RISCVFPRndMode::RTZ, DL, MVT::i64));
        Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
        Results.push_back(Res.getValue(1));
        return;
      }
      unsigned Opc = IsSigned ? RISCVISD::FCVT_W_RV64 : RISCVISD::FCVT_WU_RV64;
      SDValue Res =
          DAG.getNode(Opc, DL, MVT::i64, Op0,
                      DAG.getTargetConstant(RISCVFPRndMode::RTZ, DL, MVT::i64));
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
      return;
    }
    // If the FP type needs to be softened, emit a library call using the 'si'
    // version. If we left it to default legalization we'd end up with 'di'. If
    // the FP type doesn't need to be softened just let generic type
    // legalization promote the result type.
    RTLIB::Libcall LC;
    if (IsSigned)
      LC = RTLIB::getFPTOSINT(Op0.getValueType(), N->getValueType(0));
    else
      LC = RTLIB::getFPTOUINT(Op0.getValueType(), N->getValueType(0));
    MakeLibCallOptions CallOptions;
    EVT OpVT = Op0.getValueType();
    CallOptions.setTypeListBeforeSoften(OpVT, N->getValueType(0), true);
    SDValue Chain = IsStrict ? N->getOperand(0) : SDValue();
    SDValue Result;
    std::tie(Result, Chain) =
        makeLibCall(DAG, LC, N->getValueType(0), Op0, CallOptions, DL, Chain);
    Results.push_back(Result);
    if (IsStrict)
      Results.push_back(Chain);
    break;
  }
  case ISD::READCYCLECOUNTER: {
    assert(!Subtarget.is64Bit() &&
           "READCYCLECOUNTER only has custom type legalization on riscv32");

    SDVTList VTs = DAG.getVTList(MVT::i32, MVT::i32, MVT::Other);
    SDValue RCW =
        DAG.getNode(RISCVISD::READ_CYCLE_WIDE, DL, VTs, N->getOperand(0));

    Results.push_back(
        DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i64, RCW, RCW.getValue(1)));
    Results.push_back(RCW.getValue(2));
    break;
  }
  case ISD::LOAD: {
    if (!ISD::isNON_EXTLoad(N))
      return;

    // Use a SEXTLOAD instead of the default EXTLOAD. Similar to the
    // sext_inreg we emit for ADD/SUB/MUL/SLLI.
    LoadSDNode *Ld = cast<LoadSDNode>(N);

    SDLoc dl(N);
    SDValue Res = DAG.getExtLoad(ISD::SEXTLOAD, dl, MVT::i64, Ld->getChain(),
                                 Ld->getBasePtr(), Ld->getMemoryVT(),
                                 Ld->getMemOperand());
    Results.push_back(DAG.getNode(ISD::TRUNCATE, dl, MVT::i32, Res));
    Results.push_back(Res.getValue(1));
    return;
  }
  case ISD::MUL: {
    unsigned Size = N->getSimpleValueType(0).getSizeInBits();
    unsigned XLen = Subtarget.getXLen();
    // This multiply needs to be expanded, try to use MULHSU+MUL if possible.
    if (Size > XLen) {
      assert(Size == (XLen * 2) && "Unexpected custom legalisation");
      SDValue LHS = N->getOperand(0);
      SDValue RHS = N->getOperand(1);
      APInt HighMask = APInt::getHighBitsSet(Size, XLen);

      bool LHSIsU = DAG.MaskedValueIsZero(LHS, HighMask);
      bool RHSIsU = DAG.MaskedValueIsZero(RHS, HighMask);
      // We need exactly one side to be unsigned.
      if (LHSIsU == RHSIsU)
        return;

      auto MakeMULPair = [&](SDValue S, SDValue U) {
        MVT XLenVT = Subtarget.getXLenVT();
        S = DAG.getNode(ISD::TRUNCATE, DL, XLenVT, S);
        U = DAG.getNode(ISD::TRUNCATE, DL, XLenVT, U);
        SDValue Lo = DAG.getNode(ISD::MUL, DL, XLenVT, S, U);
        SDValue Hi = DAG.getNode(RISCVISD::MULHSU, DL, XLenVT, S, U);
        return DAG.getNode(ISD::BUILD_PAIR, DL, N->getValueType(0), Lo, Hi);
      };

      bool LHSIsS = DAG.ComputeNumSignBits(LHS) > XLen;
      bool RHSIsS = DAG.ComputeNumSignBits(RHS) > XLen;

      // The other operand should be signed, but still prefer MULH when
      // possible.
      if (RHSIsU && LHSIsS && !RHSIsS)
        Results.push_back(MakeMULPair(LHS, RHS));
      else if (LHSIsU && RHSIsS && !LHSIsS)
        Results.push_back(MakeMULPair(RHS, LHS));

      return;
    }
    [[fallthrough]];
  }
  case ISD::ADD:
  case ISD::SUB:
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    Results.push_back(customLegalizeToWOpWithSExt(N, DAG));
    break;
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    if (N->getOperand(1).getOpcode() != ISD::Constant) {
      // If we can use a BSET instruction, allow default promotion to apply.
      if (N->getOpcode() == ISD::SHL && Subtarget.hasStdExtZbs() &&
          isOneConstant(N->getOperand(0)))
        break;
      Results.push_back(customLegalizeToWOp(N, DAG));
      break;
    }

    // Custom legalize ISD::SHL by placing a SIGN_EXTEND_INREG after. This is
    // similar to customLegalizeToWOpWithSExt, but we must zero_extend the
    // shift amount.
    if (N->getOpcode() == ISD::SHL) {
      SDLoc DL(N);
      SDValue NewOp0 =
          DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
      SDValue NewOp1 =
          DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, N->getOperand(1));
      SDValue NewWOp = DAG.getNode(ISD::SHL, DL, MVT::i64, NewOp0, NewOp1);
      SDValue NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, NewWOp,
                                   DAG.getValueType(MVT::i32));
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes));
    }

    break;
  case ISD::ROTL:
  case ISD::ROTR:
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    Results.push_back(customLegalizeToWOp(N, DAG));
    break;
  case ISD::CTTZ:
  case ISD::CTTZ_ZERO_UNDEF:
  case ISD::CTLZ:
  case ISD::CTLZ_ZERO_UNDEF: {
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");

    SDValue NewOp0 =
        DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
    bool IsCTZ =
        N->getOpcode() == ISD::CTTZ || N->getOpcode() == ISD::CTTZ_ZERO_UNDEF;
    unsigned Opc = IsCTZ ? RISCVISD::CTZW : RISCVISD::CLZW;
    SDValue Res = DAG.getNode(Opc, DL, MVT::i64, NewOp0);
    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
    return;
  }
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::UREM: {
    MVT VT = N->getSimpleValueType(0);
    assert((VT == MVT::i8 || VT == MVT::i16 || VT == MVT::i32) &&
           Subtarget.is64Bit() && Subtarget.hasStdExtM() &&
           "Unexpected custom legalisation");
    // Don't promote division/remainder by constant since we should expand those
    // to multiply by magic constant.
    AttributeList Attr = DAG.getMachineFunction().getFunction().getAttributes();
    if (N->getOperand(1).getOpcode() == ISD::Constant &&
        !isIntDivCheap(N->getValueType(0), Attr))
      return;

    // If the input is i32, use ANY_EXTEND since the W instructions don't read
    // the upper 32 bits. For other types we need to sign or zero extend
    // based on the opcode.
    unsigned ExtOpc = ISD::ANY_EXTEND;
    if (VT != MVT::i32)
      ExtOpc = N->getOpcode() == ISD::SDIV ? ISD::SIGN_EXTEND
                                           : ISD::ZERO_EXTEND;

    Results.push_back(customLegalizeToWOp(N, DAG, ExtOpc));
    break;
  }
  case ISD::UADDO:
  case ISD::USUBO: {
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    bool IsAdd = N->getOpcode() == ISD::UADDO;
    // Create an ADDW or SUBW.
    SDValue LHS = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
    SDValue RHS = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(1));
    SDValue Res =
        DAG.getNode(IsAdd ? ISD::ADD : ISD::SUB, DL, MVT::i64, LHS, RHS);
    Res = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, Res,
                      DAG.getValueType(MVT::i32));

    SDValue Overflow;
    if (IsAdd && isOneConstant(RHS)) {
      // Special case uaddo X, 1 overflowed if the addition result is 0.
      // The general case (X + C) < C is not necessarily beneficial. Although we
      // reduce the live range of X, we may introduce the materialization of
      // constant C, especially when the setcc result is used by branch. We have
      // no compare with constant and branch instructions.
      Overflow = DAG.getSetCC(DL, N->getValueType(1), Res,
                              DAG.getConstant(0, DL, MVT::i64), ISD::SETEQ);
    } else {
      // Sign extend the LHS and perform an unsigned compare with the ADDW
      // result. Since the inputs are sign extended from i32, this is equivalent
      // to comparing the lower 32 bits.
      LHS = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, N->getOperand(0));
      Overflow = DAG.getSetCC(DL, N->getValueType(1), Res, LHS,
                              IsAdd ? ISD::SETULT : ISD::SETUGT);
    }

    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
    Results.push_back(Overflow);
    return;
  }
  case ISD::UADDSAT:
  case ISD::USUBSAT: {
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");
    if (Subtarget.hasStdExtZbb()) {
      // With Zbb we can sign extend and let LegalizeDAG use minu/maxu. Using
      // sign extend allows overflow of the lower 32 bits to be detected on
      // the promoted size.
      SDValue LHS =
          DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, N->getOperand(0));
      SDValue RHS =
          DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, N->getOperand(1));
      SDValue Res = DAG.getNode(N->getOpcode(), DL, MVT::i64, LHS, RHS);
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
      return;
    }

    // Without Zbb, expand to UADDO/USUBO+select which will trigger our custom
    // promotion for UADDO/USUBO.
    Results.push_back(expandAddSubSat(N, DAG));
    return;
  }
  case ISD::ABS: {
    assert(N->getValueType(0) == MVT::i32 && Subtarget.is64Bit() &&
           "Unexpected custom legalisation");

    if (Subtarget.hasStdExtZbb()) {
      // Emit a special ABSW node that will be expanded to NEGW+MAX at isel.
      // This allows us to remember that the result is sign extended. Expanding
      // to NEGW+MAX here requires a Freeze which breaks ComputeNumSignBits.
      SDValue Src = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64,
                                N->getOperand(0));
      SDValue Abs = DAG.getNode(RISCVISD::ABSW, DL, MVT::i64, Src);
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Abs));
      return;
    }

    // Expand abs to Y = (sraiw X, 31); subw(xor(X, Y), Y)
    SDValue Src = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));

    // Freeze the source so we can increase it's use count.
    Src = DAG.getFreeze(Src);

    // Copy sign bit to all bits using the sraiw pattern.
    SDValue SignFill = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, Src,
                                   DAG.getValueType(MVT::i32));
    SignFill = DAG.getNode(ISD::SRA, DL, MVT::i64, SignFill,
                           DAG.getConstant(31, DL, MVT::i64));

    SDValue NewRes = DAG.getNode(ISD::XOR, DL, MVT::i64, Src, SignFill);
    NewRes = DAG.getNode(ISD::SUB, DL, MVT::i64, NewRes, SignFill);

    // NOTE: The result is only required to be anyextended, but sext is
    // consistent with type legalization of sub.
    NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, NewRes,
                         DAG.getValueType(MVT::i32));
    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes));
    return;
  }
  case ISD::BITCAST: {
    EVT VT = N->getValueType(0);
    assert(VT.isInteger() && !VT.isVector() && "Unexpected VT!");
    SDValue Op0 = N->getOperand(0);
    EVT Op0VT = Op0.getValueType();
    MVT XLenVT = Subtarget.getXLenVT();
    if (VT == MVT::i16 && Op0VT == MVT::f16 && Subtarget.hasStdExtZfh()) {
      SDValue FPConv = DAG.getNode(RISCVISD::FMV_X_ANYEXTH, DL, XLenVT, Op0);
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i16, FPConv));
    } else if (VT == MVT::i32 && Op0VT == MVT::f32 && Subtarget.is64Bit() &&
               Subtarget.hasStdExtF()) {
      SDValue FPConv =
          DAG.getNode(RISCVISD::FMV_X_ANYEXTW_RV64, DL, MVT::i64, Op0);
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, FPConv));
    } else if (!VT.isVector() && Op0VT.isFixedLengthVector() &&
               isTypeLegal(Op0VT)) {
      // Custom-legalize bitcasts from fixed-length vector types to illegal
      // scalar types in order to improve codegen. Bitcast the vector to a
      // one-element vector type whose element type is the same as the result
      // type, and extract the first element.
      EVT BVT = EVT::getVectorVT(*DAG.getContext(), VT, 1);
      if (isTypeLegal(BVT)) {
        SDValue BVec = DAG.getBitcast(BVT, Op0);
        Results.push_back(DAG.getNode(ISD::EXTRACT_VECTOR_ELT, DL, VT, BVec,
                                      DAG.getConstant(0, DL, XLenVT)));
      }
    }
    break;
  }
  case RISCVISD::BREV8: {
    MVT VT = N->getSimpleValueType(0);
    MVT XLenVT = Subtarget.getXLenVT();
    assert((VT == MVT::i16 || (VT == MVT::i32 && Subtarget.is64Bit())) &&
           "Unexpected custom legalisation");
    assert(Subtarget.hasStdExtZbkb() && "Unexpected extension");
    SDValue NewOp = DAG.getNode(ISD::ANY_EXTEND, DL, XLenVT, N->getOperand(0));
    SDValue NewRes = DAG.getNode(N->getOpcode(), DL, XLenVT, NewOp);
    // ReplaceNodeResults requires we maintain the same type for the return
    // value.
    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, VT, NewRes));
    break;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    unsigned IntNo = cast<ConstantSDNode>(N->getOperand(0))->getZExtValue();
    switch (IntNo) {
    default:
      llvm_unreachable(
          "Don't know how to custom type legalize this intrinsic!");
    case Intrinsic::riscv_orc_b: {
      SDValue NewOp =
          DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(1));
      SDValue Res = DAG.getNode(RISCVISD::ORC_B, DL, MVT::i64, NewOp);
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
      return;
    }
    }
    break;
  }
  case ISD::FLT_ROUNDS_: {
    SDVTList VTs = DAG.getVTList(Subtarget.getXLenVT(), MVT::Other);
    SDValue Res = DAG.getNode(ISD::FLT_ROUNDS_, DL, VTs, N->getOperand(0));
    Results.push_back(Res.getValue(0));
    Results.push_back(Res.getValue(1));
    break;
  }
  }
}

// Optimize (add (shl x, c0), (shl y, c1)) ->
//          (SLLI (SH*ADD x, y), c0), if c1-c0 equals to [1|2|3].
static SDValue transformAddShlImm(SDNode *N, SelectionDAG &DAG,
                                  const RISCVSubtarget &Subtarget) {
  // Perform this optimization only in the zba extension.
  if (!Subtarget.hasStdExtZba())
    return SDValue();

  // Skip for vector types and larger types.
  EVT VT = N->getValueType(0);
  if (VT.isVector() || VT.getSizeInBits() > Subtarget.getXLen())
    return SDValue();

  // The two operand nodes must be SHL and have no other use.
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  if (N0->getOpcode() != ISD::SHL || N1->getOpcode() != ISD::SHL ||
      !N0->hasOneUse() || !N1->hasOneUse())
    return SDValue();

  // Check c0 and c1.
  auto *N0C = dyn_cast<ConstantSDNode>(N0->getOperand(1));
  auto *N1C = dyn_cast<ConstantSDNode>(N1->getOperand(1));
  if (!N0C || !N1C)
    return SDValue();
  int64_t C0 = N0C->getSExtValue();
  int64_t C1 = N1C->getSExtValue();
  if (C0 <= 0 || C1 <= 0)
    return SDValue();

  // Skip if SH1ADD/SH2ADD/SH3ADD are not applicable.
  int64_t Bits = std::min(C0, C1);
  int64_t Diff = std::abs(C0 - C1);
  if (Diff != 1 && Diff != 2 && Diff != 3)
    return SDValue();

  // Build nodes.
  SDLoc DL(N);
  SDValue NS = (C0 < C1) ? N0->getOperand(0) : N1->getOperand(0);
  SDValue NL = (C0 > C1) ? N0->getOperand(0) : N1->getOperand(0);
  SDValue NA0 =
      DAG.getNode(ISD::SHL, DL, VT, NL, DAG.getConstant(Diff, DL, VT));
  SDValue NA1 = DAG.getNode(ISD::ADD, DL, VT, NA0, NS);
  return DAG.getNode(ISD::SHL, DL, VT, NA1, DAG.getConstant(Bits, DL, VT));
}

// Combine a constant select operand into its use:
//
// (and (select cond, -1, c), x)
//   -> (select cond, x, (and x, c))  [AllOnes=1]
// (or  (select cond, 0, c), x)
//   -> (select cond, x, (or x, c))  [AllOnes=0]
// (xor (select cond, 0, c), x)
//   -> (select cond, x, (xor x, c))  [AllOnes=0]
// (add (select cond, 0, c), x)
//   -> (select cond, x, (add x, c))  [AllOnes=0]
// (sub x, (select cond, 0, c))
//   -> (select cond, x, (sub x, c))  [AllOnes=0]
static SDValue combineSelectAndUse(SDNode *N, SDValue Slct, SDValue OtherOp,
                                   SelectionDAG &DAG, bool AllOnes) {
  EVT VT = N->getValueType(0);

  // Skip vectors.
  if (VT.isVector())
    return SDValue();

  if ((Slct.getOpcode() != ISD::SELECT &&
       Slct.getOpcode() != RISCVISD::SELECT_CC) ||
      !Slct.hasOneUse())
    return SDValue();

  auto isZeroOrAllOnes = [](SDValue N, bool AllOnes) {
    return AllOnes ? isAllOnesConstant(N) : isNullConstant(N);
  };

  bool SwapSelectOps;
  unsigned OpOffset = Slct.getOpcode() == RISCVISD::SELECT_CC ? 2 : 0;
  SDValue TrueVal = Slct.getOperand(1 + OpOffset);
  SDValue FalseVal = Slct.getOperand(2 + OpOffset);
  SDValue NonConstantVal;
  if (isZeroOrAllOnes(TrueVal, AllOnes)) {
    SwapSelectOps = false;
    NonConstantVal = FalseVal;
  } else if (isZeroOrAllOnes(FalseVal, AllOnes)) {
    SwapSelectOps = true;
    NonConstantVal = TrueVal;
  } else
    return SDValue();

  // Slct is now know to be the desired identity constant when CC is true.
  TrueVal = OtherOp;
  FalseVal = DAG.getNode(N->getOpcode(), SDLoc(N), VT, OtherOp, NonConstantVal);
  // Unless SwapSelectOps says the condition should be false.
  if (SwapSelectOps)
    std::swap(TrueVal, FalseVal);

  if (Slct.getOpcode() == RISCVISD::SELECT_CC)
    return DAG.getNode(RISCVISD::SELECT_CC, SDLoc(N), VT,
                       {Slct.getOperand(0), Slct.getOperand(1),
                        Slct.getOperand(2), TrueVal, FalseVal});

  return DAG.getNode(ISD::SELECT, SDLoc(N), VT,
                     {Slct.getOperand(0), TrueVal, FalseVal});
}

// Attempt combineSelectAndUse on each operand of a commutative operator N.
static SDValue combineSelectAndUseCommutative(SDNode *N, SelectionDAG &DAG,
                                              bool AllOnes) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  if (SDValue Result = combineSelectAndUse(N, N0, N1, DAG, AllOnes))
    return Result;
  if (SDValue Result = combineSelectAndUse(N, N1, N0, DAG, AllOnes))
    return Result;
  return SDValue();
}

// Transform (add (mul x, c0), c1) ->
//           (add (mul (add x, c1/c0), c0), c1%c0).
// if c1/c0 and c1%c0 are simm12, while c1 is not. A special corner case
// that should be excluded is when c0*(c1/c0) is simm12, which will lead
// to an infinite loop in DAGCombine if transformed.
// Or transform (add (mul x, c0), c1) ->
//              (add (mul (add x, c1/c0+1), c0), c1%c0-c0),
// if c1/c0+1 and c1%c0-c0 are simm12, while c1 is not. A special corner
// case that should be excluded is when c0*(c1/c0+1) is simm12, which will
// lead to an infinite loop in DAGCombine if transformed.
// Or transform (add (mul x, c0), c1) ->
//              (add (mul (add x, c1/c0-1), c0), c1%c0+c0),
// if c1/c0-1 and c1%c0+c0 are simm12, while c1 is not. A special corner
// case that should be excluded is when c0*(c1/c0-1) is simm12, which will
// lead to an infinite loop in DAGCombine if transformed.
// Or transform (add (mul x, c0), c1) ->
//              (mul (add x, c1/c0), c0).
// if c1%c0 is zero, and c1/c0 is simm12 while c1 is not.
static SDValue transformAddImmMulImm(SDNode *N, SelectionDAG &DAG,
                                     const RISCVSubtarget &Subtarget) {
  // Skip for vector types and larger types.
  EVT VT = N->getValueType(0);
  if (VT.isVector() || VT.getSizeInBits() > Subtarget.getXLen())
    return SDValue();
  // The first operand node must be a MUL and has no other use.
  SDValue N0 = N->getOperand(0);
  if (!N0->hasOneUse() || N0->getOpcode() != ISD::MUL)
    return SDValue();
  // Check if c0 and c1 match above conditions.
  auto *N0C = dyn_cast<ConstantSDNode>(N0->getOperand(1));
  auto *N1C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!N0C || !N1C)
    return SDValue();
  // If N0C has multiple uses it's possible one of the cases in
  // DAGCombiner::isMulAddWithConstProfitable will be true, which would result
  // in an infinite loop.
  if (!N0C->hasOneUse())
    return SDValue();
  int64_t C0 = N0C->getSExtValue();
  int64_t C1 = N1C->getSExtValue();
  int64_t CA, CB;
  if (C0 == -1 || C0 == 0 || C0 == 1 || isInt<12>(C1))
    return SDValue();
  // Search for proper CA (non-zero) and CB that both are simm12.
  if ((C1 / C0) != 0 && isInt<12>(C1 / C0) && isInt<12>(C1 % C0) &&
      !isInt<12>(C0 * (C1 / C0))) {
    CA = C1 / C0;
    CB = C1 % C0;
  } else if ((C1 / C0 + 1) != 0 && isInt<12>(C1 / C0 + 1) &&
             isInt<12>(C1 % C0 - C0) && !isInt<12>(C0 * (C1 / C0 + 1))) {
    CA = C1 / C0 + 1;
    CB = C1 % C0 - C0;
  } else if ((C1 / C0 - 1) != 0 && isInt<12>(C1 / C0 - 1) &&
             isInt<12>(C1 % C0 + C0) && !isInt<12>(C0 * (C1 / C0 - 1))) {
    CA = C1 / C0 - 1;
    CB = C1 % C0 + C0;
  } else
    return SDValue();
  // Build new nodes (add (mul (add x, c1/c0), c0), c1%c0).
  SDLoc DL(N);
  SDValue New0 = DAG.getNode(ISD::ADD, DL, VT, N0->getOperand(0),
                             DAG.getConstant(CA, DL, VT));
  SDValue New1 =
      DAG.getNode(ISD::MUL, DL, VT, New0, DAG.getConstant(C0, DL, VT));
  return DAG.getNode(ISD::ADD, DL, VT, New1, DAG.getConstant(CB, DL, VT));
}

static SDValue performADDCombine(SDNode *N, SelectionDAG &DAG,
                                 const RISCVSubtarget &Subtarget) {
  if (SDValue V = transformAddImmMulImm(N, DAG, Subtarget))
    return V;
  if (SDValue V = transformAddShlImm(N, DAG, Subtarget))
    return V;
  // fold (add (select lhs, rhs, cc, 0, y), x) ->
  //      (select lhs, rhs, cc, x, (add x, y))
  return combineSelectAndUseCommutative(N, DAG, /*AllOnes*/ false);
}

// Try to turn a sub boolean RHS and constant LHS into an addi.
static SDValue combineSubOfBoolean(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  EVT VT = N->getValueType(0);
  SDLoc DL(N);

  // Require a constant LHS.
  auto *N0C = dyn_cast<ConstantSDNode>(N0);
  if (!N0C)
    return SDValue();

  // All our optimizations involve subtracting 1 from the immediate and forming
  // an ADDI. Make sure the new immediate is valid for an ADDI.
  APInt ImmValMinus1 = N0C->getAPIntValue() - 1;
  if (!ImmValMinus1.isSignedIntN(12))
    return SDValue();

  SDValue NewLHS;
  if (N1.getOpcode() == ISD::SETCC && N1.hasOneUse()) {
    // (sub constant, (setcc x, y, eq/neq)) ->
    // (add (setcc x, y, neq/eq), constant - 1)
    ISD::CondCode CCVal = cast<CondCodeSDNode>(N1.getOperand(2))->get();
    EVT SetCCOpVT = N1.getOperand(0).getValueType();
    if (!isIntEqualitySetCC(CCVal) || !SetCCOpVT.isInteger())
      return SDValue();
    CCVal = ISD::getSetCCInverse(CCVal, SetCCOpVT);
    NewLHS =
        DAG.getSetCC(SDLoc(N1), VT, N1.getOperand(0), N1.getOperand(1), CCVal);
  } else if (N1.getOpcode() == ISD::XOR && isOneConstant(N1.getOperand(1)) &&
             N1.getOperand(0).getOpcode() == ISD::SETCC) {
    // (sub C, (xor (setcc), 1)) -> (add (setcc), C-1).
    // Since setcc returns a bool the xor is equivalent to 1-setcc.
    NewLHS = N1.getOperand(0);
  } else
    return SDValue();

  SDValue NewRHS = DAG.getConstant(ImmValMinus1, DL, VT);
  return DAG.getNode(ISD::ADD, DL, VT, NewLHS, NewRHS);
}

static SDValue performSUBCombine(SDNode *N, SelectionDAG &DAG) {
  if (SDValue V = combineSubOfBoolean(N, DAG))
    return V;

  // fold (sub x, (select lhs, rhs, cc, 0, y)) ->
  //      (select lhs, rhs, cc, x, (sub x, y))
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  return combineSelectAndUse(N, N1, N0, DAG, /*AllOnes*/ false);
}

// Apply DeMorgan's law to (and/or (xor X, 1), (xor Y, 1)) if X and Y are 0/1.
// Legalizing setcc can introduce xors like this. Doing this transform reduces
// the number of xors and may allow the xor to fold into a branch condition.
static SDValue combineDeMorganOfBoolean(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  bool IsAnd = N->getOpcode() == ISD::AND;

  if (N0.getOpcode() != ISD::XOR || N1.getOpcode() != ISD::XOR)
    return SDValue();

  if (!N0.hasOneUse() || !N1.hasOneUse())
    return SDValue();

  SDValue N01 = N0.getOperand(1);
  SDValue N11 = N1.getOperand(1);

  // For AND, SimplifyDemandedBits may have turned one of the (xor X, 1) into
  // (xor X, -1) based on the upper bits of the other operand being 0. If the
  // operation is And, allow one of the Xors to use -1.
  if (isOneConstant(N01)) {
    if (!isOneConstant(N11) && !(IsAnd && isAllOnesConstant(N11)))
      return SDValue();
  } else if (isOneConstant(N11)) {
    // N01 and N11 being 1 was already handled. Handle N11==1 and N01==-1.
    if (!(IsAnd && isAllOnesConstant(N01)))
      return SDValue();
  } else
    return SDValue();

  EVT VT = N->getValueType(0);

  SDValue N00 = N0.getOperand(0);
  SDValue N10 = N1.getOperand(0);

  // The LHS of the xors needs to be 0/1.
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), 1);
  if (!DAG.MaskedValueIsZero(N00, Mask) || !DAG.MaskedValueIsZero(N10, Mask))
    return SDValue();

  // Invert the opcode and insert a new xor.
  SDLoc DL(N);
  unsigned Opc = IsAnd ? ISD::OR : ISD::AND;
  SDValue Logic = DAG.getNode(Opc, DL, VT, N00, N10);
  return DAG.getNode(ISD::XOR, DL, VT, Logic, DAG.getConstant(1, DL, VT));
}

static SDValue performTRUNCATECombine(SDNode *N, SelectionDAG &DAG,
                                      const RISCVSubtarget &Subtarget) {
  SDValue N0 = N->getOperand(0);
  EVT VT = N->getValueType(0);

  // Pre-promote (i1 (truncate (srl X, Y))) on RV64 with Zbs without zero
  // extending X. This is safe since we only need the LSB after the shift and
  // shift amounts larger than 31 would produce poison. If we wait until
  // type legalization, we'll create RISCVISD::SRLW and we can't recover it
  // to use a BEXT instruction.
  if (Subtarget.is64Bit() && Subtarget.hasStdExtZbs() && VT == MVT::i1 &&
      N0.getValueType() == MVT::i32 && N0.getOpcode() == ISD::SRL &&
      !isa<ConstantSDNode>(N0.getOperand(1)) && N0.hasOneUse()) {
    SDLoc DL(N0);
    SDValue Op0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N0.getOperand(0));
    SDValue Op1 = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, N0.getOperand(1));
    SDValue Srl = DAG.getNode(ISD::SRL, DL, MVT::i64, Op0, Op1);
    return DAG.getNode(ISD::TRUNCATE, SDLoc(N), VT, Srl);
  }

  return SDValue();
}

static SDValue performANDCombine(SDNode *N,
                                 TargetLowering::DAGCombinerInfo &DCI,
                                 const RISCVSubtarget &Subtarget) {
  SelectionDAG &DAG = DCI.DAG;

  SDValue N0 = N->getOperand(0);
  // Pre-promote (i32 (and (srl X, Y), 1)) on RV64 with Zbs without zero
  // extending X. This is safe since we only need the LSB after the shift and
  // shift amounts larger than 31 would produce poison. If we wait until
  // type legalization, we'll create RISCVISD::SRLW and we can't recover it
  // to use a BEXT instruction.
  if (Subtarget.is64Bit() && Subtarget.hasStdExtZbs() &&
      N->getValueType(0) == MVT::i32 && isOneConstant(N->getOperand(1)) &&
      N0.getOpcode() == ISD::SRL && !isa<ConstantSDNode>(N0.getOperand(1)) &&
      N0.hasOneUse()) {
    SDLoc DL(N);
    SDValue Op0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N0.getOperand(0));
    SDValue Op1 = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, N0.getOperand(1));
    SDValue Srl = DAG.getNode(ISD::SRL, DL, MVT::i64, Op0, Op1);
    SDValue And = DAG.getNode(ISD::AND, DL, MVT::i64, Srl,
                              DAG.getConstant(1, DL, MVT::i64));
    return DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, And);
  }

  if (DCI.isAfterLegalizeDAG())
    if (SDValue V = combineDeMorganOfBoolean(N, DAG))
      return V;

  // fold (and (select lhs, rhs, cc, -1, y), x) ->
  //      (select lhs, rhs, cc, x, (and x, y))
  return combineSelectAndUseCommutative(N, DAG, /*AllOnes*/ true);
}

static SDValue performORCombine(SDNode *N, TargetLowering::DAGCombinerInfo &DCI,
                                const RISCVSubtarget &Subtarget) {
  SelectionDAG &DAG = DCI.DAG;

  if (DCI.isAfterLegalizeDAG())
    if (SDValue V = combineDeMorganOfBoolean(N, DAG))
      return V;

  // fold (or (select cond, 0, y), x) ->
  //      (select cond, x, (or x, y))
  return combineSelectAndUseCommutative(N, DAG, /*AllOnes*/ false);
}

static SDValue performXORCombine(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);

  // fold (xor (sllw 1, x), -1) -> (rolw ~1, x)
  // NOTE: Assumes ROL being legal means ROLW is legal.
  const TargetLowering &TLI = DAG.getTargetLoweringInfo();
  if (N0.getOpcode() == RISCVISD::SLLW &&
      isAllOnesConstant(N1) && isOneConstant(N0.getOperand(0)) &&
      TLI.isOperationLegal(ISD::ROTL, MVT::i64)) {
    SDLoc DL(N);
    return DAG.getNode(RISCVISD::ROLW, DL, MVT::i64,
                       DAG.getConstant(~1, DL, MVT::i64), N0.getOperand(1));
  }

  // fold (xor (select cond, 0, y), x) ->
  //      (select cond, x, (xor x, y))
  return combineSelectAndUseCommutative(N, DAG, /*AllOnes*/ false);
}

// Replace (seteq (i64 (and X, 0xffffffff)), C1) with
// (seteq (i64 (sext_inreg (X, i32)), C1')) where C1' is C1 sign extended from
// bit 31. Same for setne. C1' may be cheaper to materialize and the sext_inreg
// can become a sext.w instead of a shift pair.
static SDValue performSETCCCombine(SDNode *N, SelectionDAG &DAG,
                                   const RISCVSubtarget &Subtarget) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  EVT VT = N->getValueType(0);
  EVT OpVT = N0.getValueType();

  if (OpVT != MVT::i64 || !Subtarget.is64Bit())
    return SDValue();

  // RHS needs to be a constant.
  auto *N1C = dyn_cast<ConstantSDNode>(N1);
  if (!N1C)
    return SDValue();

  // LHS needs to be (and X, 0xffffffff).
  if (N0.getOpcode() != ISD::AND || !N0.hasOneUse() ||
      !isa<ConstantSDNode>(N0.getOperand(1)) ||
      N0.getConstantOperandVal(1) != UINT64_C(0xffffffff))
    return SDValue();

  // Looking for an equality compare.
  ISD::CondCode Cond = cast<CondCodeSDNode>(N->getOperand(2))->get();
  if (!isIntEqualitySetCC(Cond))
    return SDValue();

  // Don't do this if the sign bit is provably zero, it will be turned back into
  // an AND.
  APInt SignMask = APInt::getOneBitSet(64, 31);
  if (DAG.MaskedValueIsZero(N0.getOperand(0), SignMask))
    return SDValue();

  const APInt &C1 = N1C->getAPIntValue();

  SDLoc dl(N);
  // If the constant is larger than 2^32 - 1 it is impossible for both sides
  // to be equal.
  if (C1.getActiveBits() > 32)
    return DAG.getBoolConstant(Cond == ISD::SETNE, dl, VT, OpVT);

  SDValue SExtOp = DAG.getNode(ISD::SIGN_EXTEND_INREG, N, OpVT,
                               N0.getOperand(0), DAG.getValueType(MVT::i32));
  return DAG.getSetCC(dl, VT, SExtOp, DAG.getConstant(C1.trunc(32).sext(64),
                                                      dl, OpVT), Cond);
}

static SDValue
performSIGN_EXTEND_INREGCombine(SDNode *N, SelectionDAG &DAG,
                                const RISCVSubtarget &Subtarget) {
  SDValue Src = N->getOperand(0);
  EVT VT = N->getValueType(0);

  // Fold (sext_inreg (fmv_x_anyexth X), i16) -> (fmv_x_signexth X)
  if (Src.getOpcode() == RISCVISD::FMV_X_ANYEXTH &&
      cast<VTSDNode>(N->getOperand(1))->getVT().bitsGE(MVT::i16))
    return DAG.getNode(RISCVISD::FMV_X_SIGNEXTH, SDLoc(N), VT,
                       Src.getOperand(0));

  return SDValue();
}

namespace {
// Forward declaration of the structure holding the necessary information to
// apply a combine.
struct CombineResult;

/// Helper class for folding sign/zero extensions.
/// In particular, this class is used for the following combines:
/// add_vl -> vwadd(u) | vwadd(u)_w
/// sub_vl -> vwsub(u) | vwsub(u)_w
/// mul_vl -> vwmul(u) | vwmul_su
///
/// An object of this class represents an operand of the operation we want to
/// combine.
/// E.g., when trying to combine `mul_vl a, b`, we will have one instance of
/// NodeExtensionHelper for `a` and one for `b`.
///
/// This class abstracts away how the extension is materialized and
/// how its Mask, VL, number of users affect the combines.
///
/// In particular:
/// - VWADD_W is conceptually == add(op0, sext(op1))
/// - VWADDU_W == add(op0, zext(op1))
/// - VWSUB_W == sub(op0, sext(op1))
/// - VWSUBU_W == sub(op0, zext(op1))
///
/// And VMV_V_X_VL, depending on the value, is conceptually equivalent to
/// zext|sext(smaller_value).
struct NodeExtensionHelper {
  /// Records if this operand is like being zero extended.
  bool SupportsZExt;
  /// Records if this operand is like being sign extended.
  /// Note: SupportsZExt and SupportsSExt are not mutually exclusive. For
  /// instance, a splat constant (e.g., 3), would support being both sign and
  /// zero extended.
  bool SupportsSExt;
  /// This boolean captures whether we care if this operand would still be
  /// around after the folding happens.
  bool EnforceOneUse;
  /// Records if this operand's mask needs to match the mask of the operation
  /// that it will fold into.
  bool CheckMask;
  /// Value of the Mask for this operand.
  /// It may be SDValue().
  SDValue Mask;
  /// Value of the vector length operand.
  /// It may be SDValue().
  SDValue VL;
  /// Original value that this NodeExtensionHelper represents.
  SDValue OrigOperand;

  /// Get the value feeding the extension or the value itself.
  /// E.g., for zext(a), this would return a.
  SDValue getSource() const {
    switch (OrigOperand.getOpcode()) {
    case RISCVISD::VSEXT_VL:
    case RISCVISD::VZEXT_VL:
      return OrigOperand.getOperand(0);
    default:
      return OrigOperand;
    }
  }

  /// Check if this instance represents a splat.
  bool isSplat() const {
    return OrigOperand.getOpcode() == RISCVISD::VMV_V_X_VL;
  }

  /// Get or create a value that can feed \p Root with the given extension \p
  /// SExt. If \p SExt is None, this returns the source of this operand.
  /// \see ::getSource().
  SDValue getOrCreateExtendedOp(const SDNode *Root, SelectionDAG &DAG,
                                std::optional<bool> SExt) const {
    if (!SExt.has_value())
      return OrigOperand;

    MVT NarrowVT = getNarrowType(Root);

    SDValue Source = getSource();
    if (Source.getValueType() == NarrowVT)
      return Source;

    unsigned ExtOpc = *SExt ? RISCVISD::VSEXT_VL : RISCVISD::VZEXT_VL;

    // If we need an extension, we should be changing the type.
    SDLoc DL(Root);
    auto [Mask, VL] = getMaskAndVL(Root);
    switch (OrigOperand.getOpcode()) {
    case RISCVISD::VSEXT_VL:
    case RISCVISD::VZEXT_VL:
      return DAG.getNode(ExtOpc, DL, NarrowVT, Source, Mask, VL);
    case RISCVISD::VMV_V_X_VL:
      return DAG.getNode(RISCVISD::VMV_V_X_VL, DL, NarrowVT,
                         DAG.getUNDEF(NarrowVT), Source.getOperand(1), VL);
    default:
      // Other opcodes can only come from the original LHS of VW(ADD|SUB)_W_VL
      // and that operand should already have the right NarrowVT so no
      // extension should be required at this point.
      llvm_unreachable("Unsupported opcode");
    }
  }

  /// Helper function to get the narrow type for \p Root.
  /// The narrow type is the type of \p Root where we divided the size of each
  /// element by 2. E.g., if Root's type <2xi16> -> narrow type <2xi8>.
  /// \pre The size of the type of the elements of Root must be a multiple of 2
  /// and be greater than 16.
  static MVT getNarrowType(const SDNode *Root) {
    MVT VT = Root->getSimpleValueType(0);

    // Determine the narrow size.
    unsigned NarrowSize = VT.getScalarSizeInBits() / 2;
    assert(NarrowSize >= 8 && "Trying to extend something we can't represent");
    MVT NarrowVT = MVT::getVectorVT(MVT::getIntegerVT(NarrowSize),
                                    VT.getVectorElementCount());
    return NarrowVT;
  }

  /// Return the opcode required to materialize the folding of the sign
  /// extensions (\p IsSExt == true) or zero extensions (IsSExt == false) for
  /// both operands for \p Opcode.
  /// Put differently, get the opcode to materialize:
  /// - ISExt == true: \p Opcode(sext(a), sext(b)) -> newOpcode(a, b)
  /// - ISExt == false: \p Opcode(zext(a), zext(b)) -> newOpcode(a, b)
  /// \pre \p Opcode represents a supported root (\see ::isSupportedRoot()).
  static unsigned getSameExtensionOpcode(unsigned Opcode, bool IsSExt) {
    switch (Opcode) {
    case RISCVISD::ADD_VL:
    case RISCVISD::VWADD_W_VL:
    case RISCVISD::VWADDU_W_VL:
      return IsSExt ? RISCVISD::VWADD_VL : RISCVISD::VWADDU_VL;
    case RISCVISD::MUL_VL:
      return IsSExt ? RISCVISD::VWMUL_VL : RISCVISD::VWMULU_VL;
    case RISCVISD::SUB_VL:
    case RISCVISD::VWSUB_W_VL:
    case RISCVISD::VWSUBU_W_VL:
      return IsSExt ? RISCVISD::VWSUB_VL : RISCVISD::VWSUBU_VL;
    default:
      llvm_unreachable("Unexpected opcode");
    }
  }

  /// Get the opcode to materialize \p Opcode(sext(a), zext(b)) ->
  /// newOpcode(a, b).
  static unsigned getSUOpcode(unsigned Opcode) {
    assert(Opcode == RISCVISD::MUL_VL && "SU is only supported for MUL");
    return RISCVISD::VWMULSU_VL;
  }

  /// Get the opcode to materialize \p Opcode(a, s|zext(b)) ->
  /// newOpcode(a, b).
  static unsigned getWOpcode(unsigned Opcode, bool IsSExt) {
    switch (Opcode) {
    case RISCVISD::ADD_VL:
      return IsSExt ? RISCVISD::VWADD_W_VL : RISCVISD::VWADDU_W_VL;
    case RISCVISD::SUB_VL:
      return IsSExt ? RISCVISD::VWSUB_W_VL : RISCVISD::VWSUBU_W_VL;
    default:
      llvm_unreachable("Unexpected opcode");
    }
  }

  using CombineToTry = std::function<std::optional<CombineResult>(
      SDNode * /*Root*/, const NodeExtensionHelper & /*LHS*/,
      const NodeExtensionHelper & /*RHS*/)>;

  /// Check if this node needs to be fully folded or extended for all users.
  bool needToPromoteOtherUsers() const { return EnforceOneUse; }

  /// Helper method to set the various fields of this struct based on the
  /// type of \p Root.
  void fillUpExtensionSupport(SDNode *Root, SelectionDAG &DAG) {
    SupportsZExt = false;
    SupportsSExt = false;
    EnforceOneUse = true;
    CheckMask = true;
    switch (OrigOperand.getOpcode()) {
    case RISCVISD::VZEXT_VL:
      SupportsZExt = true;
      Mask = OrigOperand.getOperand(1);
      VL = OrigOperand.getOperand(2);
      break;
    case RISCVISD::VSEXT_VL:
      SupportsSExt = true;
      Mask = OrigOperand.getOperand(1);
      VL = OrigOperand.getOperand(2);
      break;
    case RISCVISD::VMV_V_X_VL: {
      // Historically, we didn't care about splat values not disappearing during
      // combines.
      EnforceOneUse = false;
      CheckMask = false;
      VL = OrigOperand.getOperand(2);

      // The operand is a splat of a scalar.

      // The pasthru must be undef for tail agnostic.
      if (!OrigOperand.getOperand(0).isUndef())
        break;

      // Get the scalar value.
      SDValue Op = OrigOperand.getOperand(1);

      // See if we have enough sign bits or zero bits in the scalar to use a
      // widening opcode by splatting to smaller element size.
      MVT VT = Root->getSimpleValueType(0);
      unsigned EltBits = VT.getScalarSizeInBits();
      unsigned ScalarBits = Op.getValueSizeInBits();
      // Make sure we're getting all element bits from the scalar register.
      // FIXME: Support implicit sign extension of vmv.v.x?
      if (ScalarBits < EltBits)
        break;

      unsigned NarrowSize = VT.getScalarSizeInBits() / 2;
      // If the narrow type cannot be expressed with a legal VMV,
      // this is not a valid candidate.
      if (NarrowSize < 8)
        break;

      if (DAG.ComputeMaxSignificantBits(Op) <= NarrowSize)
        SupportsSExt = true;
      if (DAG.MaskedValueIsZero(Op,
                                APInt::getBitsSetFrom(ScalarBits, NarrowSize)))
        SupportsZExt = true;
      break;
    }
    default:
      break;
    }
  }

  /// Check if \p Root supports any extension folding combines.
  static bool isSupportedRoot(const SDNode *Root) {
    switch (Root->getOpcode()) {
    case RISCVISD::ADD_VL:
    case RISCVISD::MUL_VL:
    case RISCVISD::VWADD_W_VL:
    case RISCVISD::VWADDU_W_VL:
    case RISCVISD::SUB_VL:
    case RISCVISD::VWSUB_W_VL:
    case RISCVISD::VWSUBU_W_VL:
      return true;
    default:
      return false;
    }
  }

  /// Build a NodeExtensionHelper for \p Root.getOperand(\p OperandIdx).
  NodeExtensionHelper(SDNode *Root, unsigned OperandIdx, SelectionDAG &DAG) {
    assert(isSupportedRoot(Root) && "Trying to build an helper with an "
                                    "unsupported root");
    assert(OperandIdx < 2 && "Requesting something else than LHS or RHS");
    OrigOperand = Root->getOperand(OperandIdx);

    unsigned Opc = Root->getOpcode();
    switch (Opc) {
    // We consider VW<ADD|SUB>(U)_W(LHS, RHS) as if they were
    // <ADD|SUB>(LHS, S|ZEXT(RHS))
    case RISCVISD::VWADD_W_VL:
    case RISCVISD::VWADDU_W_VL:
    case RISCVISD::VWSUB_W_VL:
    case RISCVISD::VWSUBU_W_VL:
      if (OperandIdx == 1) {
        SupportsZExt =
            Opc == RISCVISD::VWADDU_W_VL || Opc == RISCVISD::VWSUBU_W_VL;
        SupportsSExt = !SupportsZExt;
        std::tie(Mask, VL) = getMaskAndVL(Root);
        CheckMask = true;
        // There's no existing extension here, so we don't have to worry about
        // making sure it gets removed.
        EnforceOneUse = false;
        break;
      }
      [[fallthrough]];
    default:
      fillUpExtensionSupport(Root, DAG);
      break;
    }
  }

  /// Check if this operand is compatible with the given vector length \p VL.
  bool isVLCompatible(SDValue VL) const {
    return this->VL != SDValue() && this->VL == VL;
  }

  /// Check if this operand is compatible with the given \p Mask.
  bool isMaskCompatible(SDValue Mask) const {
    return !CheckMask || (this->Mask != SDValue() && this->Mask == Mask);
  }

  /// Helper function to get the Mask and VL from \p Root.
  static std::pair<SDValue, SDValue> getMaskAndVL(const SDNode *Root) {
    assert(isSupportedRoot(Root) && "Unexpected root");
    return std::make_pair(Root->getOperand(3), Root->getOperand(4));
  }

  /// Check if the Mask and VL of this operand are compatible with \p Root.
  bool areVLAndMaskCompatible(const SDNode *Root) const {
    auto [Mask, VL] = getMaskAndVL(Root);
    return isMaskCompatible(Mask) && isVLCompatible(VL);
  }

  /// Helper function to check if \p N is commutative with respect to the
  /// foldings that are supported by this class.
  static bool isCommutative(const SDNode *N) {
    switch (N->getOpcode()) {
    case RISCVISD::ADD_VL:
    case RISCVISD::MUL_VL:
    case RISCVISD::VWADD_W_VL:
    case RISCVISD::VWADDU_W_VL:
      return true;
    case RISCVISD::SUB_VL:
    case RISCVISD::VWSUB_W_VL:
    case RISCVISD::VWSUBU_W_VL:
      return false;
    default:
      llvm_unreachable("Unexpected opcode");
    }
  }

  /// Get a list of combine to try for folding extensions in \p Root.
  /// Note that each returned CombineToTry function doesn't actually modify
  /// anything. Instead they produce an optional CombineResult that if not None,
  /// need to be materialized for the combine to be applied.
  /// \see CombineResult::materialize.
  /// If the related CombineToTry function returns std::nullopt, that means the
  /// combine didn't match.
  static SmallVector<CombineToTry> getSupportedFoldings(const SDNode *Root);
};

/// Helper structure that holds all the necessary information to materialize a
/// combine that does some extension folding.
struct CombineResult {
  /// Opcode to be generated when materializing the combine.
  unsigned TargetOpcode;
  // No value means no extension is needed. If extension is needed, the value
  // indicates if it needs to be sign extended.
  std::optional<bool> SExtLHS;
  std::optional<bool> SExtRHS;
  /// Root of the combine.
  SDNode *Root;
  /// LHS of the TargetOpcode.
  NodeExtensionHelper LHS;
  /// RHS of the TargetOpcode.
  NodeExtensionHelper RHS;

  CombineResult(unsigned TargetOpcode, SDNode *Root,
                const NodeExtensionHelper &LHS, std::optional<bool> SExtLHS,
                const NodeExtensionHelper &RHS, std::optional<bool> SExtRHS)
      : TargetOpcode(TargetOpcode), SExtLHS(SExtLHS), SExtRHS(SExtRHS),
        Root(Root), LHS(LHS), RHS(RHS) {}

  /// Return a value that uses TargetOpcode and that can be used to replace
  /// Root.
  /// The actual replacement is *not* done in that method.
  SDValue materialize(SelectionDAG &DAG) const {
    SDValue Mask, VL, Merge;
    std::tie(Mask, VL) = NodeExtensionHelper::getMaskAndVL(Root);
    Merge = Root->getOperand(2);
    return DAG.getNode(TargetOpcode, SDLoc(Root), Root->getValueType(0),
                       LHS.getOrCreateExtendedOp(Root, DAG, SExtLHS),
                       RHS.getOrCreateExtendedOp(Root, DAG, SExtRHS), Merge,
                       Mask, VL);
  }
};

/// Check if \p Root follows a pattern Root(ext(LHS), ext(RHS))
/// where `ext` is the same for both LHS and RHS (i.e., both are sext or both
/// are zext) and LHS and RHS can be folded into Root.
/// AllowSExt and AllozZExt define which form `ext` can take in this pattern.
///
/// \note If the pattern can match with both zext and sext, the returned
/// CombineResult will feature the zext result.
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVWWithSameExtensionImpl(SDNode *Root, const NodeExtensionHelper &LHS,
                                 const NodeExtensionHelper &RHS, bool AllowSExt,
                                 bool AllowZExt) {
  assert((AllowSExt || AllowZExt) && "Forgot to set what you want?");
  if (!LHS.areVLAndMaskCompatible(Root) || !RHS.areVLAndMaskCompatible(Root))
    return std::nullopt;
  if (AllowZExt && LHS.SupportsZExt && RHS.SupportsZExt)
    return CombineResult(NodeExtensionHelper::getSameExtensionOpcode(
                             Root->getOpcode(), /*IsSExt=*/false),
                         Root, LHS, /*SExtLHS=*/false, RHS,
                         /*SExtRHS=*/false);
  if (AllowSExt && LHS.SupportsSExt && RHS.SupportsSExt)
    return CombineResult(NodeExtensionHelper::getSameExtensionOpcode(
                             Root->getOpcode(), /*IsSExt=*/true),
                         Root, LHS, /*SExtLHS=*/true, RHS,
                         /*SExtRHS=*/true);
  return std::nullopt;
}

/// Check if \p Root follows a pattern Root(ext(LHS), ext(RHS))
/// where `ext` is the same for both LHS and RHS (i.e., both are sext or both
/// are zext) and LHS and RHS can be folded into Root.
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVWWithSameExtension(SDNode *Root, const NodeExtensionHelper &LHS,
                             const NodeExtensionHelper &RHS) {
  return canFoldToVWWithSameExtensionImpl(Root, LHS, RHS, /*AllowSExt=*/true,
                                          /*AllowZExt=*/true);
}

/// Check if \p Root follows a pattern Root(LHS, ext(RHS))
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVW_W(SDNode *Root, const NodeExtensionHelper &LHS,
              const NodeExtensionHelper &RHS) {
  if (!RHS.areVLAndMaskCompatible(Root))
    return std::nullopt;

  // FIXME: Is it useful to form a vwadd.wx or vwsub.wx if it removes a scalar
  // sext/zext?
  // Control this behavior behind an option (AllowSplatInVW_W) for testing
  // purposes.
  if (RHS.SupportsZExt && (!RHS.isSplat() || AllowSplatInVW_W))
    return CombineResult(
        NodeExtensionHelper::getWOpcode(Root->getOpcode(), /*IsSExt=*/false),
        Root, LHS, /*SExtLHS=*/std::nullopt, RHS, /*SExtRHS=*/false);
  if (RHS.SupportsSExt && (!RHS.isSplat() || AllowSplatInVW_W))
    return CombineResult(
        NodeExtensionHelper::getWOpcode(Root->getOpcode(), /*IsSExt=*/true),
        Root, LHS, /*SExtLHS=*/std::nullopt, RHS, /*SExtRHS=*/true);
  return std::nullopt;
}

/// Check if \p Root follows a pattern Root(sext(LHS), sext(RHS))
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVWWithSEXT(SDNode *Root, const NodeExtensionHelper &LHS,
                    const NodeExtensionHelper &RHS) {
  return canFoldToVWWithSameExtensionImpl(Root, LHS, RHS, /*AllowSExt=*/true,
                                          /*AllowZExt=*/false);
}

/// Check if \p Root follows a pattern Root(zext(LHS), zext(RHS))
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVWWithZEXT(SDNode *Root, const NodeExtensionHelper &LHS,
                    const NodeExtensionHelper &RHS) {
  return canFoldToVWWithSameExtensionImpl(Root, LHS, RHS, /*AllowSExt=*/false,
                                          /*AllowZExt=*/true);
}

/// Check if \p Root follows a pattern Root(sext(LHS), zext(RHS))
///
/// \returns std::nullopt if the pattern doesn't match or a CombineResult that
/// can be used to apply the pattern.
static std::optional<CombineResult>
canFoldToVW_SU(SDNode *Root, const NodeExtensionHelper &LHS,
               const NodeExtensionHelper &RHS) {
  if (!LHS.SupportsSExt || !RHS.SupportsZExt)
    return std::nullopt;
  if (!LHS.areVLAndMaskCompatible(Root) || !RHS.areVLAndMaskCompatible(Root))
    return std::nullopt;
  return CombineResult(NodeExtensionHelper::getSUOpcode(Root->getOpcode()),
                       Root, LHS, /*SExtLHS=*/true, RHS, /*SExtRHS=*/false);
}

SmallVector<NodeExtensionHelper::CombineToTry>
NodeExtensionHelper::getSupportedFoldings(const SDNode *Root) {
  SmallVector<CombineToTry> Strategies;
  switch (Root->getOpcode()) {
  case RISCVISD::ADD_VL:
  case RISCVISD::SUB_VL:
    // add|sub -> vwadd(u)|vwsub(u)
    Strategies.push_back(canFoldToVWWithSameExtension);
    // add|sub -> vwadd(u)_w|vwsub(u)_w
    Strategies.push_back(canFoldToVW_W);
    break;
  case RISCVISD::MUL_VL:
    // mul -> vwmul(u)
    Strategies.push_back(canFoldToVWWithSameExtension);
    // mul -> vwmulsu
    Strategies.push_back(canFoldToVW_SU);
    break;
  case RISCVISD::VWADD_W_VL:
  case RISCVISD::VWSUB_W_VL:
    // vwadd_w|vwsub_w -> vwadd|vwsub
    Strategies.push_back(canFoldToVWWithSEXT);
    break;
  case RISCVISD::VWADDU_W_VL:
  case RISCVISD::VWSUBU_W_VL:
    // vwaddu_w|vwsubu_w -> vwaddu|vwsubu
    Strategies.push_back(canFoldToVWWithZEXT);
    break;
  default:
    llvm_unreachable("Unexpected opcode");
  }
  return Strategies;
}
} // End anonymous namespace.

/// Combine a binary operation to its equivalent VW or VW_W form.
/// The supported combines are:
/// add_vl -> vwadd(u) | vwadd(u)_w
/// sub_vl -> vwsub(u) | vwsub(u)_w
/// mul_vl -> vwmul(u) | vwmul_su
/// vwadd_w(u) -> vwadd(u)
/// vwub_w(u) -> vwadd(u)
static SDValue
combineBinOp_VLToVWBinOp_VL(SDNode *N, TargetLowering::DAGCombinerInfo &DCI) {
  SelectionDAG &DAG = DCI.DAG;

  assert(NodeExtensionHelper::isSupportedRoot(N) &&
         "Shouldn't have called this method");
  SmallVector<SDNode *> Worklist;
  SmallSet<SDNode *, 8> Inserted;
  Worklist.push_back(N);
  Inserted.insert(N);
  SmallVector<CombineResult> CombinesToApply;

  while (!Worklist.empty()) {
    SDNode *Root = Worklist.pop_back_val();
    if (!NodeExtensionHelper::isSupportedRoot(Root))
      return SDValue();

    NodeExtensionHelper LHS(N, 0, DAG);
    NodeExtensionHelper RHS(N, 1, DAG);
    auto AppendUsersIfNeeded = [&Worklist,
                                &Inserted](const NodeExtensionHelper &Op) {
      if (Op.needToPromoteOtherUsers()) {
        for (SDNode *TheUse : Op.OrigOperand->uses()) {
          if (Inserted.insert(TheUse).second)
            Worklist.push_back(TheUse);
        }
      }
    };
    AppendUsersIfNeeded(LHS);
    AppendUsersIfNeeded(RHS);

    // Control the compile time by limiting the number of node we look at in
    // total.
    if (Inserted.size() > ExtensionMaxWebSize)
      return SDValue();

    SmallVector<NodeExtensionHelper::CombineToTry> FoldingStrategies =
        NodeExtensionHelper::getSupportedFoldings(N);

    assert(!FoldingStrategies.empty() && "Nothing to be folded");
    bool Matched = false;
    for (int Attempt = 0;
         (Attempt != 1 + NodeExtensionHelper::isCommutative(N)) && !Matched;
         ++Attempt) {

      for (NodeExtensionHelper::CombineToTry FoldingStrategy :
           FoldingStrategies) {
        std::optional<CombineResult> Res = FoldingStrategy(N, LHS, RHS);
        if (Res) {
          Matched = true;
          CombinesToApply.push_back(*Res);
          break;
        }
      }
      std::swap(LHS, RHS);
    }
    // Right now we do an all or nothing approach.
    if (!Matched)
      return SDValue();
  }
  // Store the value for the replacement of the input node separately.
  SDValue InputRootReplacement;
  // We do the RAUW after we materialize all the combines, because some replaced
  // nodes may be feeding some of the yet-to-be-replaced nodes. Put differently,
  // some of these nodes may appear in the NodeExtensionHelpers of some of the
  // yet-to-be-visited CombinesToApply roots.
  SmallVector<std::pair<SDValue, SDValue>> ValuesToReplace;
  ValuesToReplace.reserve(CombinesToApply.size());
  for (CombineResult Res : CombinesToApply) {
    SDValue NewValue = Res.materialize(DAG);
    if (!InputRootReplacement) {
      assert(Res.Root == N &&
             "First element is expected to be the current node");
      InputRootReplacement = NewValue;
    } else {
      ValuesToReplace.emplace_back(SDValue(Res.Root, 0), NewValue);
    }
  }
  for (std::pair<SDValue, SDValue> OldNewValues : ValuesToReplace) {
    DAG.ReplaceAllUsesOfValueWith(OldNewValues.first, OldNewValues.second);
    DCI.AddToWorklist(OldNewValues.second.getNode());
  }
  return InputRootReplacement;
}

// Fold
//   (fp_to_int (froundeven X)) -> fcvt X, rne
//   (fp_to_int (ftrunc X))     -> fcvt X, rtz
//   (fp_to_int (ffloor X))     -> fcvt X, rdn
//   (fp_to_int (fceil X))      -> fcvt X, rup
//   (fp_to_int (fround X))     -> fcvt X, rmm
static SDValue performFP_TO_INTCombine(SDNode *N,
                                       TargetLowering::DAGCombinerInfo &DCI,
                                       const RISCVSubtarget &Subtarget) {
  SelectionDAG &DAG = DCI.DAG;
  const TargetLowering &TLI = DAG.getTargetLoweringInfo();
  MVT XLenVT = Subtarget.getXLenVT();

  // Only handle XLen or i32 types. Other types narrower than XLen will
  // eventually be legalized to XLenVT.
  EVT VT = N->getValueType(0);
  if (VT != MVT::i32 && VT != XLenVT)
    return SDValue();

  SDValue Src = N->getOperand(0);

  // Ensure the FP type is also legal.
  if (!TLI.isTypeLegal(Src.getValueType()))
    return SDValue();

  // Don't do this for f16 with Zfhmin and not Zfh.
  if (Src.getValueType() == MVT::f16 && !Subtarget.hasStdExtZfh())
    return SDValue();

  RISCVFPRndMode::RoundingMode FRM = matchRoundingOp(Src.getOpcode());
  if (FRM == RISCVFPRndMode::Invalid)
    return SDValue();

  bool IsSigned = N->getOpcode() == ISD::FP_TO_SINT;

  unsigned Opc;
  if (VT == XLenVT)
    Opc = IsSigned ? RISCVISD::FCVT_X : RISCVISD::FCVT_XU;
  else
    Opc = IsSigned ? RISCVISD::FCVT_W_RV64 : RISCVISD::FCVT_WU_RV64;

  SDLoc DL(N);
  SDValue FpToInt = DAG.getNode(Opc, DL, XLenVT, Src.getOperand(0),
                                DAG.getTargetConstant(FRM, DL, XLenVT));
  return DAG.getNode(ISD::TRUNCATE, DL, VT, FpToInt);
}

// Fold
//   (fp_to_int_sat (froundeven X)) -> (select X == nan, 0, (fcvt X, rne))
//   (fp_to_int_sat (ftrunc X))     -> (select X == nan, 0, (fcvt X, rtz))
//   (fp_to_int_sat (ffloor X))     -> (select X == nan, 0, (fcvt X, rdn))
//   (fp_to_int_sat (fceil X))      -> (select X == nan, 0, (fcvt X, rup))
//   (fp_to_int_sat (fround X))     -> (select X == nan, 0, (fcvt X, rmm))
static SDValue performFP_TO_INT_SATCombine(SDNode *N,
                                       TargetLowering::DAGCombinerInfo &DCI,
                                       const RISCVSubtarget &Subtarget) {
  SelectionDAG &DAG = DCI.DAG;
  const TargetLowering &TLI = DAG.getTargetLoweringInfo();
  MVT XLenVT = Subtarget.getXLenVT();

  // Only handle XLen types. Other types narrower than XLen will eventually be
  // legalized to XLenVT.
  EVT DstVT = N->getValueType(0);
  if (DstVT != XLenVT)
    return SDValue();

  SDValue Src = N->getOperand(0);

  // Ensure the FP type is also legal.
  if (!TLI.isTypeLegal(Src.getValueType()))
    return SDValue();

  // Don't do this for f16 with Zfhmin and not Zfh.
  if (Src.getValueType() == MVT::f16 && !Subtarget.hasStdExtZfh())
    return SDValue();

  EVT SatVT = cast<VTSDNode>(N->getOperand(1))->getVT();

  RISCVFPRndMode::RoundingMode FRM = matchRoundingOp(Src.getOpcode());
  if (FRM == RISCVFPRndMode::Invalid)
    return SDValue();

  bool IsSigned = N->getOpcode() == ISD::FP_TO_SINT_SAT;

  unsigned Opc;
  if (SatVT == DstVT)
    Opc = IsSigned ? RISCVISD::FCVT_X : RISCVISD::FCVT_XU;
  else if (DstVT == MVT::i64 && SatVT == MVT::i32)
    Opc = IsSigned ? RISCVISD::FCVT_W_RV64 : RISCVISD::FCVT_WU_RV64;
  else
    return SDValue();
  // FIXME: Support other SatVTs by clamping before or after the conversion.

  Src = Src.getOperand(0);

  SDLoc DL(N);
  SDValue FpToInt = DAG.getNode(Opc, DL, XLenVT, Src,
                                DAG.getTargetConstant(FRM, DL, XLenVT));

  // fcvt.wu.* sign extends bit 31 on RV64. FP_TO_UINT_SAT expects to zero
  // extend.
  if (Opc == RISCVISD::FCVT_WU_RV64)
    FpToInt = DAG.getZeroExtendInReg(FpToInt, DL, MVT::i32);

  // RISCV FP-to-int conversions saturate to the destination register size, but
  // don't produce 0 for nan.
  SDValue ZeroInt = DAG.getConstant(0, DL, DstVT);
  return DAG.getSelectCC(DL, Src, Src, ZeroInt, FpToInt, ISD::CondCode::SETUO);
}

// Combine (bitreverse (bswap X)) to the BREV8 GREVI encoding if the type is
// smaller than XLenVT.
static SDValue performBITREVERSECombine(SDNode *N, SelectionDAG &DAG,
                                        const RISCVSubtarget &Subtarget) {
  assert(Subtarget.hasStdExtZbkb() && "Unexpected extension");

  SDValue Src = N->getOperand(0);
  if (Src.getOpcode() != ISD::BSWAP)
    return SDValue();

  EVT VT = N->getValueType(0);
  if (!VT.isScalarInteger() || VT.getSizeInBits() >= Subtarget.getXLen() ||
      !isPowerOf2_32(VT.getSizeInBits()))
    return SDValue();

  SDLoc DL(N);
  return DAG.getNode(RISCVISD::BREV8, DL, VT, Src.getOperand(0));
}

// Convert from one FMA opcode to another based on whether we are negating the
// multiply result and/or the accumulator.
// NOTE: Only supports RVV operations with VL.
static unsigned negateFMAOpcode(unsigned Opcode, bool NegMul, bool NegAcc) {
  assert((NegMul || NegAcc) && "Not negating anything?");

  // Negating the multiply result changes ADD<->SUB and toggles 'N'.
  if (NegMul) {
    // clang-format off
    switch (Opcode) {
    default: llvm_unreachable("Unexpected opcode");
    case RISCVISD::VFMADD_VL:  Opcode = RISCVISD::VFNMSUB_VL; break;
    case RISCVISD::VFNMSUB_VL: Opcode = RISCVISD::VFMADD_VL;  break;
    case RISCVISD::VFNMADD_VL: Opcode = RISCVISD::VFMSUB_VL;  break;
    case RISCVISD::VFMSUB_VL:  Opcode = RISCVISD::VFNMADD_VL; break;
    }
    // clang-format on
  }

  // Negating the accumulator changes ADD<->SUB.
  if (NegAcc) {
    // clang-format off
    switch (Opcode) {
    default: llvm_unreachable("Unexpected opcode");
    case RISCVISD::VFMADD_VL:  Opcode = RISCVISD::VFMSUB_VL;  break;
    case RISCVISD::VFMSUB_VL:  Opcode = RISCVISD::VFMADD_VL;  break;
    case RISCVISD::VFNMADD_VL: Opcode = RISCVISD::VFNMSUB_VL; break;
    case RISCVISD::VFNMSUB_VL: Opcode = RISCVISD::VFNMADD_VL; break;
    }
    // clang-format on
  }

  return Opcode;
}

static SDValue performSRACombine(SDNode *N, SelectionDAG &DAG,
                                 const RISCVSubtarget &Subtarget) {
  assert(N->getOpcode() == ISD::SRA && "Unexpected opcode");

  if (N->getValueType(0) != MVT::i64 || !Subtarget.is64Bit())
    return SDValue();

  if (!isa<ConstantSDNode>(N->getOperand(1)))
    return SDValue();
  uint64_t ShAmt = N->getConstantOperandVal(1);
  if (ShAmt > 32)
    return SDValue();

  SDValue N0 = N->getOperand(0);

  // Combine (sra (sext_inreg (shl X, C1), i32), C2) ->
  // (sra (shl X, C1+32), C2+32) so it gets selected as SLLI+SRAI instead of
  // SLLIW+SRAIW. SLLI+SRAI have compressed forms.
  if (ShAmt < 32 &&
      N0.getOpcode() == ISD::SIGN_EXTEND_INREG && N0.hasOneUse() &&
      cast<VTSDNode>(N0.getOperand(1))->getVT() == MVT::i32 &&
      N0.getOperand(0).getOpcode() == ISD::SHL && N0.getOperand(0).hasOneUse() &&
      isa<ConstantSDNode>(N0.getOperand(0).getOperand(1))) {
    uint64_t LShAmt = N0.getOperand(0).getConstantOperandVal(1);
    if (LShAmt < 32) {
      SDLoc ShlDL(N0.getOperand(0));
      SDValue Shl = DAG.getNode(ISD::SHL, ShlDL, MVT::i64,
                                N0.getOperand(0).getOperand(0),
                                DAG.getConstant(LShAmt + 32, ShlDL, MVT::i64));
      SDLoc DL(N);
      return DAG.getNode(ISD::SRA, DL, MVT::i64, Shl,
                         DAG.getConstant(ShAmt + 32, DL, MVT::i64));
    }
  }

  // Combine (sra (shl X, 32), 32 - C) -> (shl (sext_inreg X, i32), C)
  // FIXME: Should this be a generic combine? There's a similar combine on X86.
  //
  // Also try these folds where an add or sub is in the middle.
  // (sra (add (shl X, 32), C1), 32 - C) -> (shl (sext_inreg (add X, C1), C)
  // (sra (sub C1, (shl X, 32)), 32 - C) -> (shl (sext_inreg (sub C1, X), C)
  SDValue Shl;
  ConstantSDNode *AddC = nullptr;

  // We might have an ADD or SUB between the SRA and SHL.
  bool IsAdd = N0.getOpcode() == ISD::ADD;
  if ((IsAdd || N0.getOpcode() == ISD::SUB)) {
    // Other operand needs to be a constant we can modify.
    AddC = dyn_cast<ConstantSDNode>(N0.getOperand(IsAdd ? 1 : 0));
    if (!AddC)
      return SDValue();

    // AddC needs to have at least 32 trailing zeros.
    if (AddC->getAPIntValue().countTrailingZeros() < 32)
      return SDValue();

    // All users should be a shift by constant less than or equal to 32. This
    // ensures we'll do this optimization for each of them to produce an
    // add/sub+sext_inreg they can all share.
    for (SDNode *U : N0->uses()) {
      if (U->getOpcode() != ISD::SRA ||
          !isa<ConstantSDNode>(U->getOperand(1)) ||
          cast<ConstantSDNode>(U->getOperand(1))->getZExtValue() > 32)
        return SDValue();
    }

    Shl = N0.getOperand(IsAdd ? 0 : 1);
  } else {
    // Not an ADD or SUB.
    Shl = N0;
  }

  // Look for a shift left by 32.
  if (Shl.getOpcode() != ISD::SHL || !isa<ConstantSDNode>(Shl.getOperand(1)) ||
      Shl.getConstantOperandVal(1) != 32)
    return SDValue();

  // We if we didn't look through an add/sub, then the shl should have one use.
  // If we did look through an add/sub, the sext_inreg we create is free so
  // we're only creating 2 new instructions. It's enough to only remove the
  // original sra+add/sub.
  if (!AddC && !Shl.hasOneUse())
    return SDValue();

  SDLoc DL(N);
  SDValue In = Shl.getOperand(0);

  // If we looked through an ADD or SUB, we need to rebuild it with the shifted
  // constant.
  if (AddC) {
    SDValue ShiftedAddC =
        DAG.getConstant(AddC->getAPIntValue().lshr(32), DL, MVT::i64);
    if (IsAdd)
      In = DAG.getNode(ISD::ADD, DL, MVT::i64, In, ShiftedAddC);
    else
      In = DAG.getNode(ISD::SUB, DL, MVT::i64, ShiftedAddC, In);
  }

  SDValue SExt = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, In,
                             DAG.getValueType(MVT::i32));
  if (ShAmt == 32)
    return SExt;

  return DAG.getNode(
      ISD::SHL, DL, MVT::i64, SExt,
      DAG.getConstant(32 - ShAmt, DL, MVT::i64));
}

// Invert (and/or (set cc X, Y), (xor Z, 1)) to (or/and (set !cc X, Y)), Z) if
// the result is used as the conditon of a br_cc or select_cc we can invert,
// inverting the setcc is free, and Z is 0/1. Caller will invert the
// br_cc/select_cc.
static SDValue tryDemorganOfBooleanCondition(SDValue Cond, SelectionDAG &DAG) {
  bool IsAnd = Cond.getOpcode() == ISD::AND;
  if (!IsAnd && Cond.getOpcode() != ISD::OR)
    return SDValue();

  if (!Cond.hasOneUse())
    return SDValue();

  SDValue Setcc = Cond.getOperand(0);
  SDValue Xor = Cond.getOperand(1);
  // Canonicalize setcc to LHS.
  if (Setcc.getOpcode() != ISD::SETCC)
    std::swap(Setcc, Xor);
  // LHS should be a setcc and RHS should be an xor.
  if (Setcc.getOpcode() != ISD::SETCC || !Setcc.hasOneUse() ||
      Xor.getOpcode() != ISD::XOR || !Xor.hasOneUse())
    return SDValue();

  // If the condition is an And, SimplifyDemandedBits may have changed
  // (xor Z, 1) to (not Z).
  SDValue Xor1 = Xor.getOperand(1);
  if (!isOneConstant(Xor1) && !(IsAnd && isAllOnesConstant(Xor1)))
    return SDValue();

  EVT VT = Cond.getValueType();
  SDValue Xor0 = Xor.getOperand(0);

  // The LHS of the xor needs to be 0/1.
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), 1);
  if (!DAG.MaskedValueIsZero(Xor0, Mask))
    return SDValue();

  // We can only invert integer setccs.
  EVT SetCCOpVT = Setcc.getOperand(0).getValueType();
  if (!SetCCOpVT.isScalarInteger())
    return SDValue();

  ISD::CondCode CCVal = cast<CondCodeSDNode>(Setcc.getOperand(2))->get();
  if (ISD::isIntEqualitySetCC(CCVal)) {
    CCVal = ISD::getSetCCInverse(CCVal, SetCCOpVT);
    Setcc = DAG.getSetCC(SDLoc(Setcc), VT, Setcc.getOperand(0),
                         Setcc.getOperand(1), CCVal);
  } else if (CCVal == ISD::SETLT && isNullConstant(Setcc.getOperand(0))) {
    // Invert (setlt 0, X) by converting to (setlt X, 1).
    Setcc = DAG.getSetCC(SDLoc(Setcc), VT, Setcc.getOperand(1),
                         DAG.getConstant(1, SDLoc(Setcc), VT), CCVal);
  } else if (CCVal == ISD::SETLT && isOneConstant(Setcc.getOperand(1))) {
    // (setlt X, 1) by converting to (setlt 0, X).
    Setcc = DAG.getSetCC(SDLoc(Setcc), VT,
                         DAG.getConstant(0, SDLoc(Setcc), VT),
                         Setcc.getOperand(0), CCVal);
  } else
    return SDValue();

  unsigned Opc = IsAnd ? ISD::OR : ISD::AND;
  return DAG.getNode(Opc, SDLoc(Cond), VT, Setcc, Xor.getOperand(0));
}

// Perform common combines for BR_CC and SELECT_CC condtions.
static bool combine_CC(SDValue &LHS, SDValue &RHS, SDValue &CC, const SDLoc &DL,
                       SelectionDAG &DAG, const RISCVSubtarget &Subtarget) {
  ISD::CondCode CCVal = cast<CondCodeSDNode>(CC)->get();
  if (!ISD::isIntEqualitySetCC(CCVal))
    return false;

  // Fold ((setlt X, Y), 0, ne) -> (X, Y, lt)
  // Sometimes the setcc is introduced after br_cc/select_cc has been formed.
  if (LHS.getOpcode() == ISD::SETCC && isNullConstant(RHS) &&
      LHS.getOperand(0).getValueType() == Subtarget.getXLenVT()) {
    // If we're looking for eq 0 instead of ne 0, we need to invert the
    // condition.
    bool Invert = CCVal == ISD::SETEQ;
    CCVal = cast<CondCodeSDNode>(LHS.getOperand(2))->get();
    if (Invert)
      CCVal = ISD::getSetCCInverse(CCVal, LHS.getValueType());

    RHS = LHS.getOperand(1);
    LHS = LHS.getOperand(0);
    translateSetCCForBranch(DL, LHS, RHS, CCVal, DAG);

    CC = DAG.getCondCode(CCVal);
    return true;
  }

  // Fold ((xor X, Y), 0, eq/ne) -> (X, Y, eq/ne)
  if (LHS.getOpcode() == ISD::XOR && isNullConstant(RHS)) {
    RHS = LHS.getOperand(1);
    LHS = LHS.getOperand(0);
    return true;
  }

  // Fold ((srl (and X, 1<<C), C), 0, eq/ne) -> ((shl X, XLen-1-C), 0, ge/lt)
  if (isNullConstant(RHS) && LHS.getOpcode() == ISD::SRL && LHS.hasOneUse() &&
      LHS.getOperand(1).getOpcode() == ISD::Constant) {
    SDValue LHS0 = LHS.getOperand(0);
    if (LHS0.getOpcode() == ISD::AND &&
        LHS0.getOperand(1).getOpcode() == ISD::Constant) {
      uint64_t Mask = LHS0.getConstantOperandVal(1);
      uint64_t ShAmt = LHS.getConstantOperandVal(1);
      if (isPowerOf2_64(Mask) && Log2_64(Mask) == ShAmt) {
        CCVal = CCVal == ISD::SETEQ ? ISD::SETGE : ISD::SETLT;
        CC = DAG.getCondCode(CCVal);

        ShAmt = LHS.getValueSizeInBits() - 1 - ShAmt;
        LHS = LHS0.getOperand(0);
        if (ShAmt != 0)
          LHS =
              DAG.getNode(ISD::SHL, DL, LHS.getValueType(), LHS0.getOperand(0),
                          DAG.getConstant(ShAmt, DL, LHS.getValueType()));
        return true;
      }
    }
  }

  // (X, 1, setne) -> // (X, 0, seteq) if we can prove X is 0/1.
  // This can occur when legalizing some floating point comparisons.
  APInt Mask = APInt::getBitsSetFrom(LHS.getValueSizeInBits(), 1);
  if (isOneConstant(RHS) && DAG.MaskedValueIsZero(LHS, Mask)) {
    CCVal = ISD::getSetCCInverse(CCVal, LHS.getValueType());
    CC = DAG.getCondCode(CCVal);
    RHS = DAG.getConstant(0, DL, LHS.getValueType());
    return true;
  }

  if (isNullConstant(RHS)) {
    if (SDValue NewCond = tryDemorganOfBooleanCondition(LHS, DAG)) {
      CCVal = ISD::getSetCCInverse(CCVal, LHS.getValueType());
      CC = DAG.getCondCode(CCVal);
      LHS = NewCond;
      return true;
    }
  }

  return false;
}

SDValue RISCVTargetLowering::PerformDAGCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;

  // Helper to call SimplifyDemandedBits on an operand of N where only some low
  // bits are demanded. N will be added to the Worklist if it was not deleted.
  // Caller should return SDValue(N, 0) if this returns true.
  auto SimplifyDemandedLowBitsHelper = [&](unsigned OpNo, unsigned LowBits) {
    SDValue Op = N->getOperand(OpNo);
    APInt Mask = APInt::getLowBitsSet(Op.getValueSizeInBits(), LowBits);
    if (!SimplifyDemandedBits(Op, Mask, DCI))
      return false;

    if (N->getOpcode() != ISD::DELETED_NODE)
      DCI.AddToWorklist(N);
    return true;
  };

  switch (N->getOpcode()) {
  default:
    break;
  case RISCVISD::SplitF64: {
    SDValue Op0 = N->getOperand(0);
    // If the input to SplitF64 is just BuildPairF64 then the operation is
    // redundant. Instead, use BuildPairF64's operands directly.
    if (Op0->getOpcode() == RISCVISD::BuildPairF64)
      return DCI.CombineTo(N, Op0.getOperand(0), Op0.getOperand(1));

    if (Op0->isUndef()) {
      SDValue Lo = DAG.getUNDEF(MVT::i32);
      SDValue Hi = DAG.getUNDEF(MVT::i32);
      return DCI.CombineTo(N, Lo, Hi);
    }

    SDLoc DL(N);

    // It's cheaper to materialise two 32-bit integers than to load a double
    // from the constant pool and transfer it to integer registers through the
    // stack.
    if (ConstantFPSDNode *C = dyn_cast<ConstantFPSDNode>(Op0)) {
      APInt V = C->getValueAPF().bitcastToAPInt();
      SDValue Lo = DAG.getConstant(V.trunc(32), DL, MVT::i32);
      SDValue Hi = DAG.getConstant(V.lshr(32).trunc(32), DL, MVT::i32);
      return DCI.CombineTo(N, Lo, Hi);
    }

    // This is a target-specific version of a DAGCombine performed in
    // DAGCombiner::visitBITCAST. It performs the equivalent of:
    // fold (bitconvert (fneg x)) -> (xor (bitconvert x), signbit)
    // fold (bitconvert (fabs x)) -> (and (bitconvert x), (not signbit))
    if (!(Op0.getOpcode() == ISD::FNEG || Op0.getOpcode() == ISD::FABS) ||
        !Op0.getNode()->hasOneUse())
      break;
    SDValue NewSplitF64 =
        DAG.getNode(RISCVISD::SplitF64, DL, DAG.getVTList(MVT::i32, MVT::i32),
                    Op0.getOperand(0));
    SDValue Lo = NewSplitF64.getValue(0);
    SDValue Hi = NewSplitF64.getValue(1);
    APInt SignBit = APInt::getSignMask(32);
    if (Op0.getOpcode() == ISD::FNEG) {
      SDValue NewHi = DAG.getNode(ISD::XOR, DL, MVT::i32, Hi,
                                  DAG.getConstant(SignBit, DL, MVT::i32));
      return DCI.CombineTo(N, Lo, NewHi);
    }
    assert(Op0.getOpcode() == ISD::FABS);
    SDValue NewHi = DAG.getNode(ISD::AND, DL, MVT::i32, Hi,
                                DAG.getConstant(~SignBit, DL, MVT::i32));
    return DCI.CombineTo(N, Lo, NewHi);
  }
  case RISCVISD::SLLW:
  case RISCVISD::SRAW:
  case RISCVISD::SRLW:
  case RISCVISD::RORW:
  case RISCVISD::ROLW: {
    // Only the lower 32 bits of LHS and lower 5 bits of RHS are read.
    if (SimplifyDemandedLowBitsHelper(0, 32) ||
        SimplifyDemandedLowBitsHelper(1, 5))
      return SDValue(N, 0);

    break;
  }
  case RISCVISD::CLZW:
  case RISCVISD::CTZW: {
    // Only the lower 32 bits of the first operand are read
    if (SimplifyDemandedLowBitsHelper(0, 32))
      return SDValue(N, 0);
    break;
  }
  case RISCVISD::FMV_X_ANYEXTH:
  case RISCVISD::FMV_X_ANYEXTW_RV64: {
    SDLoc DL(N);
    SDValue Op0 = N->getOperand(0);
    MVT VT = N->getSimpleValueType(0);
    // If the input to FMV_X_ANYEXTW_RV64 is just FMV_W_X_RV64 then the
    // conversion is unnecessary and can be replaced with the FMV_W_X_RV64
    // operand. Similar for FMV_X_ANYEXTH and FMV_H_X.
    if ((N->getOpcode() == RISCVISD::FMV_X_ANYEXTW_RV64 &&
         Op0->getOpcode() == RISCVISD::FMV_W_X_RV64) ||
        (N->getOpcode() == RISCVISD::FMV_X_ANYEXTH &&
         Op0->getOpcode() == RISCVISD::FMV_H_X)) {
      assert(Op0.getOperand(0).getValueType() == VT &&
             "Unexpected value type!");
      return Op0.getOperand(0);
    }

    // This is a target-specific version of a DAGCombine performed in
    // DAGCombiner::visitBITCAST. It performs the equivalent of:
    // fold (bitconvert (fneg x)) -> (xor (bitconvert x), signbit)
    // fold (bitconvert (fabs x)) -> (and (bitconvert x), (not signbit))
    if (!(Op0.getOpcode() == ISD::FNEG || Op0.getOpcode() == ISD::FABS) ||
        !Op0.getNode()->hasOneUse())
      break;
    SDValue NewFMV = DAG.getNode(N->getOpcode(), DL, VT, Op0.getOperand(0));
    unsigned FPBits = N->getOpcode() == RISCVISD::FMV_X_ANYEXTW_RV64 ? 32 : 16;
    APInt SignBit = APInt::getSignMask(FPBits).sext(VT.getSizeInBits());
    if (Op0.getOpcode() == ISD::FNEG)
      return DAG.getNode(ISD::XOR, DL, VT, NewFMV,
                         DAG.getConstant(SignBit, DL, VT));

    assert(Op0.getOpcode() == ISD::FABS);
    return DAG.getNode(ISD::AND, DL, VT, NewFMV,
                       DAG.getConstant(~SignBit, DL, VT));
  }
  case ISD::ADD:
    return performADDCombine(N, DAG, Subtarget);
  case ISD::SUB:
    return performSUBCombine(N, DAG);
  case ISD::AND:
    return performANDCombine(N, DCI, Subtarget);
  case ISD::OR:
    return performORCombine(N, DCI, Subtarget);
  case ISD::XOR:
    return performXORCombine(N, DAG);
  case ISD::SETCC:
    return performSETCCCombine(N, DAG, Subtarget);
  case ISD::SIGN_EXTEND_INREG:
    return performSIGN_EXTEND_INREGCombine(N, DAG, Subtarget);
  case ISD::ZERO_EXTEND:
    // Fold (zero_extend (fp_to_uint X)) to prevent forming fcvt+zexti32 during
    // type legalization. This is safe because fp_to_uint produces poison if
    // it overflows.
    if (N->getValueType(0) == MVT::i64 && Subtarget.is64Bit()) {
      SDValue Src = N->getOperand(0);
      if (Src.getOpcode() == ISD::FP_TO_UINT &&
          isTypeLegal(Src.getOperand(0).getValueType()))
        return DAG.getNode(ISD::FP_TO_UINT, SDLoc(N), MVT::i64,
                           Src.getOperand(0));
      if (Src.getOpcode() == ISD::STRICT_FP_TO_UINT && Src.hasOneUse() &&
          isTypeLegal(Src.getOperand(1).getValueType())) {
        SDVTList VTs = DAG.getVTList(MVT::i64, MVT::Other);
        SDValue Res = DAG.getNode(ISD::STRICT_FP_TO_UINT, SDLoc(N), VTs,
                                  Src.getOperand(0), Src.getOperand(1));
        DCI.CombineTo(N, Res);
        DAG.ReplaceAllUsesOfValueWith(Src.getValue(1), Res.getValue(1));
        DCI.recursivelyDeleteUnusedNodes(Src.getNode());
        return SDValue(N, 0); // Return N so it doesn't get rechecked.
      }
    }
    return SDValue();
  case ISD::TRUNCATE:
    return performTRUNCATECombine(N, DAG, Subtarget);
  case RISCVISD::SELECT_CC: {
    // Transform
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    SDValue CC = N->getOperand(2);
    ISD::CondCode CCVal = cast<CondCodeSDNode>(CC)->get();
    SDValue TrueV = N->getOperand(3);
    SDValue FalseV = N->getOperand(4);
    SDLoc DL(N);
    EVT VT = N->getValueType(0);

    // If the True and False values are the same, we don't need a select_cc.
    if (TrueV == FalseV)
      return TrueV;

    // (select (x in [0,1] == 0), y, (z ^ y) ) -> (-x & z ) ^ y
    // (select (x in [0,1] != 0), (z ^ y), y ) -> (-x & z ) ^ y
    // (select (x in [0,1] == 0), y, (z | y) ) -> (-x & z ) | y
    // (select (x in [0,1] != 0), (z | y), y ) -> (-x & z ) | y
    // NOTE: We only do this if the target does not have the short forward
    // branch optimization.
    APInt Mask = APInt::getBitsSetFrom(LHS.getValueSizeInBits(), 1);
    if (!Subtarget.hasShortForwardBranchOpt() && isNullConstant(RHS) &&
        ISD::isIntEqualitySetCC(CCVal) && DAG.MaskedValueIsZero(LHS, Mask)) {
      unsigned Opcode;
      SDValue Src1, Src2;
      // true if FalseV is XOR or OR operator and one of its operands
      // is equal to Op1
      // ( a , a op b) || ( b , a op b)
      auto isOrXorPattern = [&]() {
        if (CCVal == ISD::SETEQ &&
            (FalseV.getOpcode() == ISD::XOR || FalseV.getOpcode() == ISD::OR) &&
            (FalseV.getOperand(0) == TrueV || FalseV.getOperand(1) == TrueV)) {
          Src1 = FalseV.getOperand(0) == TrueV ?
            FalseV.getOperand(1) : FalseV.getOperand(0);
          Src2 = TrueV;
          Opcode = FalseV.getOpcode();
          return true;
        }
        if (CCVal == ISD::SETNE &&
            (TrueV.getOpcode() == ISD::XOR || TrueV.getOpcode() == ISD::OR) &&
            (TrueV.getOperand(0) == FalseV || TrueV.getOperand(1) == FalseV)) {
          Src1 = TrueV.getOperand(0) == FalseV ?
            TrueV.getOperand(1) : TrueV.getOperand(0);
          Src2 = FalseV;
          Opcode = TrueV.getOpcode();
          return true;
        }

        return false;
      };

      if (isOrXorPattern()) {
        SDValue Neg;
        unsigned CmpSz = LHS.getSimpleValueType().getSizeInBits();
        // We need mask of all zeros or ones with same size of the other
        // operands.
        if (CmpSz > VT.getSizeInBits())
          Neg = DAG.getNode(ISD::TRUNCATE, DL, VT, LHS);
        else if (CmpSz < VT.getSizeInBits())
          Neg = DAG.getNode(ISD::AND, DL, VT,
                            DAG.getNode(ISD::ANY_EXTEND, DL, VT, LHS),
                            DAG.getConstant(1, DL, VT));
        else
          Neg = LHS;
        SDValue Mask = DAG.getNegative(Neg, DL, VT);               // -x
        SDValue And = DAG.getNode(ISD::AND, DL, VT, Mask, Src1); // Mask & z
        return DAG.getNode(Opcode, DL, VT, And, Src2);           // And Op y
      }
    }

    // (select (x < 0), y, z)  -> x >> (XLEN - 1) & (y - z) + z
    // (select (x >= 0), y, z) -> x >> (XLEN - 1) & (z - y) + y
    if (!Subtarget.hasShortForwardBranchOpt() && isa<ConstantSDNode>(TrueV) &&
        isa<ConstantSDNode>(FalseV) && isNullConstant(RHS) &&
        (CCVal == ISD::CondCode::SETLT || CCVal == ISD::CondCode::SETGE)) {
      if (CCVal == ISD::CondCode::SETGE)
        std::swap(TrueV, FalseV);

      int64_t TrueSImm = cast<ConstantSDNode>(TrueV)->getSExtValue();
      int64_t FalseSImm = cast<ConstantSDNode>(FalseV)->getSExtValue();
      // Only handle simm12, if it is not in this range, it can be considered as
      // register.
      if (isInt<12>(TrueSImm) && isInt<12>(FalseSImm) &&
          isInt<12>(TrueSImm - FalseSImm)) {
        SDValue SRA =
            DAG.getNode(ISD::SRA, DL, VT, LHS,
                        DAG.getConstant(Subtarget.getXLen() - 1, DL, VT));
        SDValue AND =
            DAG.getNode(ISD::AND, DL, VT, SRA,
                        DAG.getConstant(TrueSImm - FalseSImm, DL, VT));
        return DAG.getNode(ISD::ADD, DL, VT, AND, FalseV);
      }

      if (CCVal == ISD::CondCode::SETGE)
        std::swap(TrueV, FalseV);
    }

    if (combine_CC(LHS, RHS, CC, DL, DAG, Subtarget))
      return DAG.getNode(RISCVISD::SELECT_CC, DL, N->getValueType(0),
                         {LHS, RHS, CC, TrueV, FalseV});

    if (!Subtarget.hasShortForwardBranchOpt()) {
      // (select c, -1, y) -> -c | y
      if (isAllOnesConstant(TrueV)) {
        SDValue C = DAG.getSetCC(DL, VT, LHS, RHS, CCVal);
        SDValue Neg = DAG.getNegative(C, DL, VT);
        return DAG.getNode(ISD::OR, DL, VT, Neg, FalseV);
      }
      // (select c, y, -1) -> -!c | y
      if (isAllOnesConstant(FalseV)) {
        SDValue C =
            DAG.getSetCC(DL, VT, LHS, RHS, ISD::getSetCCInverse(CCVal, VT));
        SDValue Neg = DAG.getNegative(C, DL, VT);
        return DAG.getNode(ISD::OR, DL, VT, Neg, TrueV);
      }

      // (select c, 0, y) -> -!c & y
      if (isNullConstant(TrueV)) {
        SDValue C =
            DAG.getSetCC(DL, VT, LHS, RHS, ISD::getSetCCInverse(CCVal, VT));
        SDValue Neg = DAG.getNegative(C, DL, VT);
        return DAG.getNode(ISD::AND, DL, VT, Neg, FalseV);
      }
      // (select c, y, 0) -> -c & y
      if (isNullConstant(FalseV)) {
        SDValue C = DAG.getSetCC(DL, VT, LHS, RHS, CCVal);
        SDValue Neg = DAG.getNegative(C, DL, VT);
        return DAG.getNode(ISD::AND, DL, VT, Neg, TrueV);
      }
    }

    return SDValue();
  }
  case RISCVISD::BR_CC: {
    SDValue LHS = N->getOperand(1);
    SDValue RHS = N->getOperand(2);
    SDValue CC = N->getOperand(3);
    SDLoc DL(N);

    if (combine_CC(LHS, RHS, CC, DL, DAG, Subtarget))
      return DAG.getNode(RISCVISD::BR_CC, DL, N->getValueType(0),
                         N->getOperand(0), LHS, RHS, CC, N->getOperand(4));

    return SDValue();
  }
  case ISD::BITREVERSE:
    return performBITREVERSECombine(N, DAG, Subtarget);
  case ISD::FP_TO_SINT:
  case ISD::FP_TO_UINT:
    return performFP_TO_INTCombine(N, DCI, Subtarget);
  case ISD::FP_TO_SINT_SAT:
  case ISD::FP_TO_UINT_SAT:
    return performFP_TO_INT_SATCombine(N, DCI, Subtarget);
  case ISD::FCOPYSIGN: {
    EVT VT = N->getValueType(0);
    if (!VT.isVector())
      break;
    // There is a form of VFSGNJ which injects the negated sign of its second
    // operand. Try and bubble any FNEG up after the extend/round to produce
    // this optimized pattern. Avoid modifying cases where FP_ROUND and
    // TRUNC=1.
    SDValue In2 = N->getOperand(1);
    // Avoid cases where the extend/round has multiple uses, as duplicating
    // those is typically more expensive than removing a fneg.
    if (!In2.hasOneUse())
      break;
    if (In2.getOpcode() != ISD::FP_EXTEND &&
        (In2.getOpcode() != ISD::FP_ROUND || In2.getConstantOperandVal(1) != 0))
      break;
    In2 = In2.getOperand(0);
    if (In2.getOpcode() != ISD::FNEG)
      break;
    SDLoc DL(N);
    SDValue NewFPExtRound = DAG.getFPExtendOrRound(In2.getOperand(0), DL, VT);
    return DAG.getNode(ISD::FCOPYSIGN, DL, VT, N->getOperand(0),
                       DAG.getNode(ISD::FNEG, DL, VT, NewFPExtRound));
  }
  case ISD::SRA:
    if (SDValue V = performSRACombine(N, DAG, Subtarget))
      return V;
    [[fallthrough]];
  case ISD::SRL:
  case ISD::SHL: {
    SDValue ShAmt = N->getOperand(1);
    if (ShAmt.getOpcode() == RISCVISD::SPLAT_VECTOR_SPLIT_I64_VL) {
      // We don't need the upper 32 bits of a 64-bit element for a shift amount.
      SDLoc DL(N);
      EVT VT = N->getValueType(0);
      ShAmt = DAG.getNode(RISCVISD::VMV_V_X_VL, DL, VT, DAG.getUNDEF(VT),
                          ShAmt.getOperand(1),
                          DAG.getRegister(RISCV::X0, Subtarget.getXLenVT()));
      return DAG.getNode(N->getOpcode(), DL, VT, N->getOperand(0), ShAmt);
    }
    break;
  }
  case RISCVISD::ADD_VL:
  case RISCVISD::SUB_VL:
  case RISCVISD::VWADD_W_VL:
  case RISCVISD::VWADDU_W_VL:
  case RISCVISD::VWSUB_W_VL:
  case RISCVISD::VWSUBU_W_VL:
  case RISCVISD::MUL_VL:
    return combineBinOp_VLToVWBinOp_VL(N, DCI);
  case RISCVISD::VFMADD_VL:
  case RISCVISD::VFNMADD_VL:
  case RISCVISD::VFMSUB_VL:
  case RISCVISD::VFNMSUB_VL: {
    // Fold FNEG_VL into FMA opcodes.
    SDValue A = N->getOperand(0);
    SDValue B = N->getOperand(1);
    SDValue C = N->getOperand(2);
    SDValue Mask = N->getOperand(3);
    SDValue VL = N->getOperand(4);

    auto invertIfNegative = [&Mask, &VL](SDValue &V) {
      if (V.getOpcode() == RISCVISD::FNEG_VL && V.getOperand(1) == Mask &&
          V.getOperand(2) == VL) {
        // Return the negated input.
        V = V.getOperand(0);
        return true;
      }

      return false;
    };

    bool NegA = invertIfNegative(A);
    bool NegB = invertIfNegative(B);
    bool NegC = invertIfNegative(C);

    // If no operands are negated, we're done.
    if (!NegA && !NegB && !NegC)
      return SDValue();

    unsigned NewOpcode = negateFMAOpcode(N->getOpcode(), NegA != NegB, NegC);
    return DAG.getNode(NewOpcode, SDLoc(N), N->getValueType(0), A, B, C, Mask,
                       VL);
  }
  case ISD::BITCAST: {
    assert(Subtarget.useRVVForFixedLengthVectors());
    SDValue N0 = N->getOperand(0);
    EVT VT = N->getValueType(0);
    EVT SrcVT = N0.getValueType();
    // If this is a bitcast between a MVT::v4i1/v2i1/v1i1 and an illegal integer
    // type, widen both sides to avoid a trip through memory.
    if ((SrcVT == MVT::v1i1 || SrcVT == MVT::v2i1 || SrcVT == MVT::v4i1) &&
        VT.isScalarInteger()) {
      unsigned NumConcats = 8 / SrcVT.getVectorNumElements();
      SmallVector<SDValue, 4> Ops(NumConcats, DAG.getUNDEF(SrcVT));
      Ops[0] = N0;
      SDLoc DL(N);
      N0 = DAG.getNode(ISD::CONCAT_VECTORS, DL, MVT::v8i1, Ops);
      N0 = DAG.getBitcast(MVT::i8, N0);
      return DAG.getNode(ISD::TRUNCATE, DL, VT, N0);
    }

    return SDValue();
  }
  }

  return SDValue();
}

bool RISCVTargetLowering::isDesirableToCommuteWithShift(
    const SDNode *N, CombineLevel Level) const {
  assert((N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRA ||
          N->getOpcode() == ISD::SRL) &&
         "Expected shift op");

  // The following folds are only desirable if `(OP _, c1 << c2)` can be
  // materialised in fewer instructions than `(OP _, c1)`:
  //
  //   (shl (add x, c1), c2) -> (add (shl x, c2), c1 << c2)
  //   (shl (or x, c1), c2) -> (or (shl x, c2), c1 << c2)
  SDValue N0 = N->getOperand(0);
  EVT Ty = N0.getValueType();
  if (Ty.isScalarInteger() &&
      (N0.getOpcode() == ISD::ADD || N0.getOpcode() == ISD::OR)) {
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    auto *C2 = dyn_cast<ConstantSDNode>(N->getOperand(1));
    if (C1 && C2) {
      const APInt &C1Int = C1->getAPIntValue();
      APInt ShiftedC1Int = C1Int << C2->getAPIntValue();

      // We can materialise `c1 << c2` into an add immediate, so it's "free",
      // and the combine should happen, to potentially allow further combines
      // later.
      if (ShiftedC1Int.getMinSignedBits() <= 64 &&
          isLegalAddImmediate(ShiftedC1Int.getSExtValue()))
        return true;

      // We can materialise `c1` in an add immediate, so it's "free", and the
      // combine should be prevented.
      if (C1Int.getMinSignedBits() <= 64 &&
          isLegalAddImmediate(C1Int.getSExtValue()))
        return false;

      // Neither constant will fit into an immediate, so find materialisation
      // costs.
      int C1Cost = RISCVMatInt::getIntMatCost(C1Int, Ty.getSizeInBits(),
                                              Subtarget.getFeatureBits(),
                                              /*CompressionCost*/true);
      int ShiftedC1Cost = RISCVMatInt::getIntMatCost(
          ShiftedC1Int, Ty.getSizeInBits(), Subtarget.getFeatureBits(),
          /*CompressionCost*/true);

      // Materialising `c1` is cheaper than materialising `c1 << c2`, so the
      // combine should be prevented.
      if (C1Cost < ShiftedC1Cost)
        return false;
    }
  }
  return true;
}

bool RISCVTargetLowering::targetShrinkDemandedConstant(
    SDValue Op, const APInt &DemandedBits, const APInt &DemandedElts,
    TargetLoweringOpt &TLO) const {
  // Delay this optimization as late as possible.
  if (!TLO.LegalOps)
    return false;

  EVT VT = Op.getValueType();
  if (VT.isVector())
    return false;

  unsigned Opcode = Op.getOpcode();
  if (Opcode != ISD::AND && Opcode != ISD::OR && Opcode != ISD::XOR)
    return false;

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op.getOperand(1));
  if (!C)
    return false;

  const APInt &Mask = C->getAPIntValue();

  // Clear all non-demanded bits initially.
  APInt ShrunkMask = Mask & DemandedBits;

  // Try to make a smaller immediate by setting undemanded bits.

  APInt ExpandedMask = Mask | ~DemandedBits;

  auto IsLegalMask = [ShrunkMask, ExpandedMask](const APInt &Mask) -> bool {
    return ShrunkMask.isSubsetOf(Mask) && Mask.isSubsetOf(ExpandedMask);
  };
  auto UseMask = [Mask, Op, &TLO](const APInt &NewMask) -> bool {
    if (NewMask == Mask)
      return true;
    SDLoc DL(Op);
    SDValue NewC = TLO.DAG.getConstant(NewMask, DL, Op.getValueType());
    SDValue NewOp = TLO.DAG.getNode(Op.getOpcode(), DL, Op.getValueType(),
                                    Op.getOperand(0), NewC);
    return TLO.CombineTo(Op, NewOp);
  };

  // If the shrunk mask fits in sign extended 12 bits, let the target
  // independent code apply it.
  if (ShrunkMask.isSignedIntN(12))
    return false;

  // And has a few special cases for zext.
  if (Opcode == ISD::AND) {
    // Preserve (and X, 0xffff), if zext.h exists use zext.h,
    // otherwise use SLLI + SRLI.
    APInt NewMask = APInt(Mask.getBitWidth(), 0xffff);
    if (IsLegalMask(NewMask))
      return UseMask(NewMask);

    // Try to preserve (and X, 0xffffffff), the (zext_inreg X, i32) pattern.
    if (VT == MVT::i64) {
      APInt NewMask = APInt(64, 0xffffffff);
      if (IsLegalMask(NewMask))
        return UseMask(NewMask);
    }
  }

  // For the remaining optimizations, we need to be able to make a negative
  // number through a combination of mask and undemanded bits.
  if (!ExpandedMask.isNegative())
    return false;

  // What is the fewest number of bits we need to represent the negative number.
  unsigned MinSignedBits = ExpandedMask.getMinSignedBits();

  // Try to make a 12 bit negative immediate. If that fails try to make a 32
  // bit negative immediate unless the shrunk immediate already fits in 32 bits.
  // If we can't create a simm12, we shouldn't change opaque constants.
  APInt NewMask = ShrunkMask;
  if (MinSignedBits <= 12)
    NewMask.setBitsFrom(11);
  else if (!C->isOpaque() && MinSignedBits <= 32 && !ShrunkMask.isSignedIntN(32))
    NewMask.setBitsFrom(31);
  else
    return false;

  // Check that our new mask is a subset of the demanded mask.
  assert(IsLegalMask(NewMask));
  return UseMask(NewMask);
}

static uint64_t computeGREVOrGORC(uint64_t x, unsigned ShAmt, bool IsGORC) {
  static const uint64_t GREVMasks[] = {
      0x5555555555555555ULL, 0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL,
      0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL};

  for (unsigned Stage = 0; Stage != 6; ++Stage) {
    unsigned Shift = 1 << Stage;
    if (ShAmt & Shift) {
      uint64_t Mask = GREVMasks[Stage];
      uint64_t Res = ((x & Mask) << Shift) | ((x >> Shift) & Mask);
      if (IsGORC)
        Res |= x;
      x = Res;
    }
  }

  return x;
}

void RISCVTargetLowering::computeKnownBitsForTargetNode(const SDValue Op,
                                                        KnownBits &Known,
                                                        const APInt &DemandedElts,
                                                        const SelectionDAG &DAG,
                                                        unsigned Depth) const {
  unsigned BitWidth = Known.getBitWidth();
  unsigned Opc = Op.getOpcode();
  assert((Opc >= ISD::BUILTIN_OP_END ||
          Opc == ISD::INTRINSIC_WO_CHAIN ||
          Opc == ISD::INTRINSIC_W_CHAIN ||
          Opc == ISD::INTRINSIC_VOID) &&
         "Should use MaskedValueIsZero if you don't know whether Op"
         " is a target node!");

  Known.resetAll();
  switch (Opc) {
  default: break;
  case RISCVISD::SELECT_CC: {
    Known = DAG.computeKnownBits(Op.getOperand(4), Depth + 1);
    // If we don't know any bits, early out.
    if (Known.isUnknown())
      break;
    KnownBits Known2 = DAG.computeKnownBits(Op.getOperand(3), Depth + 1);

    // Only known if known in both the LHS and RHS.
    Known = KnownBits::commonBits(Known, Known2);
    break;
  }
  case RISCVISD::REMUW: {
    KnownBits Known2;
    Known = DAG.computeKnownBits(Op.getOperand(0), DemandedElts, Depth + 1);
    Known2 = DAG.computeKnownBits(Op.getOperand(1), DemandedElts, Depth + 1);
    // We only care about the lower 32 bits.
    Known = KnownBits::urem(Known.trunc(32), Known2.trunc(32));
    // Restore the original width by sign extending.
    Known = Known.sext(BitWidth);
    break;
  }
  case RISCVISD::DIVUW: {
    KnownBits Known2;
    Known = DAG.computeKnownBits(Op.getOperand(0), DemandedElts, Depth + 1);
    Known2 = DAG.computeKnownBits(Op.getOperand(1), DemandedElts, Depth + 1);
    // We only care about the lower 32 bits.
    Known = KnownBits::udiv(Known.trunc(32), Known2.trunc(32));
    // Restore the original width by sign extending.
    Known = Known.sext(BitWidth);
    break;
  }
  case RISCVISD::CTZW: {
    KnownBits Known2 = DAG.computeKnownBits(Op.getOperand(0), Depth + 1);
    unsigned PossibleTZ = Known2.trunc(32).countMaxTrailingZeros();
    unsigned LowBits = Log2_32(PossibleTZ) + 1;
    Known.Zero.setBitsFrom(LowBits);
    break;
  }
  case RISCVISD::CLZW: {
    KnownBits Known2 = DAG.computeKnownBits(Op.getOperand(0), Depth + 1);
    unsigned PossibleLZ = Known2.trunc(32).countMaxLeadingZeros();
    unsigned LowBits = Log2_32(PossibleLZ) + 1;
    Known.Zero.setBitsFrom(LowBits);
    break;
  }
  case RISCVISD::BREV8:
  case RISCVISD::ORC_B: {
    // FIXME: This is based on the non-ratified Zbp GREV and GORC where a
    // control value of 7 is equivalent to brev8 and orc.b.
    Known = DAG.computeKnownBits(Op.getOperand(0), Depth + 1);
    bool IsGORC = Op.getOpcode() == RISCVISD::ORC_B;
    // To compute zeros, we need to invert the value and invert it back after.
    Known.Zero =
        ~computeGREVOrGORC(~Known.Zero.getZExtValue(), 7, IsGORC);
    Known.One = computeGREVOrGORC(Known.One.getZExtValue(), 7, IsGORC);
    break;
  }
  case RISCVISD::READ_VLENB: {
    // We can use the minimum and maximum VLEN values to bound VLENB.  We
    // know VLEN must be a power of two.
    const unsigned MinVLenB = Subtarget.getRealMinVLen() / 8;
    const unsigned MaxVLenB = Subtarget.getRealMaxVLen() / 8;
    assert(MinVLenB > 0 && "READ_VLENB without vector extension enabled?");
    Known.Zero.setLowBits(Log2_32(MinVLenB));
    Known.Zero.setBitsFrom(Log2_32(MaxVLenB)+1);
    if (MaxVLenB == MinVLenB)
      Known.One.setBit(Log2_32(MinVLenB));
    break;
  }
  case ISD::INTRINSIC_W_CHAIN:
  case ISD::INTRINSIC_WO_CHAIN:
    break;
  }
}

unsigned RISCVTargetLowering::ComputeNumSignBitsForTargetNode(
    SDValue Op, const APInt &DemandedElts, const SelectionDAG &DAG,
    unsigned Depth) const {
  switch (Op.getOpcode()) {
  default:
    break;
  case RISCVISD::SELECT_CC: {
    unsigned Tmp =
        DAG.ComputeNumSignBits(Op.getOperand(3), DemandedElts, Depth + 1);
    if (Tmp == 1) return 1;  // Early out.
    unsigned Tmp2 =
        DAG.ComputeNumSignBits(Op.getOperand(4), DemandedElts, Depth + 1);
    return std::min(Tmp, Tmp2);
  }
  case RISCVISD::SLLW:
  case RISCVISD::SRAW:
  case RISCVISD::SRLW:
  case RISCVISD::DIVW:
  case RISCVISD::DIVUW:
  case RISCVISD::REMUW:
  case RISCVISD::ROLW:
  case RISCVISD::RORW:
  case RISCVISD::ABSW:
  case RISCVISD::FCVT_W_RV64:
  case RISCVISD::FCVT_WU_RV64:
  case RISCVISD::STRICT_FCVT_W_RV64:
  case RISCVISD::STRICT_FCVT_WU_RV64:
    // TODO: As the result is sign-extended, this is conservatively correct. A
    // more precise answer could be calculated for SRAW depending on known
    // bits in the shift amount.
    return 33;
  case RISCVISD::VMV_X_S: {
    // The number of sign bits of the scalar result is computed by obtaining the
    // element type of the input vector operand, subtracting its width from the
    // XLEN, and then adding one (sign bit within the element type). If the
    // element type is wider than XLen, the least-significant XLEN bits are
    // taken.
    unsigned XLen = Subtarget.getXLen();
    unsigned EltBits = Op.getOperand(0).getScalarValueSizeInBits();
    if (EltBits <= XLen)
      return XLen - EltBits + 1;
    break;
  }
  case ISD::INTRINSIC_W_CHAIN: {
    unsigned IntNo = Op.getConstantOperandVal(1);
    switch (IntNo) {
    default:
      break;
    case Intrinsic::riscv_masked_atomicrmw_xchg_i64:
    case Intrinsic::riscv_masked_atomicrmw_add_i64:
    case Intrinsic::riscv_masked_atomicrmw_sub_i64:
    case Intrinsic::riscv_masked_atomicrmw_nand_i64:
    case Intrinsic::riscv_masked_atomicrmw_max_i64:
    case Intrinsic::riscv_masked_atomicrmw_min_i64:
    case Intrinsic::riscv_masked_atomicrmw_umax_i64:
    case Intrinsic::riscv_masked_atomicrmw_umin_i64:
    case Intrinsic::riscv_masked_cmpxchg_i64:
      // riscv_masked_{atomicrmw_*,cmpxchg} intrinsics represent an emulated
      // narrow atomic operation. These are implemented using atomic
      // operations at the minimum supported atomicrmw/cmpxchg width whose
      // result is then sign extended to XLEN. With +A, the minimum width is
      // 32 for both 64 and 32.
      assert(Subtarget.getXLen() == 64);
      assert(getMinCmpXchgSizeInBits() == 32);
      assert(Subtarget.hasStdExtA());
      return 33;
    }
  }
  }

  return 1;
}

const Constant *
RISCVTargetLowering::getTargetConstantFromLoad(LoadSDNode *Ld) const {
  assert(Ld && "Unexpected null LoadSDNode");
  if (!ISD::isNormalLoad(Ld))
    return nullptr;

  SDValue Ptr = Ld->getBasePtr();

  // Only constant pools with no offset are supported.
  auto GetSupportedConstantPool = [](SDValue Ptr) -> ConstantPoolSDNode * {
    auto *CNode = dyn_cast<ConstantPoolSDNode>(Ptr);
    if (!CNode || CNode->isMachineConstantPoolEntry() ||
        CNode->getOffset() != 0)
      return nullptr;

    return CNode;
  };

  // Simple case, LLA.
  if (Ptr.getOpcode() == RISCVISD::LLA) {
    auto *CNode = GetSupportedConstantPool(Ptr);
    if (!CNode || CNode->getTargetFlags() != 0)
      return nullptr;

    return CNode->getConstVal();
  }

  // Look for a HI and ADD_LO pair.
  if (Ptr.getOpcode() != RISCVISD::ADD_LO ||
      Ptr.getOperand(0).getOpcode() != RISCVISD::HI)
    return nullptr;

  auto *CNodeLo = GetSupportedConstantPool(Ptr.getOperand(1));
  auto *CNodeHi = GetSupportedConstantPool(Ptr.getOperand(0).getOperand(0));

  if (!CNodeLo || CNodeLo->getTargetFlags() != RISCVII::MO_LO ||
      !CNodeHi || CNodeHi->getTargetFlags() != RISCVII::MO_HI)
    return nullptr;

  if (CNodeLo->getConstVal() != CNodeHi->getConstVal())
    return nullptr;

  return CNodeLo->getConstVal();
}

static MachineBasicBlock *emitReadCycleWidePseudo(MachineInstr &MI,
                                                  MachineBasicBlock *BB) {
  assert(MI.getOpcode() == RISCV::ReadCycleWide && "Unexpected instruction");

  // To read the 64-bit cycle CSR on a 32-bit target, we read the two halves.
  // Should the count have wrapped while it was being read, we need to try
  // again.
  // ...
  // read:
  // rdcycleh x3 # load high word of cycle
  // rdcycle  x2 # load low word of cycle
  // rdcycleh x4 # load high word of cycle
  // bne x3, x4, read # check if high word reads match, otherwise try again
  // ...

  MachineFunction &MF = *BB->getParent();
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  MachineBasicBlock *LoopMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MF.insert(It, LoopMBB);

  MachineBasicBlock *DoneMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MF.insert(It, DoneMBB);

  // Transfer the remainder of BB and its successor edges to DoneMBB.
  DoneMBB->splice(DoneMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(BB);

  BB->addSuccessor(LoopMBB);

  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  Register ReadAgainReg = RegInfo.createVirtualRegister(&RISCV::GPRRegClass);
  Register LoReg = MI.getOperand(0).getReg();
  Register HiReg = MI.getOperand(1).getReg();
  DebugLoc DL = MI.getDebugLoc();

  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  BuildMI(LoopMBB, DL, TII->get(RISCV::CSRRS), HiReg)
      .addImm(RISCVSysReg::lookupSysRegByName("CYCLEH")->Encoding)
      .addReg(RISCV::X0);
  BuildMI(LoopMBB, DL, TII->get(RISCV::CSRRS), LoReg)
      .addImm(RISCVSysReg::lookupSysRegByName("CYCLE")->Encoding)
      .addReg(RISCV::X0);
  BuildMI(LoopMBB, DL, TII->get(RISCV::CSRRS), ReadAgainReg)
      .addImm(RISCVSysReg::lookupSysRegByName("CYCLEH")->Encoding)
      .addReg(RISCV::X0);

  BuildMI(LoopMBB, DL, TII->get(RISCV::BNE))
      .addReg(HiReg)
      .addReg(ReadAgainReg)
      .addMBB(LoopMBB);

  LoopMBB->addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(DoneMBB);

  MI.eraseFromParent();

  return DoneMBB;
}

static MachineBasicBlock *emitSplitF64Pseudo(MachineInstr &MI,
                                             MachineBasicBlock *BB) {
  assert(MI.getOpcode() == RISCV::SplitF64Pseudo && "Unexpected instruction");

  MachineFunction &MF = *BB->getParent();
  DebugLoc DL = MI.getDebugLoc();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  Register LoReg = MI.getOperand(0).getReg();
  Register HiReg = MI.getOperand(1).getReg();
  Register SrcReg = MI.getOperand(2).getReg();
  const TargetRegisterClass *SrcRC = &RISCV::FPR64RegClass;
  int FI = MF.getInfo<RISCVMachineFunctionInfo>()->getMoveF64FrameIndex(MF);

  TII.storeRegToStackSlot(*BB, MI, SrcReg, MI.getOperand(2).isKill(), FI, SrcRC,
                          RI);
  MachinePointerInfo MPI = MachinePointerInfo::getFixedStack(MF, FI);
  MachineMemOperand *MMOLo =
      MF.getMachineMemOperand(MPI, MachineMemOperand::MOLoad, 4, Align(8));
  MachineMemOperand *MMOHi = MF.getMachineMemOperand(
      MPI.getWithOffset(4), MachineMemOperand::MOLoad, 4, Align(8));
  BuildMI(*BB, MI, DL, TII.get(RISCV::LW), LoReg)
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMOLo);
  BuildMI(*BB, MI, DL, TII.get(RISCV::LW), HiReg)
      .addFrameIndex(FI)
      .addImm(4)
      .addMemOperand(MMOHi);
  MI.eraseFromParent(); // The pseudo instruction is gone now.
  return BB;
}

static MachineBasicBlock *emitBuildPairF64Pseudo(MachineInstr &MI,
                                                 MachineBasicBlock *BB) {
  assert(MI.getOpcode() == RISCV::BuildPairF64Pseudo &&
         "Unexpected instruction");

  MachineFunction &MF = *BB->getParent();
  DebugLoc DL = MI.getDebugLoc();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  Register DstReg = MI.getOperand(0).getReg();
  Register LoReg = MI.getOperand(1).getReg();
  Register HiReg = MI.getOperand(2).getReg();
  const TargetRegisterClass *DstRC = &RISCV::FPR64RegClass;
  int FI = MF.getInfo<RISCVMachineFunctionInfo>()->getMoveF64FrameIndex(MF);

  MachinePointerInfo MPI = MachinePointerInfo::getFixedStack(MF, FI);
  MachineMemOperand *MMOLo =
      MF.getMachineMemOperand(MPI, MachineMemOperand::MOStore, 4, Align(8));
  MachineMemOperand *MMOHi = MF.getMachineMemOperand(
      MPI.getWithOffset(4), MachineMemOperand::MOStore, 4, Align(8));
  BuildMI(*BB, MI, DL, TII.get(RISCV::SW))
      .addReg(LoReg, getKillRegState(MI.getOperand(1).isKill()))
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMOLo);
  BuildMI(*BB, MI, DL, TII.get(RISCV::SW))
      .addReg(HiReg, getKillRegState(MI.getOperand(2).isKill()))
      .addFrameIndex(FI)
      .addImm(4)
      .addMemOperand(MMOHi);
  TII.loadRegFromStackSlot(*BB, MI, DstReg, FI, DstRC, RI);
  MI.eraseFromParent(); // The pseudo instruction is gone now.
  return BB;
}

static bool isSelectPseudo(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    return false;
  case RISCV::Select_GPR_Using_CC_GPR:
  case RISCV::Select_FPR16_Using_CC_GPR:
  case RISCV::Select_FPR32_Using_CC_GPR:
  case RISCV::Select_FPR64_Using_CC_GPR:
    return true;
  }
}

static MachineBasicBlock *emitQuietFCMP(MachineInstr &MI, MachineBasicBlock *BB,
                                        unsigned RelOpcode, unsigned EqOpcode,
                                        const RISCVSubtarget &Subtarget) {
  DebugLoc DL = MI.getDebugLoc();
  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();
  MachineRegisterInfo &MRI = BB->getParent()->getRegInfo();
  Register SavedFFlags = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();

  // Save the current FFLAGS.
  BuildMI(*BB, MI, DL, TII.get(RISCV::ReadFFLAGS), SavedFFlags);

  auto MIB = BuildMI(*BB, MI, DL, TII.get(RelOpcode), DstReg)
                 .addReg(Src1Reg)
                 .addReg(Src2Reg);
  if (MI.getFlag(MachineInstr::MIFlag::NoFPExcept))
    MIB->setFlag(MachineInstr::MIFlag::NoFPExcept);

  // Restore the FFLAGS.
  BuildMI(*BB, MI, DL, TII.get(RISCV::WriteFFLAGS))
      .addReg(SavedFFlags, RegState::Kill);

  // Issue a dummy FEQ opcode to raise exception for signaling NaNs.
  auto MIB2 = BuildMI(*BB, MI, DL, TII.get(EqOpcode), RISCV::X0)
                  .addReg(Src1Reg, getKillRegState(MI.getOperand(1).isKill()))
                  .addReg(Src2Reg, getKillRegState(MI.getOperand(2).isKill()));
  if (MI.getFlag(MachineInstr::MIFlag::NoFPExcept))
    MIB2->setFlag(MachineInstr::MIFlag::NoFPExcept);

  // Erase the pseudoinstruction.
  MI.eraseFromParent();
  return BB;
}

static MachineBasicBlock *
EmitLoweredCascadedSelect(MachineInstr &First, MachineInstr &Second,
                          MachineBasicBlock *ThisMBB,
                          const RISCVSubtarget &Subtarget) {
  // Select_FPRX_ (rs1, rs2, imm, rs4, (Select_FPRX_ rs1, rs2, imm, rs4, rs5)
  // Without this, custom-inserter would have generated:
  //
  //   A
  //   | \
  //   |  B
  //   | /
  //   C
  //   | \
  //   |  D
  //   | /
  //   E
  //
  // A: X = ...; Y = ...
  // B: empty
  // C: Z = PHI [X, A], [Y, B]
  // D: empty
  // E: PHI [X, C], [Z, D]
  //
  // If we lower both Select_FPRX_ in a single step, we can instead generate:
  //
  //   A
  //   | \
  //   |  C
  //   | /|
  //   |/ |
  //   |  |
  //   |  D
  //   | /
  //   E
  //
  // A: X = ...; Y = ...
  // D: empty
  // E: PHI [X, A], [X, C], [Y, D]

  const RISCVInstrInfo &TII = *Subtarget.getInstrInfo();
  const DebugLoc &DL = First.getDebugLoc();
  const BasicBlock *LLVM_BB = ThisMBB->getBasicBlock();
  MachineFunction *F = ThisMBB->getParent();
  MachineBasicBlock *FirstMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SecondMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = ++ThisMBB->getIterator();
  F->insert(It, FirstMBB);
  F->insert(It, SecondMBB);
  F->insert(It, SinkMBB);

  // Transfer the remainder of ThisMBB and its successor edges to SinkMBB.
  SinkMBB->splice(SinkMBB->begin(), ThisMBB,
                  std::next(MachineBasicBlock::iterator(First)),
                  ThisMBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(ThisMBB);

  // Fallthrough block for ThisMBB.
  ThisMBB->addSuccessor(FirstMBB);
  // Fallthrough block for FirstMBB.
  FirstMBB->addSuccessor(SecondMBB);
  ThisMBB->addSuccessor(SinkMBB);
  FirstMBB->addSuccessor(SinkMBB);
  // This is fallthrough.
  SecondMBB->addSuccessor(SinkMBB);

  auto FirstCC = static_cast<RISCVCC::CondCode>(First.getOperand(3).getImm());
  Register FLHS = First.getOperand(1).getReg();
  Register FRHS = First.getOperand(2).getReg();
  // Insert appropriate branch.
  BuildMI(FirstMBB, DL, TII.getBrCond(FirstCC))
      .addReg(FLHS)
      .addReg(FRHS)
      .addMBB(SinkMBB);

  Register SLHS = Second.getOperand(1).getReg();
  Register SRHS = Second.getOperand(2).getReg();
  Register Op1Reg4 = First.getOperand(4).getReg();
  Register Op1Reg5 = First.getOperand(5).getReg();

  auto SecondCC = static_cast<RISCVCC::CondCode>(Second.getOperand(3).getImm());
  // Insert appropriate branch.
  BuildMI(ThisMBB, DL, TII.getBrCond(SecondCC))
      .addReg(SLHS)
      .addReg(SRHS)
      .addMBB(SinkMBB);

  Register DestReg = Second.getOperand(0).getReg();
  Register Op2Reg4 = Second.getOperand(4).getReg();
  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(RISCV::PHI), DestReg)
      .addReg(Op2Reg4)
      .addMBB(ThisMBB)
      .addReg(Op1Reg4)
      .addMBB(FirstMBB)
      .addReg(Op1Reg5)
      .addMBB(SecondMBB);

  // Now remove the Select_FPRX_s.
  First.eraseFromParent();
  Second.eraseFromParent();
  return SinkMBB;
}

static MachineBasicBlock *emitSelectPseudo(MachineInstr &MI,
                                           MachineBasicBlock *BB,
                                           const RISCVSubtarget &Subtarget) {
  // To "insert" Select_* instructions, we actually have to insert the triangle
  // control-flow pattern.  The incoming instructions know the destination vreg
  // to set, the condition code register to branch on, the true/false values to
  // select between, and the condcode to use to select the appropriate branch.
  //
  // We produce the following control flow:
  //     HeadMBB
  //     |  \
  //     |  IfFalseMBB
  //     | /
  //    TailMBB
  //
  // When we find a sequence of selects we attempt to optimize their emission
  // by sharing the control flow. Currently we only handle cases where we have
  // multiple selects with the exact same condition (same LHS, RHS and CC).
  // The selects may be interleaved with other instructions if the other
  // instructions meet some requirements we deem safe:
  // - They are debug instructions. Otherwise,
  // - They do not have side-effects, do not access memory and their inputs do
  //   not depend on the results of the select pseudo-instructions.
  // The TrueV/FalseV operands of the selects cannot depend on the result of
  // previous selects in the sequence.
  // These conditions could be further relaxed. See the X86 target for a
  // related approach and more information.
  //
  // Select_FPRX_ (rs1, rs2, imm, rs4, (Select_FPRX_ rs1, rs2, imm, rs4, rs5))
  // is checked here and handled by a separate function -
  // EmitLoweredCascadedSelect.
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  auto CC = static_cast<RISCVCC::CondCode>(MI.getOperand(3).getImm());

  SmallVector<MachineInstr *, 4> SelectDebugValues;
  SmallSet<Register, 4> SelectDests;
  SelectDests.insert(MI.getOperand(0).getReg());

  MachineInstr *LastSelectPseudo = &MI;
  auto Next = next_nodbg(MI.getIterator(), BB->instr_end());
  if (MI.getOpcode() != RISCV::Select_GPR_Using_CC_GPR && Next != BB->end() &&
      Next->getOpcode() == MI.getOpcode() &&
      Next->getOperand(5).getReg() == MI.getOperand(0).getReg() &&
      Next->getOperand(5).isKill()) {
    return EmitLoweredCascadedSelect(MI, *Next, BB, Subtarget);
  }

  for (auto E = BB->end(), SequenceMBBI = MachineBasicBlock::iterator(MI);
       SequenceMBBI != E; ++SequenceMBBI) {
    if (SequenceMBBI->isDebugInstr())
      continue;
    if (isSelectPseudo(*SequenceMBBI)) {
      if (SequenceMBBI->getOperand(1).getReg() != LHS ||
          SequenceMBBI->getOperand(2).getReg() != RHS ||
          SequenceMBBI->getOperand(3).getImm() != CC ||
          SelectDests.count(SequenceMBBI->getOperand(4).getReg()) ||
          SelectDests.count(SequenceMBBI->getOperand(5).getReg()))
        break;
      LastSelectPseudo = &*SequenceMBBI;
      SequenceMBBI->collectDebugValues(SelectDebugValues);
      SelectDests.insert(SequenceMBBI->getOperand(0).getReg());
      continue;
    }
    if (SequenceMBBI->hasUnmodeledSideEffects() ||
        SequenceMBBI->mayLoadOrStore())
      break;
    if (llvm::any_of(SequenceMBBI->operands(), [&](MachineOperand &MO) {
          return MO.isReg() && MO.isUse() && SelectDests.count(MO.getReg());
        }))
      break;
  }

  const RISCVInstrInfo &TII = *Subtarget.getInstrInfo();
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction::iterator I = ++BB->getIterator();

  MachineBasicBlock *HeadMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *TailMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *IfFalseMBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(I, IfFalseMBB);
  F->insert(I, TailMBB);

  // Transfer debug instructions associated with the selects to TailMBB.
  for (MachineInstr *DebugInstr : SelectDebugValues) {
    TailMBB->push_back(DebugInstr->removeFromParent());
  }

  // Move all instructions after the sequence to TailMBB.
  TailMBB->splice(TailMBB->end(), HeadMBB,
                  std::next(LastSelectPseudo->getIterator()), HeadMBB->end());
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi nodes for the selects.
  TailMBB->transferSuccessorsAndUpdatePHIs(HeadMBB);
  // Set the successors for HeadMBB.
  HeadMBB->addSuccessor(IfFalseMBB);
  HeadMBB->addSuccessor(TailMBB);

  // Insert appropriate branch.
  BuildMI(HeadMBB, DL, TII.getBrCond(CC))
    .addReg(LHS)
    .addReg(RHS)
    .addMBB(TailMBB);

  // IfFalseMBB just falls through to TailMBB.
  IfFalseMBB->addSuccessor(TailMBB);

  // Create PHIs for all of the select pseudo-instructions.
  auto SelectMBBI = MI.getIterator();
  auto SelectEnd = std::next(LastSelectPseudo->getIterator());
  auto InsertionPoint = TailMBB->begin();
  while (SelectMBBI != SelectEnd) {
    auto Next = std::next(SelectMBBI);
    if (isSelectPseudo(*SelectMBBI)) {
      // %Result = phi [ %TrueValue, HeadMBB ], [ %FalseValue, IfFalseMBB ]
      BuildMI(*TailMBB, InsertionPoint, SelectMBBI->getDebugLoc(),
              TII.get(RISCV::PHI), SelectMBBI->getOperand(0).getReg())
          .addReg(SelectMBBI->getOperand(4).getReg())
          .addMBB(HeadMBB)
          .addReg(SelectMBBI->getOperand(5).getReg())
          .addMBB(IfFalseMBB);
      SelectMBBI->eraseFromParent();
    }
    SelectMBBI = Next;
  }

  F->getProperties().reset(MachineFunctionProperties::Property::NoPHIs);
  return TailMBB;
}

static MachineBasicBlock *emitFROUND(MachineInstr &MI, MachineBasicBlock *MBB,
                                     const RISCVSubtarget &Subtarget) {
  unsigned CmpOpc, F2IOpc, I2FOpc, FSGNJOpc, FSGNJXOpc;
  const TargetRegisterClass *RC;
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("Unexpected opcode");
  case RISCV::PseudoFROUND_H:
    CmpOpc = RISCV::FLT_H;
    F2IOpc = RISCV::FCVT_W_H;
    I2FOpc = RISCV::FCVT_H_W;
    FSGNJOpc = RISCV::FSGNJ_H;
    FSGNJXOpc = RISCV::FSGNJX_H;
    RC = &RISCV::FPR16RegClass;
    break;
  case RISCV::PseudoFROUND_S:
    CmpOpc = RISCV::FLT_S;
    F2IOpc = RISCV::FCVT_W_S;
    I2FOpc = RISCV::FCVT_S_W;
    FSGNJOpc = RISCV::FSGNJ_S;
    FSGNJXOpc = RISCV::FSGNJX_S;
    RC = &RISCV::FPR32RegClass;
    break;
  case RISCV::PseudoFROUND_D:
    assert(Subtarget.is64Bit() && "Expected 64-bit GPR.");
    CmpOpc = RISCV::FLT_D;
    F2IOpc = RISCV::FCVT_L_D;
    I2FOpc = RISCV::FCVT_D_L;
    FSGNJOpc = RISCV::FSGNJ_D;
    FSGNJXOpc = RISCV::FSGNJX_D;
    RC = &RISCV::FPR64RegClass;
    break;
  }

  const BasicBlock *BB = MBB->getBasicBlock();
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction::iterator I = ++MBB->getIterator();

  MachineFunction *F = MBB->getParent();
  MachineBasicBlock *CvtMBB = F->CreateMachineBasicBlock(BB);
  MachineBasicBlock *DoneMBB = F->CreateMachineBasicBlock(BB);

  F->insert(I, CvtMBB);
  F->insert(I, DoneMBB);
  // Move all instructions after the sequence to DoneMBB.
  DoneMBB->splice(DoneMBB->end(), MBB, MachineBasicBlock::iterator(MI),
                  MBB->end());
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi nodes for the selects.
  DoneMBB->transferSuccessorsAndUpdatePHIs(MBB);
  // Set the successors for MBB.
  MBB->addSuccessor(CvtMBB);
  MBB->addSuccessor(DoneMBB);

  Register DstReg = MI.getOperand(0).getReg();
  Register SrcReg = MI.getOperand(1).getReg();
  Register MaxReg = MI.getOperand(2).getReg();
  int64_t FRM = MI.getOperand(3).getImm();

  const RISCVInstrInfo &TII = *Subtarget.getInstrInfo();
  MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();

  Register FabsReg = MRI.createVirtualRegister(RC);
  BuildMI(MBB, DL, TII.get(FSGNJXOpc), FabsReg).addReg(SrcReg).addReg(SrcReg);

  // Compare the FP value to the max value.
  Register CmpReg = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  auto MIB =
      BuildMI(MBB, DL, TII.get(CmpOpc), CmpReg).addReg(FabsReg).addReg(MaxReg);
  if (MI.getFlag(MachineInstr::MIFlag::NoFPExcept))
    MIB->setFlag(MachineInstr::MIFlag::NoFPExcept);

  // Insert branch.
  BuildMI(MBB, DL, TII.get(RISCV::BEQ))
      .addReg(CmpReg)
      .addReg(RISCV::X0)
      .addMBB(DoneMBB);

  CvtMBB->addSuccessor(DoneMBB);

  // Convert to integer.
  Register F2IReg = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  MIB = BuildMI(CvtMBB, DL, TII.get(F2IOpc), F2IReg).addReg(SrcReg).addImm(FRM);
  if (MI.getFlag(MachineInstr::MIFlag::NoFPExcept))
    MIB->setFlag(MachineInstr::MIFlag::NoFPExcept);

  // Convert back to FP.
  Register I2FReg = MRI.createVirtualRegister(RC);
  MIB = BuildMI(CvtMBB, DL, TII.get(I2FOpc), I2FReg).addReg(F2IReg).addImm(FRM);
  if (MI.getFlag(MachineInstr::MIFlag::NoFPExcept))
    MIB->setFlag(MachineInstr::MIFlag::NoFPExcept);

  // Restore the sign bit.
  Register CvtReg = MRI.createVirtualRegister(RC);
  BuildMI(CvtMBB, DL, TII.get(FSGNJOpc), CvtReg).addReg(I2FReg).addReg(SrcReg);

  // Merge the results.
  BuildMI(*DoneMBB, DoneMBB->begin(), DL, TII.get(RISCV::PHI), DstReg)
      .addReg(SrcReg)
      .addMBB(MBB)
      .addReg(CvtReg)
      .addMBB(CvtMBB);

  MI.eraseFromParent();
  return DoneMBB;
}

MachineBasicBlock *
RISCVTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("Unexpected instr type to insert");
  case RISCV::ReadCycleWide:
    assert(!Subtarget.is64Bit() &&
           "ReadCycleWrite is only to be used on riscv32");
    return emitReadCycleWidePseudo(MI, BB);
  case RISCV::Select_GPR_Using_CC_GPR:
  case RISCV::Select_FPR16_Using_CC_GPR:
  case RISCV::Select_FPR32_Using_CC_GPR:
  case RISCV::Select_FPR64_Using_CC_GPR:
    return emitSelectPseudo(MI, BB, Subtarget);
  case RISCV::BuildPairF64Pseudo:
    return emitBuildPairF64Pseudo(MI, BB);
  case RISCV::SplitF64Pseudo:
    return emitSplitF64Pseudo(MI, BB);
  case RISCV::PseudoQuietFLE_H:
    return emitQuietFCMP(MI, BB, RISCV::FLE_H, RISCV::FEQ_H, Subtarget);
  case RISCV::PseudoQuietFLT_H:
    return emitQuietFCMP(MI, BB, RISCV::FLT_H, RISCV::FEQ_H, Subtarget);
  case RISCV::PseudoQuietFLE_S:
    return emitQuietFCMP(MI, BB, RISCV::FLE_S, RISCV::FEQ_S, Subtarget);
  case RISCV::PseudoQuietFLT_S:
    return emitQuietFCMP(MI, BB, RISCV::FLT_S, RISCV::FEQ_S, Subtarget);
  case RISCV::PseudoQuietFLE_D:
    return emitQuietFCMP(MI, BB, RISCV::FLE_D, RISCV::FEQ_D, Subtarget);
  case RISCV::PseudoQuietFLT_D:
    return emitQuietFCMP(MI, BB, RISCV::FLT_D, RISCV::FEQ_D, Subtarget);
  case RISCV::PseudoFROUND_H:
  case RISCV::PseudoFROUND_S:
  case RISCV::PseudoFROUND_D:
    return emitFROUND(MI, BB, Subtarget);
  }
}

void RISCVTargetLowering::AdjustInstrPostInstrSelection(MachineInstr &MI,
                                                        SDNode *Node) const {
  // Add FRM dependency to any instructions with dynamic rounding mode.
  unsigned Opc = MI.getOpcode();
  auto Idx = RISCV::getNamedOperandIdx(Opc, RISCV::OpName::frm);
  if (Idx < 0)
    return;
  if (MI.getOperand(Idx).getImm() != RISCVFPRndMode::DYN)
    return;
  // If the instruction already reads FRM, don't add another read.
  if (MI.readsRegister(RISCV::FRM))
    return;
  MI.addOperand(
      MachineOperand::CreateReg(RISCV::FRM, /*isDef*/ false, /*isImp*/ true));
}

// Calling Convention Implementation.
// The expectations for frontend ABI lowering vary from target to target.
// Ideally, an LLVM frontend would be able to avoid worrying about many ABI
// details, but this is a longer term goal. For now, we simply try to keep the
// role of the frontend as simple and well-defined as possible. The rules can
// be summarised as:
// * Never split up large scalar arguments. We handle them here.
// * If a hardfloat calling convention is being used, and the struct may be
// passed in a pair of registers (fp+fp, int+fp), and both registers are
// available, then pass as two separate arguments. If either the GPRs or FPRs
// are exhausted, then pass according to the rule below.
// * If a struct could never be passed in registers or directly in a stack
// slot (as it is larger than 2*XLEN and the floating point rules don't
// apply), then pass it using a pointer with the byval attribute.
// * If a struct is less than 2*XLEN, then coerce to either a two-element
// word-sized array or a 2*XLEN scalar (depending on alignment).
// * The frontend can determine whether a struct is returned by reference or
// not based on its size and fields. If it will be returned by reference, the
// frontend must modify the prototype so a pointer with the sret annotation is
// passed as the first argument. This is not necessary for large scalar
// returns.
// * Struct return values and varargs should be coerced to structs containing
// register-size fields in the same situations they would be for fixed
// arguments.

static const MCPhysReg ArgGPRs[] = {
  RISCV::X10, RISCV::X11, RISCV::X12, RISCV::X13,
  RISCV::X14, RISCV::X15, RISCV::X16, RISCV::X17
};
static const MCPhysReg ArgFPR16s[] = {
  RISCV::F10_H, RISCV::F11_H, RISCV::F12_H, RISCV::F13_H,
  RISCV::F14_H, RISCV::F15_H, RISCV::F16_H, RISCV::F17_H
};
static const MCPhysReg ArgFPR32s[] = {
  RISCV::F10_F, RISCV::F11_F, RISCV::F12_F, RISCV::F13_F,
  RISCV::F14_F, RISCV::F15_F, RISCV::F16_F, RISCV::F17_F
};
static const MCPhysReg ArgFPR64s[] = {
  RISCV::F10_D, RISCV::F11_D, RISCV::F12_D, RISCV::F13_D,
  RISCV::F14_D, RISCV::F15_D, RISCV::F16_D, RISCV::F17_D
};

// Calling convention for Ventus GPGPU
static const MCPhysReg ArgVGPRs[] = {
  RISCV::V2, RISCV::V3, RISCV::V4, RISCV::V5, RISCV::V6, RISCV::V7,
};

// Pass a 2*XLEN argument that has been split into two XLEN values through
// registers or the stack as necessary.
static bool CC_RISCVAssign2XLen(unsigned XLen, CCState &State, CCValAssign VA1,
                                ISD::ArgFlagsTy ArgFlags1, unsigned ValNo2,
                                MVT ValVT2, MVT LocVT2,
                                ISD::ArgFlagsTy ArgFlags2) {
  unsigned XLenInBytes = XLen / 8;
  if (Register Reg = State.AllocateReg(ArgGPRs)) {
    // At least one half can be passed via register.
    State.addLoc(CCValAssign::getReg(VA1.getValNo(), VA1.getValVT(), Reg,
                                     VA1.getLocVT(), CCValAssign::Full));
  } else {
    // Both halves must be passed on the stack, with proper alignment.
    Align StackAlign =
        std::max(Align(XLenInBytes), ArgFlags1.getNonZeroOrigAlign());
    State.addLoc(
        CCValAssign::getMem(VA1.getValNo(), VA1.getValVT(),
                            State.AllocateStack(XLenInBytes, StackAlign),
                            VA1.getLocVT(), CCValAssign::Full));
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
    return false;
  }

  if (Register Reg = State.AllocateReg(ArgGPRs)) {
    // The second half can also be passed via register.
    State.addLoc(
        CCValAssign::getReg(ValNo2, ValVT2, Reg, LocVT2, CCValAssign::Full));
  } else {
    // The second half is passed via the stack, without additional alignment.
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
  }

  return false;
}

static unsigned allocateRVVReg(MVT ValVT, unsigned ValNo,
                               std::optional<unsigned> FirstMaskArgument,
                               CCState &State, const RISCVTargetLowering &TLI) {
  // Assign the first mask argument to V0.
  // This is an interim calling convention and it may be changed in the
  // future.
  if (FirstMaskArgument && ValNo == *FirstMaskArgument)
    return State.AllocateReg(RISCV::V0);

  // For Ventus GPGPU, the only argument regs are ArgVGPRs
  return State.AllocateReg(ArgVGPRs);
}

// Implements the Ventus RISC-V calling convention. Returns true upon failure.
static bool CC_Ventus(const DataLayout &DL, RISCVABI::ABI ABI, unsigned ValNo,
                      MVT ValVT, MVT LocVT, CCValAssign::LocInfo LocInfo,
                      ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsFixed,
                      bool IsRet, Type *OrigTy, const RISCVTargetLowering &TLI,
                      std::optional<unsigned> FirstMaskArgument) {
  assert(IsFixed && "TODO: Add varidic argument lowering!");

  unsigned XLen = DL.getLargestLegalIntTypeSizeInBits();

  // Allocate to a register if possible, or else a stack slot.
  Register Reg;
  unsigned StoreSizeBytes = XLen / 8;
  Align StackAlign = Align(XLen / 8);

  // All arguments are passed in vector registers or via stack for non-kernel
  // function.
  Reg = allocateRVVReg(ValVT, ValNo, FirstMaskArgument, State, TLI);
  if (!Reg) {
    // For return values, the vector must be passed fully via registers or
    // via the stack.
    // FIXME: The Ventus ABI only use V2 for return values, but we're
    // not using it yet.
    if (IsRet)
      return true;

    // Pass vector on stack.
    LocVT = ValVT;
    StoreSizeBytes = ValVT.getStoreSize();
    // Align vectors to their element sizes, being careful for vXi1
    // vectors.
    StackAlign = MaybeAlign(ValVT.getScalarSizeInBits() / 8).valueOrOne();
    unsigned StackOffset = State.AllocateStack(StoreSizeBytes, StackAlign);

    State.AllocateStack(StoreSizeBytes, StackAlign);
    State.addLoc(CCValAssign::getMem(ValNo, ValVT, StackOffset, LocVT, LocInfo));
  } else {
    State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
  }

  return false;
}

template <typename ArgTy>
static std::optional<unsigned> preAssignMask(const ArgTy &Args) {
  for (const auto &ArgIdx : enumerate(Args)) {
    MVT ArgVT = ArgIdx.value().VT;
    if (ArgVT.isVector() && ArgVT.getVectorElementType() == MVT::i1)
      return ArgIdx.index();
  }
  return std::nullopt;
}

void RISCVTargetLowering::analyzeInputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
    RISCVCCAssignFn Fn) const {
  unsigned NumArgs = Ins.size();
  FunctionType *FType = MF.getFunction().getFunctionType();

  std::optional<unsigned> FirstMaskArgument;
  if (Subtarget.hasVInstructions())
    FirstMaskArgument = preAssignMask(Ins);

  for (unsigned i = 0; i != NumArgs; ++i) {
    MVT ArgVT = Ins[i].VT;
    ISD::ArgFlagsTy ArgFlags = Ins[i].Flags;

    Type *ArgTy = nullptr;
    if (IsRet)
      ArgTy = FType->getReturnType();
    else if (Ins[i].isOrigArg())
      ArgTy = FType->getParamType(Ins[i].getOrigArgIndex());

    RISCVABI::ABI ABI = MF.getSubtarget<RISCVSubtarget>().getTargetABI();
    if (Fn(MF.getDataLayout(), ABI, i, ArgVT, ArgVT, CCValAssign::Full,
           ArgFlags, CCInfo, /*IsFixed=*/true, IsRet, ArgTy, *this,
           FirstMaskArgument)) {
      LLVM_DEBUG(dbgs() << "InputArg #" << i << " has unhandled type "
                        << EVT(ArgVT).getEVTString() << '\n');
      llvm_unreachable(nullptr);
    }
  }
}

void RISCVTargetLowering::analyzeOutputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::OutputArg> &Outs, bool IsRet,
    CallLoweringInfo *CLI, RISCVCCAssignFn Fn) const {
  unsigned NumArgs = Outs.size();

  std::optional<unsigned> FirstMaskArgument;
  if (Subtarget.hasVInstructions())
    FirstMaskArgument = preAssignMask(Outs);

  for (unsigned i = 0; i != NumArgs; i++) {
    MVT ArgVT = Outs[i].VT;
    ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
    Type *OrigTy = CLI ? CLI->getArgs()[Outs[i].OrigArgIndex].Ty : nullptr;

    RISCVABI::ABI ABI = MF.getSubtarget<RISCVSubtarget>().getTargetABI();
    if (Fn(MF.getDataLayout(), ABI, i, ArgVT, ArgVT, CCValAssign::Full,
           ArgFlags, CCInfo, Outs[i].IsFixed, IsRet, OrigTy, *this,
           FirstMaskArgument)) {
      LLVM_DEBUG(dbgs() << "OutputArg #" << i << " has unhandled type "
                        << EVT(ArgVT).getEVTString() << "\n");
      llvm_unreachable(nullptr);
    }
  }
}

void RISCVTargetLowering::analyzeFormalArgumentsCompute(MachineFunction &MF,
    CCState &State,
    const SmallVectorImpl<ISD::InputArg> &Ins) const {
  const Function &Fn = MF.getFunction();
  LLVMContext &Ctx = Fn.getParent()->getContext();
  CallingConv::ID CC = Fn.getCallingConv();

  uint64_t ExplicitArgOffset = 0;
  const DataLayout &DL = Fn.getParent()->getDataLayout();

  unsigned InIndex = 0;

  for (const Argument &Arg : Fn.args()) {
    const bool IsByRef = Arg.hasByRefAttr();
    Type *BaseArgTy = Arg.getType();
    Type *MemArgTy = IsByRef ? Arg.getParamByRefType() : BaseArgTy;
    Align Alignment = DL.getValueOrABITypeAlignment(
        IsByRef ? Arg.getParamAlign() : std::nullopt, MemArgTy);
    uint64_t AllocSize = DL.getTypeAllocSize(MemArgTy);

    uint64_t ArgOffset = alignTo(ExplicitArgOffset, Alignment) + ExplicitArgOffset;
    ExplicitArgOffset = alignTo(ExplicitArgOffset, Alignment) + AllocSize;

    SmallVector<EVT, 16> ValueVTs;
    SmallVector<uint64_t, 16> Offsets;
    ComputeValueVTs(*this, DL, BaseArgTy, ValueVTs, &Offsets, ArgOffset);

    for (unsigned Value = 0, NumValues = ValueVTs.size();
         Value != NumValues; ++Value) {
      uint64_t BasePartOffset = Offsets[Value];

      EVT ArgVT = ValueVTs[Value];
      EVT MemVT = ArgVT;
      MVT RegisterVT = getRegisterTypeForCallingConv(Ctx, CC, ArgVT);
      unsigned NumRegs = getNumRegistersForCallingConv(Ctx, CC, ArgVT);

      if (NumRegs == 1) {
        // The argument is not split, so the IR type is the memory type.
        if (ArgVT.isExtended()) {
          // We have an extended type, like i24, so we should just use
          // the register type.
          MemVT = RegisterVT;
        } else {
          MemVT = ArgVT;
        }
      } else if (ArgVT.isVector() && RegisterVT.isVector() &&
                 ArgVT.getScalarType() == RegisterVT.getScalarType()) {
        assert(ArgVT.getVectorNumElements() > RegisterVT.getVectorNumElements());
        // We have a vector value which has been split into a vector with
        // the same scalar type, but fewer elements. This should handle
        // all the floating-point vector types.
        MemVT = RegisterVT;
      } else if (ArgVT.isVector() &&
                 ArgVT.getVectorNumElements() == NumRegs) {
        // This arg has been split so that each element is stored in a separate
        // register.
        MemVT = ArgVT.getScalarType();
      } else if (ArgVT.isExtended()) {
        // We have an extended type, like i65.
        MemVT = RegisterVT;
      } else {
        unsigned MemoryBits = ArgVT.getStoreSizeInBits() / NumRegs;
        assert(ArgVT.getStoreSizeInBits() % NumRegs == 0);
        if (RegisterVT.isInteger()) {
          MemVT = EVT::getIntegerVT(State.getContext(), MemoryBits);
        } else if (RegisterVT.isVector()) {
          assert(!RegisterVT.getScalarType().isFloatingPoint());
          unsigned NumElements = RegisterVT.getVectorNumElements();
          assert(MemoryBits % NumElements == 0);
          // This vector type has been split into another vector type with
          // a different elements size.
          EVT ScalarVT = EVT::getIntegerVT(State.getContext(),
                                           MemoryBits / NumElements);
          MemVT = EVT::getVectorVT(State.getContext(), ScalarVT, NumElements);
        } else {
          llvm_unreachable("Cannot deduce memory type.");
        }
      }

      // Convert on element vectors to scalar.
      if (MemVT.isVector() && MemVT.getVectorNumElements() == 1)
        MemVT = MemVT.getScalarType();

      // Round up vec3/vec5 argument
      if (MemVT.isVector() && !MemVT.isPow2VectorType()) {
        MemVT = MemVT.getPow2VectorType(State.getContext());
      } else if (!MemVT.isSimple() && !MemVT.isVector()) {
        MemVT = MemVT.getRoundIntegerType(State.getContext());
      }

      unsigned PartOffset = 0;
      for (unsigned i = 0; i != NumRegs; ++i) {
        State.addLoc(CCValAssign::getCustomMem(InIndex++, RegisterVT,
                                               BasePartOffset + PartOffset,
                                               MemVT.getSimpleVT(),
                                               CCValAssign::Full));
        PartOffset += MemVT.getStoreSize();
      }
    }
  }
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromRegLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL,
                                const ISD::InputArg &In,
                                const RISCVTargetLowering &TLI) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  EVT LocVT = VA.getLocVT();
  SDValue Val;
  const TargetRegisterClass *RC = TLI.getRegClassFor(LocVT.getSimpleVT());
  Register VReg = RegInfo.createVirtualRegister(RC);
  RegInfo.addLiveIn(VA.getLocReg(), VReg);
  Val = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);

  // If input is sign extended from 32 bits, note it for the SExtWRemoval pass.
  if (In.isOrigArg()) {
    Argument *OrigArg = MF.getFunction().getArg(In.getOrigArgIndex());
    if (OrigArg->getType()->isIntegerTy()) {
      unsigned BitWidth = OrigArg->getType()->getIntegerBitWidth();
      // An input zero extended from i31 can also be considered sign extended.
      if ((BitWidth <= 32 && In.Flags.isSExt()) ||
          (BitWidth < 32 && In.Flags.isZExt())) {
        RISCVMachineFunctionInfo *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
        RVFI->addSExt32Register(VReg);
      }
    }
  }

  if (VA.getLocInfo() == CCValAssign::Indirect)
    return Val;

  assert(0 && "TODO");
}


// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromMemLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  EVT LocVT = VA.getLocVT();
  EVT ValVT = VA.getValVT();
  EVT PtrVT = MVT::getIntegerVT(DAG.getDataLayout().getPointerSizeInBits(0));
  if (ValVT.isScalableVector()) {
    // When the value is a scalable vector, we save the pointer which points to
    // the scalable vector value in the stack. The ValVT will be the pointer
    // type, instead of the scalable vector type.
    ValVT = LocVT;
  }
  int FI = MFI.CreateFixedObject(ValVT.getStoreSize(), VA.getLocMemOffset(),
                                 /*IsImmutable=*/true);
  SDValue FIN = DAG.getFrameIndex(FI, PtrVT);
  SDValue Val;

  ISD::LoadExtType ExtType;
  switch (VA.getLocInfo()) {
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  case CCValAssign::Full:
  case CCValAssign::Indirect:
  case CCValAssign::BCvt:
    ExtType = ISD::NON_EXTLOAD;
    break;
  }
  Val = DAG.getExtLoad(
      ExtType, DL, LocVT, Chain, FIN,
      MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI), ValVT);
  return Val;
}

static SDValue unpackF64OnRV32DSoftABI(SelectionDAG &DAG, SDValue Chain,
                                       const CCValAssign &VA, const SDLoc &DL) {
  assert(VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64 &&
         "Unexpected VA");
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  if (VA.isMemLoc()) {
    // f64 is passed on the stack.
    int FI =
        MFI.CreateFixedObject(8, VA.getLocMemOffset(), /*IsImmutable=*/true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
    return DAG.getLoad(MVT::f64, DL, Chain, FIN,
                       MachinePointerInfo::getFixedStack(MF, FI));
  }

  assert(VA.isRegLoc() && "Expected register VA assignment");

  Register LoVReg = RegInfo.createVirtualRegister(&RISCV::GPRRegClass);
  RegInfo.addLiveIn(VA.getLocReg(), LoVReg);
  SDValue Lo = DAG.getCopyFromReg(Chain, DL, LoVReg, MVT::i32);
  SDValue Hi;
  if (VA.getLocReg() == RISCV::X17) {
    // Second half of f64 is passed on the stack.
    int FI = MFI.CreateFixedObject(4, 0, /*IsImmutable=*/true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
    Hi = DAG.getLoad(MVT::i32, DL, Chain, FIN,
                     MachinePointerInfo::getFixedStack(MF, FI));
  } else {
    // Second half of f64 is passed in another GPR.
    Register HiVReg = RegInfo.createVirtualRegister(&RISCV::GPRRegClass);
    RegInfo.addLiveIn(VA.getLocReg() + 1, HiVReg);
    Hi = DAG.getCopyFromReg(Chain, DL, HiVReg, MVT::i32);
  }
  return DAG.getNode(RISCVISD::BuildPairF64, DL, MVT::f64, Lo, Hi);
}

static bool CC_RISCV_GHC(unsigned ValNo, MVT ValVT, MVT LocVT,
                         CCValAssign::LocInfo LocInfo,
                         ISD::ArgFlagsTy ArgFlags, CCState &State) {

  if (ArgFlags.isNest()) {
    report_fatal_error(
        "Attribute 'nest' is not supported in GHC calling convention");
  }

  if (LocVT == MVT::i32 || LocVT == MVT::i64) {
    // Pass in STG registers: Base, Sp, Hp, R1, R2, R3, R4, R5, R6, R7, SpLim
    //                        s1    s2  s3  s4  s5  s6  s7  s8  s9  s10 s11
    static const MCPhysReg GPRList[] = {
        RISCV::X9, RISCV::X18, RISCV::X19, RISCV::X20, RISCV::X21, RISCV::X22,
        RISCV::X23, RISCV::X24, RISCV::X25, RISCV::X26, RISCV::X27};
    if (unsigned Reg = State.AllocateReg(GPRList)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f32) {
    // Pass in STG registers: F1, ..., F6
    //                        fs0 ... fs5
    static const MCPhysReg FPR32List[] = {RISCV::F8_F, RISCV::F9_F,
                                          RISCV::F18_F, RISCV::F19_F,
                                          RISCV::F20_F, RISCV::F21_F};
    if (unsigned Reg = State.AllocateReg(FPR32List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f64) {
    // Pass in STG registers: D1, ..., D6
    //                        fs6 ... fs11
    static const MCPhysReg FPR64List[] = {RISCV::F22_D, RISCV::F23_D,
                                          RISCV::F24_D, RISCV::F25_D,
                                          RISCV::F26_D, RISCV::F27_D};
    if (unsigned Reg = State.AllocateReg(FPR64List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  report_fatal_error("No registers left in GHC calling convention");
  return true;
}

// Transform physical registers into virtual registers.
SDValue RISCVTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();

  switch (CallConv) {
  default:
    report_fatal_error("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
  // FIXME: Use AMDGPU_KERNEL IR as test input temporarily
  case CallingConv::AMDGPU_KERNEL:
  case CallingConv::SPIR_KERNEL:
    break;
  case CallingConv::GHC:
    if (!MF.getSubtarget().getFeatureBits()[RISCV::FeatureStdExtF] ||
        !MF.getSubtarget().getFeatureBits()[RISCV::FeatureStdExtD])
      report_fatal_error(
        "GHC calling convention requires the F and D instruction set extensions");
  }

  const Function &Func = MF.getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    if (!Func.arg_empty())
      report_fatal_error(
        "Functions with the interrupt attribute cannot have arguments!");

    StringRef Kind =
      MF.getFunction().getFnAttribute("interrupt").getValueAsString();

    if (!(Kind == "user" || Kind == "supervisor" || Kind == "machine"))
      report_fatal_error(
        "Function interrupt attribute argument not supported!");
  }

  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();
  unsigned XLenInBytes = Subtarget.getXLen() / 8;
  // Used with vargs to acumulate store chains.
  std::vector<SDValue> OutChains;

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  if (CallConv == CallingConv::GHC)
    CCInfo.AnalyzeFormalArguments(Ins, CC_RISCV_GHC);
  else if (CallConv == CallingConv::AMDGPU_KERNEL ||
           CallConv == CallingConv::SPIR_KERNEL)
    analyzeFormalArgumentsCompute(MF, CCInfo, Ins);
  else
    analyzeInputArgs(MF, CCInfo, Ins, /*IsRet=*/false, CC_Ventus);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue;
    // Passing f64 on RV32D with a soft float ABI must be handled as a special
    // case.
    if (VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64)
      ArgValue = unpackF64OnRV32DSoftABI(DAG, Chain, VA, DL);
    else if (VA.isRegLoc())
      ArgValue = unpackFromRegLoc(DAG, Chain, VA, DL, Ins[i], *this);
    else
      ArgValue = unpackFromMemLoc(DAG, Chain, VA, DL);

    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // If the original argument was split and passed by reference (e.g. i128
      // on RV32), we need to load all parts of it here (using the same
      // address). Vectors may be partly split to registers and partly to the
      // stack, in which case the base address is partly offset and subsequent
      // stores are relative to that.
      InVals.push_back(DAG.getLoad(VA.getValVT(), DL, Chain, ArgValue,
                                   MachinePointerInfo()));
      unsigned ArgIndex = Ins[i].OrigArgIndex;
      unsigned ArgPartOffset = Ins[i].PartOffset;
      assert(VA.getValVT().isVector() || ArgPartOffset == 0);
      while (i + 1 != e && Ins[i + 1].OrigArgIndex == ArgIndex) {
        CCValAssign &PartVA = ArgLocs[i + 1];
        unsigned PartOffset = Ins[i + 1].PartOffset - ArgPartOffset;
        SDValue Offset = DAG.getIntPtrConstant(PartOffset, DL);
        if (PartVA.getValVT().isScalableVector())
          Offset = DAG.getNode(ISD::VSCALE, DL, XLenVT, Offset);
        SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, ArgValue, Offset);
        InVals.push_back(DAG.getLoad(PartVA.getValVT(), DL, Chain, Address,
                                     MachinePointerInfo()));
        ++i;
      }
      continue;
    }
    InVals.push_back(ArgValue);
  }

  if (IsVarArg) {
    ArrayRef<MCPhysReg> ArgRegs = makeArrayRef(ArgGPRs);
    unsigned Idx = CCInfo.getFirstUnallocated(ArgRegs);
    const TargetRegisterClass *RC = &RISCV::GPRRegClass;
    MachineFrameInfo &MFI = MF.getFrameInfo();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();
    RISCVMachineFunctionInfo *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

    // Offset of the first variable argument from stack pointer, and size of
    // the vararg save area. For now, the varargs save area is either zero or
    // large enough to hold a0-a7.
    int VaArgOffset, VarArgsSaveSize;

    // If all registers are allocated, then all varargs must be passed on the
    // stack and we don't need to save any argregs.
    if (ArgRegs.size() == Idx) {
      VaArgOffset = CCInfo.getNextStackOffset();
      VarArgsSaveSize = 0;
    } else {
      VarArgsSaveSize = XLenInBytes * (ArgRegs.size() - Idx);
      VaArgOffset = -VarArgsSaveSize;
    }

    // Record the frame index of the first variable argument
    // which is a value necessary to VASTART.
    int FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
    RVFI->setVarArgsFrameIndex(FI);

    // If saving an odd number of registers then create an extra stack slot to
    // ensure that the frame pointer is 2*XLEN-aligned, which in turn ensures
    // offsets to even-numbered registered remain 2*XLEN-aligned.
    if (Idx % 2) {
      MFI.CreateFixedObject(XLenInBytes, VaArgOffset - (int)XLenInBytes, true);
      VarArgsSaveSize += XLenInBytes;
    }

    // Copy the integer registers that may have been used for passing varargs
    // to the vararg save area.
    for (unsigned I = Idx; I < ArgRegs.size();
         ++I, VaArgOffset += XLenInBytes) {
      const Register Reg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(ArgRegs[I], Reg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, XLenVT);
      FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
      SDValue PtrOff = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue Store = DAG.getStore(Chain, DL, ArgValue, PtrOff,
                                   MachinePointerInfo::getFixedStack(MF, FI));
      cast<StoreSDNode>(Store.getNode())
          ->getMemOperand()
          ->setValue((Value *)nullptr);
      OutChains.push_back(Store);
    }
    RVFI->setVarArgsSaveSize(VarArgsSaveSize);
  }

  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens for vararg functions.
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
  }

  return Chain;
}

/// isEligibleForTailCallOptimization - Check whether the call is eligible
/// for tail call optimization.
/// Note: This is modelled after ARM's IsEligibleForTailCallOptimization.
bool RISCVTargetLowering::isEligibleForTailCallOptimization(
    CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
    const SmallVector<CCValAssign, 16> &ArgLocs) const {

  auto &Callee = CLI.Callee;
  auto CalleeCC = CLI.CallConv;
  auto &Outs = CLI.Outs;
  auto &Caller = MF.getFunction();
  auto CallerCC = Caller.getCallingConv();

  // Exception-handling functions need a special set of instructions to
  // indicate a return to the hardware. Tail-calling another function would
  // probably break this.
  // TODO: The "interrupt" attribute isn't currently defined by RISC-V. This
  // should be expanded as new function attributes are introduced.
  if (Caller.hasFnAttribute("interrupt"))
    return false;

  // Do not tail call opt if the stack is used to pass parameters.
  if (CCInfo.getNextStackOffset() != 0)
    return false;

  // Do not tail call opt if any parameters need to be passed indirectly.
  // Since long doubles (fp128) and i128 are larger than 2*XLEN, they are
  // passed indirectly. So the address of the value will be passed in a
  // register, or if not available, then the address is put on the stack. In
  // order to pass indirectly, space on the stack often needs to be allocated
  // in order to store the value. In this case the CCInfo.getNextStackOffset()
  // != 0 check is not enough and we need to check if any CCValAssign ArgsLocs
  // are passed CCValAssign::Indirect.
  for (auto &VA : ArgLocs)
    if (VA.getLocInfo() == CCValAssign::Indirect)
      return false;

  // Do not tail call opt if either caller or callee uses struct return
  // semantics.
  auto IsCallerStructRet = Caller.hasStructRetAttr();
  auto IsCalleeStructRet = Outs.empty() ? false : Outs[0].Flags.isSRet();
  if (IsCallerStructRet || IsCalleeStructRet)
    return false;

  // Externally-defined functions with weak linkage should not be
  // tail-called. The behaviour of branch instructions in this situation (as
  // used for tail calls) is implementation-defined, so we cannot rely on the
  // linker replacing the tail call with a return.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = G->getGlobal();
    if (GV->hasExternalWeakLinkage())
      return false;
  }

  // The callee has to preserve all registers the caller needs to preserve.
  const RISCVRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *CallerPreserved = TRI->getCallPreservedMask(MF, CallerCC);
  if (CalleeCC != CallerCC) {
    const uint32_t *CalleePreserved = TRI->getCallPreservedMask(MF, CalleeCC);
    if (!TRI->regmaskSubsetEqual(CallerPreserved, CalleePreserved))
      return false;
  }

  // Byval parameters hand the function a pointer directly into the stack area
  // we want to reuse during a tail call. Working around this *is* possible
  // but less efficient and uglier in LowerCall.
  for (auto &Arg : Outs)
    if (Arg.Flags.isByVal())
      return false;

  return true;
}

static Align getPrefTypeAlign(EVT VT, SelectionDAG &DAG) {
  return DAG.getDataLayout().getPrefTypeAlign(
      VT.getTypeForEVT(*DAG.getContext()));
}

// Lower a call to a callseq_start + CALL + callseq_end chain, and add input
// and output parameter nodes.
SDValue RISCVTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();

  MachineFunction &MF = DAG.getMachineFunction();

  // Analyze the operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState ArgCCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  if (CallConv == CallingConv::GHC)
    ArgCCInfo.AnalyzeCallOperands(Outs, CC_RISCV_GHC);
  else
    analyzeOutputArgs(MF, ArgCCInfo, Outs, /*IsRet=*/false, &CLI,
                      CC_Ventus);

  // Check if it's really possible to do a tail call.
  if (IsTailCall)
    IsTailCall = isEligibleForTailCallOptimization(ArgCCInfo, CLI, MF, ArgLocs);

  if (IsTailCall)
    ++NumTailCalls;
  else if (CLI.CB && CLI.CB->isMustTailCall())
    report_fatal_error("failed to perform tail call elimination on a call "
                       "site marked musttail");

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = ArgCCInfo.getNextStackOffset();

  // Create local copies for byval args
  SmallVector<SDValue, 8> ByValArgs;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (!Flags.isByVal())
      continue;

    SDValue Arg = OutVals[i];
    unsigned Size = Flags.getByValSize();
    Align Alignment = Flags.getNonZeroByValAlign();

    int FI =
        MF.getFrameInfo().CreateStackObject(Size, Alignment, /*isSS=*/false);
    SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
    SDValue SizeNode = DAG.getConstant(Size, DL, XLenVT);

    Chain = DAG.getMemcpy(Chain, DL, FIPtr, Arg, SizeNode, Alignment,
                          /*IsVolatile=*/false,
                          /*AlwaysInline=*/false, IsTailCall,
                          MachinePointerInfo(), MachinePointerInfo());
    ByValArgs.push_back(FIPtr);
  }

  if (!IsTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, CLI.DL);

  // Copy argument values to their designated locations.
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;
  SDValue StackPtr;
  for (unsigned i = 0, j = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue = OutVals[i];
    ISD::ArgFlagsTy Flags = Outs[i].Flags;

    // Handle passing f64 on RV32D with a soft float ABI as a special case.
    bool IsF64OnRV32DSoftABI =
        VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64;
    if (IsF64OnRV32DSoftABI && VA.isRegLoc()) {
      SDValue SplitF64 = DAG.getNode(
          RISCVISD::SplitF64, DL, DAG.getVTList(MVT::i32, MVT::i32), ArgValue);
      SDValue Lo = SplitF64.getValue(0);
      SDValue Hi = SplitF64.getValue(1);

      Register RegLo = VA.getLocReg();
      RegsToPass.push_back(std::make_pair(RegLo, Lo));

      if (RegLo == RISCV::X17) {
        // Second half of f64 is passed on the stack.
        // Work out the address of the stack slot.
        if (!StackPtr.getNode())
          StackPtr = DAG.getCopyFromReg(Chain, DL, RISCV::X2, PtrVT);
        // Emit the store.
        MemOpChains.push_back(
            DAG.getStore(Chain, DL, Hi, StackPtr, MachinePointerInfo()));
      } else {
        // Second half of f64 is passed in another GPR.
        assert(RegLo < RISCV::X31 && "Invalid register pair");
        Register RegHigh = RegLo + 1;
        RegsToPass.push_back(std::make_pair(RegHigh, Hi));
      }
      continue;
    }

    // IsF64OnRV32DSoftABI && VA.isMemLoc() is handled below in the same way
    // as any other MemLoc.

    // Promote the value if needed.
    // For now, only handle fully promoted and indirect arguments.
    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // Store the argument in a stack slot and pass its address.
      Align StackAlign =
          std::max(getPrefTypeAlign(Outs[i].ArgVT, DAG),
                   getPrefTypeAlign(ArgValue.getValueType(), DAG));
      TypeSize StoredSize = ArgValue.getValueType().getStoreSize();
      // If the original argument was split (e.g. i128), we need
      // to store the required parts of it here (and pass just one address).
      // Vectors may be partly split to registers and partly to the stack, in
      // which case the base address is partly offset and subsequent stores are
      // relative to that.
      unsigned ArgIndex = Outs[i].OrigArgIndex;
      unsigned ArgPartOffset = Outs[i].PartOffset;
      assert(VA.getValVT().isVector() || ArgPartOffset == 0);
      // Calculate the total size to store. We don't have access to what we're
      // actually storing other than performing the loop and collecting the
      // info.
      SmallVector<std::pair<SDValue, SDValue>> Parts;
      while (i + 1 != e && Outs[i + 1].OrigArgIndex == ArgIndex) {
        SDValue PartValue = OutVals[i + 1];
        unsigned PartOffset = Outs[i + 1].PartOffset - ArgPartOffset;
        SDValue Offset = DAG.getIntPtrConstant(PartOffset, DL);
        EVT PartVT = PartValue.getValueType();
        if (PartVT.isScalableVector())
          Offset = DAG.getNode(ISD::VSCALE, DL, XLenVT, Offset);
        StoredSize += PartVT.getStoreSize();
        StackAlign = std::max(StackAlign, getPrefTypeAlign(PartVT, DAG));
        Parts.push_back(std::make_pair(PartValue, Offset));
        ++i;
      }
      SDValue SpillSlot = DAG.CreateStackTemporary(StoredSize, StackAlign);
      int FI = cast<FrameIndexSDNode>(SpillSlot)->getIndex();
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, SpillSlot,
                       MachinePointerInfo::getFixedStack(MF, FI)));
      for (const auto &Part : Parts) {
        SDValue PartValue = Part.first;
        SDValue PartOffset = Part.second;
        SDValue Address =
            DAG.getNode(ISD::ADD, DL, PtrVT, SpillSlot, PartOffset);
        MemOpChains.push_back(
            DAG.getStore(Chain, DL, PartValue, Address,
                         MachinePointerInfo::getFixedStack(MF, FI)));
      }
      ArgValue = SpillSlot;
    } else {
      assert(0 && "TODO");
      // ArgValue = convertValVTToLocVT(DAG, ArgValue, VA, DL, Subtarget);
    }

    // Use local copy if it is a byval arg.
    if (Flags.isByVal())
      ArgValue = ByValArgs[j++];

    if (VA.isRegLoc()) {
      // Queue up the argument copies and emit them at the end.
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), ArgValue));
    } else {
      assert(VA.isMemLoc() && "Argument not register or memory");
      assert(!IsTailCall && "Tail call not allowed if stack is used "
                            "for passing parameters");

      // Work out the address of the stack slot.
      if (!StackPtr.getNode())
        StackPtr = DAG.getCopyFromReg(Chain, DL, RISCV::X2, PtrVT);
      SDValue Address =
          DAG.getNode(ISD::ADD, DL, PtrVT, StackPtr,
                      DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

      // Emit the store.
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, Address, MachinePointerInfo()));
    }
  }

  // Join the stores, which are independent of one another.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;

  // Build a sequence of copy-to-reg nodes, chained and glued together.
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
    Glue = Chain.getValue(1);
  }

  // Validate that none of the argument registers have been marked as
  // reserved, if so report an error. Do the same for the return address if this
  // is not a tailcall.
  validateCCReservedRegs(RegsToPass, MF);
  if (!IsTailCall &&
      MF.getSubtarget<RISCVSubtarget>().isRegisterReservedByUser(RISCV::X1))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Return address register required, but has been reserved."});

  // If the callee is a GlobalAddress/ExternalSymbol node, turn it into a
  // TargetGlobalAddress/TargetExternalSymbol node so that legalize won't
  // split it and then direct call can be matched by PseudoCALL.
  if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = S->getGlobal();

    unsigned OpFlags = RISCVII::MO_CALL;
    if (!getTargetMachine().shouldAssumeDSOLocal(*GV->getParent(), GV))
      OpFlags = RISCVII::MO_PLT;

    Callee = DAG.getTargetGlobalAddress(GV, DL, PtrVT, 0, OpFlags);
  } else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    unsigned OpFlags = RISCVII::MO_CALL;

    if (!getTargetMachine().shouldAssumeDSOLocal(*MF.getFunction().getParent(),
                                                 nullptr))
      OpFlags = RISCVII::MO_PLT;

    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), PtrVT, OpFlags);
  }

  // The first call operand is the chain and the second is the target address.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

  if (!IsTailCall) {
    // Add a register mask operand representing the call-preserved registers.
    const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
    const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
    assert(Mask && "Missing call preserved mask for calling convention");
    Ops.push_back(DAG.getRegisterMask(Mask));
  }

  // Glue the call to the argument copies, if any.
  if (Glue.getNode())
    Ops.push_back(Glue);

  // Emit the call.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);

  if (IsTailCall) {
    MF.getFrameInfo().setHasTailCall();
    return DAG.getNode(RISCVISD::TAIL, DL, NodeTys, Ops);
  }

  Chain = DAG.getNode(RISCVISD::CALL, DL, NodeTys, Ops);
  DAG.addNoMergeSiteInfo(Chain.getNode(), CLI.NoMerge);
  Glue = Chain.getValue(1);

  // Mark the end of the call, which is glued to the call itself.
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  // TODO: Use CC_Ventus conditionally
  analyzeInputArgs(MF, RetCCInfo, Ins, /*IsRet=*/true, CC_Ventus);

  // Copy all of the result registers out of their specified physreg.
  for (auto &VA : RVLocs) {
    // Copy the value out
    SDValue RetValue =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    // Glue the RetValue to the end of the call sequence
    Chain = RetValue.getValue(1);
    Glue = RetValue.getValue(2);

    if (VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64) {
      assert(VA.getLocReg() == ArgGPRs[0] && "Unexpected reg assignment");
      SDValue RetValue2 =
          DAG.getCopyFromReg(Chain, DL, ArgGPRs[1], MVT::i32, Glue);
      Chain = RetValue2.getValue(1);
      Glue = RetValue2.getValue(2);
      RetValue = DAG.getNode(RISCVISD::BuildPairF64, DL, MVT::f64, RetValue,
                             RetValue2);
    }

    assert(0 && "TODO!");
    //RetValue = convertLocVTToValVT(DAG, RetValue, VA, DL, Subtarget);

    InVals.push_back(RetValue);
  }

  return Chain;
}

bool RISCVTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  std::optional<unsigned> FirstMaskArgument;
  if (Subtarget.hasVInstructions())
    FirstMaskArgument = preAssignMask(Outs);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    MVT VT = Outs[i].VT;
    ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
    RISCVABI::ABI ABI = MF.getSubtarget<RISCVSubtarget>().getTargetABI();
    // TODO: Use CC_Ventus conditionally
    if (CC_Ventus(MF.getDataLayout(), ABI, i, VT, VT, CCValAssign::Full,
                 ArgFlags, CCInfo, /*IsFixed=*/true, /*IsRet=*/true, nullptr,
                 *this, FirstMaskArgument))
      return false;
  }
  return true;
}

SDValue
RISCVTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  const MachineFunction &MF = DAG.getMachineFunction();
  const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();

  // Stores the assignment of the return value to a location.
  SmallVector<CCValAssign, 16> RVLocs;

  // Info about the registers and stack slot.
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  // TODO: Use CC_Ventus conditional
  analyzeOutputArgs(DAG.getMachineFunction(), CCInfo, Outs, /*IsRet=*/true,
                    nullptr, CC_Ventus);

  if (CallConv == CallingConv::GHC && !RVLocs.empty())
    report_fatal_error("GHC functions return void only");

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (unsigned i = 0, e = RVLocs.size(); i < e; ++i) {
    SDValue Val = OutVals[i];
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    if (VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64) {
      // Handle returning f64 on RV32D with a soft float ABI.
      assert(VA.isRegLoc() && "Expected return via registers");
      SDValue SplitF64 = DAG.getNode(RISCVISD::SplitF64, DL,
                                     DAG.getVTList(MVT::i32, MVT::i32), Val);
      SDValue Lo = SplitF64.getValue(0);
      SDValue Hi = SplitF64.getValue(1);
      Register RegLo = VA.getLocReg();
      assert(RegLo < RISCV::X31 && "Invalid register pair");
      Register RegHi = RegLo + 1;

      if (STI.isRegisterReservedByUser(RegLo) ||
          STI.isRegisterReservedByUser(RegHi))
        MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
            MF.getFunction(),
            "Return value register required, but has been reserved."});

      Chain = DAG.getCopyToReg(Chain, DL, RegLo, Lo, Glue);
      Glue = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(RegLo, MVT::i32));
      Chain = DAG.getCopyToReg(Chain, DL, RegHi, Hi, Glue);
      Glue = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(RegHi, MVT::i32));
    } else {
      // Handle a 'normal' return.
      assert(0 && "TODO");
      /*
      Val = convertValVTToLocVT(DAG, Val, VA, DL, Subtarget);
      Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);

      if (STI.isRegisterReservedByUser(VA.getLocReg()))
        MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
            MF.getFunction(),
            "Return value register required, but has been reserved."});

      // Guarantee that all emitted copies are stuck together.
      Glue = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
      */
    }
  }

  RetOps[0] = Chain; // Update chain.

  // Add the glue node if we have it.
  if (Glue.getNode()) {
    RetOps.push_back(Glue);
  }

  unsigned RetOpc = RISCVISD::RET_FLAG;
  // Interrupt service routines use different return instructions.
  const Function &Func = DAG.getMachineFunction().getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    if (!Func.getReturnType()->isVoidTy())
      report_fatal_error(
          "Functions with the interrupt attribute must have void return type!");

    MachineFunction &MF = DAG.getMachineFunction();
    StringRef Kind =
      MF.getFunction().getFnAttribute("interrupt").getValueAsString();

    if (Kind == "user")
      RetOpc = RISCVISD::URET_FLAG;
    else if (Kind == "supervisor")
      RetOpc = RISCVISD::SRET_FLAG;
    else
      RetOpc = RISCVISD::MRET_FLAG;
  }

  return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
}

void RISCVTargetLowering::validateCCReservedRegs(
    const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
    MachineFunction &MF) const {
  const Function &F = MF.getFunction();
  const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();

  if (llvm::any_of(Regs, [&STI](auto Reg) {
        return STI.isRegisterReservedByUser(Reg.first);
      }))
    F.getContext().diagnose(DiagnosticInfoUnsupported{
        F, "Argument register required, but has been reserved."});
}

// Check if the result of the node is only used as a return value, as
// otherwise we can't perform a tail-call.
bool RISCVTargetLowering::isUsedByReturnOnly(SDNode *N, SDValue &Chain) const {
  if (N->getNumValues() != 1)
    return false;
  if (!N->hasNUsesOfValue(1, 0))
    return false;

  SDNode *Copy = *N->use_begin();
  // TODO: Handle additional opcodes in order to support tail-calling libcalls
  // with soft float ABIs.
  if (Copy->getOpcode() != ISD::CopyToReg) {
    return false;
  }

  // If the ISD::CopyToReg has a glue operand, we conservatively assume it
  // isn't safe to perform a tail call.
  if (Copy->getOperand(Copy->getNumOperands() - 1).getValueType() == MVT::Glue)
    return false;

  // The copy must be used by a RISCVISD::RET_FLAG, and nothing else.
  bool HasRet = false;
  for (SDNode *Node : Copy->uses()) {
    if (Node->getOpcode() != RISCVISD::RET_FLAG)
      return false;
    HasRet = true;
  }
  if (!HasRet)
    return false;

  Chain = Copy->getOperand(0);
  return true;
}

bool RISCVTargetLowering::mayBeEmittedAsTailCall(const CallInst *CI) const {
  return CI->isTailCall();
}

const char *RISCVTargetLowering::getTargetNodeName(unsigned Opcode) const {
#define NODE_NAME_CASE(NODE)                                                   \
  case RISCVISD::NODE:                                                         \
    return "RISCVISD::" #NODE;
  // clang-format off
  switch ((RISCVISD::NodeType)Opcode) {
  case RISCVISD::FIRST_NUMBER:
    break;
  NODE_NAME_CASE(RET_FLAG)
  NODE_NAME_CASE(URET_FLAG)
  NODE_NAME_CASE(SRET_FLAG)
  NODE_NAME_CASE(MRET_FLAG)
  NODE_NAME_CASE(CALL)
  NODE_NAME_CASE(SELECT_CC)
  NODE_NAME_CASE(BR_CC)
  NODE_NAME_CASE(BuildPairF64)
  NODE_NAME_CASE(SplitF64)
  NODE_NAME_CASE(TAIL)
  NODE_NAME_CASE(ADD_LO)
  NODE_NAME_CASE(HI)
  NODE_NAME_CASE(LLA)
  NODE_NAME_CASE(ADD_TPREL)
  NODE_NAME_CASE(LA)
  NODE_NAME_CASE(LA_TLS_IE)
  NODE_NAME_CASE(LA_TLS_GD)
  NODE_NAME_CASE(MULHSU)
  NODE_NAME_CASE(SLLW)
  NODE_NAME_CASE(SRAW)
  NODE_NAME_CASE(SRLW)
  NODE_NAME_CASE(DIVW)
  NODE_NAME_CASE(DIVUW)
  NODE_NAME_CASE(REMUW)
  NODE_NAME_CASE(ROLW)
  NODE_NAME_CASE(RORW)
  NODE_NAME_CASE(CLZW)
  NODE_NAME_CASE(CTZW)
  NODE_NAME_CASE(ABSW)
  NODE_NAME_CASE(FMV_H_X)
  NODE_NAME_CASE(FMV_X_ANYEXTH)
  NODE_NAME_CASE(FMV_X_SIGNEXTH)
  NODE_NAME_CASE(FMV_W_X_RV64)
  NODE_NAME_CASE(FMV_X_ANYEXTW_RV64)
  NODE_NAME_CASE(FCVT_X)
  NODE_NAME_CASE(FCVT_XU)
  NODE_NAME_CASE(FCVT_W_RV64)
  NODE_NAME_CASE(FCVT_WU_RV64)
  NODE_NAME_CASE(STRICT_FCVT_W_RV64)
  NODE_NAME_CASE(STRICT_FCVT_WU_RV64)
  NODE_NAME_CASE(FROUND)
  NODE_NAME_CASE(READ_CYCLE_WIDE)
  NODE_NAME_CASE(BREV8)
  NODE_NAME_CASE(ORC_B)
  NODE_NAME_CASE(ZIP)
  NODE_NAME_CASE(UNZIP)
  NODE_NAME_CASE(VMV_V_X_VL)
  NODE_NAME_CASE(VFMV_V_F_VL)
  NODE_NAME_CASE(VMV_X_S)
  NODE_NAME_CASE(VMV_S_X_VL)
  NODE_NAME_CASE(VFMV_S_F_VL)
  NODE_NAME_CASE(SPLAT_VECTOR_SPLIT_I64_VL)
  NODE_NAME_CASE(READ_VLENB)
  NODE_NAME_CASE(TRUNCATE_VECTOR_VL)
  NODE_NAME_CASE(VSLIDEUP_VL)
  NODE_NAME_CASE(VSLIDE1UP_VL)
  NODE_NAME_CASE(VSLIDEDOWN_VL)
  NODE_NAME_CASE(VSLIDE1DOWN_VL)
  NODE_NAME_CASE(VID_VL)
  NODE_NAME_CASE(VFNCVT_ROD_VL)
  NODE_NAME_CASE(VECREDUCE_ADD_VL)
  NODE_NAME_CASE(VECREDUCE_UMAX_VL)
  NODE_NAME_CASE(VECREDUCE_SMAX_VL)
  NODE_NAME_CASE(VECREDUCE_UMIN_VL)
  NODE_NAME_CASE(VECREDUCE_SMIN_VL)
  NODE_NAME_CASE(VECREDUCE_AND_VL)
  NODE_NAME_CASE(VECREDUCE_OR_VL)
  NODE_NAME_CASE(VECREDUCE_XOR_VL)
  NODE_NAME_CASE(VECREDUCE_FADD_VL)
  NODE_NAME_CASE(VECREDUCE_SEQ_FADD_VL)
  NODE_NAME_CASE(VECREDUCE_FMIN_VL)
  NODE_NAME_CASE(VECREDUCE_FMAX_VL)
  NODE_NAME_CASE(ADD_VL)
  NODE_NAME_CASE(AND_VL)
  NODE_NAME_CASE(MUL_VL)
  NODE_NAME_CASE(OR_VL)
  NODE_NAME_CASE(SDIV_VL)
  NODE_NAME_CASE(SHL_VL)
  NODE_NAME_CASE(SREM_VL)
  NODE_NAME_CASE(SRA_VL)
  NODE_NAME_CASE(SRL_VL)
  NODE_NAME_CASE(SUB_VL)
  NODE_NAME_CASE(UDIV_VL)
  NODE_NAME_CASE(UREM_VL)
  NODE_NAME_CASE(XOR_VL)
  NODE_NAME_CASE(SADDSAT_VL)
  NODE_NAME_CASE(UADDSAT_VL)
  NODE_NAME_CASE(SSUBSAT_VL)
  NODE_NAME_CASE(USUBSAT_VL)
  NODE_NAME_CASE(FADD_VL)
  NODE_NAME_CASE(FSUB_VL)
  NODE_NAME_CASE(FMUL_VL)
  NODE_NAME_CASE(FDIV_VL)
  NODE_NAME_CASE(FNEG_VL)
  NODE_NAME_CASE(FABS_VL)
  NODE_NAME_CASE(FSQRT_VL)
  NODE_NAME_CASE(VFMADD_VL)
  NODE_NAME_CASE(VFNMADD_VL)
  NODE_NAME_CASE(VFMSUB_VL)
  NODE_NAME_CASE(VFNMSUB_VL)
  NODE_NAME_CASE(FCOPYSIGN_VL)
  NODE_NAME_CASE(SMIN_VL)
  NODE_NAME_CASE(SMAX_VL)
  NODE_NAME_CASE(UMIN_VL)
  NODE_NAME_CASE(UMAX_VL)
  NODE_NAME_CASE(FMINNUM_VL)
  NODE_NAME_CASE(FMAXNUM_VL)
  NODE_NAME_CASE(MULHS_VL)
  NODE_NAME_CASE(MULHU_VL)
  NODE_NAME_CASE(VFCVT_RTZ_X_F_VL)
  NODE_NAME_CASE(VFCVT_RTZ_XU_F_VL)
  NODE_NAME_CASE(VFCVT_RM_X_F_VL)
  NODE_NAME_CASE(VFCVT_X_F_VL)
  NODE_NAME_CASE(VFROUND_NOEXCEPT_VL)
  NODE_NAME_CASE(SINT_TO_FP_VL)
  NODE_NAME_CASE(UINT_TO_FP_VL)
  NODE_NAME_CASE(FP_EXTEND_VL)
  NODE_NAME_CASE(FP_ROUND_VL)
  NODE_NAME_CASE(VWMUL_VL)
  NODE_NAME_CASE(VWMULU_VL)
  NODE_NAME_CASE(VWMULSU_VL)
  NODE_NAME_CASE(VWADD_VL)
  NODE_NAME_CASE(VWADDU_VL)
  NODE_NAME_CASE(VWSUB_VL)
  NODE_NAME_CASE(VWSUBU_VL)
  NODE_NAME_CASE(VWADD_W_VL)
  NODE_NAME_CASE(VWADDU_W_VL)
  NODE_NAME_CASE(VWSUB_W_VL)
  NODE_NAME_CASE(VWSUBU_W_VL)
  NODE_NAME_CASE(VNSRL_VL)
  NODE_NAME_CASE(SETCC_VL)
  NODE_NAME_CASE(VSELECT_VL)
  NODE_NAME_CASE(VP_MERGE_VL)
  NODE_NAME_CASE(VMAND_VL)
  NODE_NAME_CASE(VMOR_VL)
  NODE_NAME_CASE(VMXOR_VL)
  NODE_NAME_CASE(VMCLR_VL)
  NODE_NAME_CASE(VMSET_VL)
  NODE_NAME_CASE(VRGATHER_VX_VL)
  NODE_NAME_CASE(VRGATHER_VV_VL)
  NODE_NAME_CASE(VRGATHEREI16_VV_VL)
  NODE_NAME_CASE(VSEXT_VL)
  NODE_NAME_CASE(VZEXT_VL)
  NODE_NAME_CASE(VCPOP_VL)
  NODE_NAME_CASE(READ_CSR)
  NODE_NAME_CASE(WRITE_CSR)
  NODE_NAME_CASE(SWAP_CSR)
  }
  // clang-format on
  return nullptr;
#undef NODE_NAME_CASE
}

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
RISCVTargetLowering::ConstraintType
RISCVTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'f':
      return C_RegisterClass;
    case 'I':
    case 'J':
    case 'K':
      return C_Immediate;
    case 'A':
      return C_Memory;
    case 'S': // A symbolic address
      return C_Other;
    }
  } else {
    if (Constraint == "vr" || Constraint == "vm")
      return C_RegisterClass;
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
RISCVTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  // First, see if this is a constraint that directly corresponds to a
  // RISCV register class.
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      // TODO: Support fixed vectors up to XLen for P extension?
      if (VT.isVector())
        break;
      return std::make_pair(0U, &RISCV::GPRRegClass);
    case 'f':
      if (Subtarget.hasStdExtZfh() && VT == MVT::f16)
        return std::make_pair(0U, &RISCV::FPR16RegClass);
      if (Subtarget.hasStdExtF() && VT == MVT::f32)
        return std::make_pair(0U, &RISCV::FPR32RegClass);
      if (Subtarget.hasStdExtD() && VT == MVT::f64)
        return std::make_pair(0U, &RISCV::FPR64RegClass);
      break;
    default:
      break;
    }
  } else if (Constraint == "vr") {
    for (const auto *RC : {&RISCV::VGPRRegClass}) {
      if (TRI->isTypeLegalForClass(*RC, VT.SimpleTy))
        return std::make_pair(0U, RC);
    }
  }

  // Clang will correctly decode the usage of register name aliases into their
  // official names. However, other frontends like `rustc` do not. This allows
  // users of these frontends to use the ABI names for registers in LLVM-style
  // register constraints.
  unsigned XRegFromAlias = StringSwitch<unsigned>(Constraint.lower())
                               .Case("{zero}", RISCV::X0)
                               .Case("{ra}", RISCV::X1)
                               .Case("{sp}", RISCV::X2)
                               .Case("{gp}", RISCV::X3)
                               .Case("{tp}", RISCV::X4)
                               .Case("{t0}", RISCV::X5)
                               .Case("{t1}", RISCV::X6)
                               .Case("{t2}", RISCV::X7)
                               .Cases("{s0}", "{fp}", RISCV::X8)
                               .Case("{s1}", RISCV::X9)
                               .Case("{a0}", RISCV::X10)
                               .Case("{a1}", RISCV::X11)
                               .Case("{a2}", RISCV::X12)
                               .Case("{a3}", RISCV::X13)
                               .Case("{a4}", RISCV::X14)
                               .Case("{a5}", RISCV::X15)
                               .Case("{a6}", RISCV::X16)
                               .Case("{a7}", RISCV::X17)
                               .Case("{s2}", RISCV::X18)
                               .Case("{s3}", RISCV::X19)
                               .Case("{s4}", RISCV::X20)
                               .Case("{s5}", RISCV::X21)
                               .Case("{s6}", RISCV::X22)
                               .Case("{s7}", RISCV::X23)
                               .Case("{s8}", RISCV::X24)
                               .Case("{s9}", RISCV::X25)
                               .Case("{s10}", RISCV::X26)
                               .Case("{s11}", RISCV::X27)
                               .Case("{t3}", RISCV::X28)
                               .Case("{t4}", RISCV::X29)
                               .Case("{t5}", RISCV::X30)
                               .Case("{t6}", RISCV::X31)
                               .Default(RISCV::NoRegister);
  if (XRegFromAlias != RISCV::NoRegister)
    return std::make_pair(XRegFromAlias, &RISCV::GPRRegClass);

  // Since TargetLowering::getRegForInlineAsmConstraint uses the name of the
  // TableGen record rather than the AsmName to choose registers for InlineAsm
  // constraints, plus we want to match those names to the widest floating point
  // register type available, manually select floating point registers here.
  //
  // The second case is the ABI name of the register, so that frontends can also
  // use the ABI names in register constraint lists.
  if (Subtarget.hasStdExtF()) {
    unsigned FReg = StringSwitch<unsigned>(Constraint.lower())
                        .Cases("{f0}", "{ft0}", RISCV::F0_F)
                        .Cases("{f1}", "{ft1}", RISCV::F1_F)
                        .Cases("{f2}", "{ft2}", RISCV::F2_F)
                        .Cases("{f3}", "{ft3}", RISCV::F3_F)
                        .Cases("{f4}", "{ft4}", RISCV::F4_F)
                        .Cases("{f5}", "{ft5}", RISCV::F5_F)
                        .Cases("{f6}", "{ft6}", RISCV::F6_F)
                        .Cases("{f7}", "{ft7}", RISCV::F7_F)
                        .Cases("{f8}", "{fs0}", RISCV::F8_F)
                        .Cases("{f9}", "{fs1}", RISCV::F9_F)
                        .Cases("{f10}", "{fa0}", RISCV::F10_F)
                        .Cases("{f11}", "{fa1}", RISCV::F11_F)
                        .Cases("{f12}", "{fa2}", RISCV::F12_F)
                        .Cases("{f13}", "{fa3}", RISCV::F13_F)
                        .Cases("{f14}", "{fa4}", RISCV::F14_F)
                        .Cases("{f15}", "{fa5}", RISCV::F15_F)
                        .Cases("{f16}", "{fa6}", RISCV::F16_F)
                        .Cases("{f17}", "{fa7}", RISCV::F17_F)
                        .Cases("{f18}", "{fs2}", RISCV::F18_F)
                        .Cases("{f19}", "{fs3}", RISCV::F19_F)
                        .Cases("{f20}", "{fs4}", RISCV::F20_F)
                        .Cases("{f21}", "{fs5}", RISCV::F21_F)
                        .Cases("{f22}", "{fs6}", RISCV::F22_F)
                        .Cases("{f23}", "{fs7}", RISCV::F23_F)
                        .Cases("{f24}", "{fs8}", RISCV::F24_F)
                        .Cases("{f25}", "{fs9}", RISCV::F25_F)
                        .Cases("{f26}", "{fs10}", RISCV::F26_F)
                        .Cases("{f27}", "{fs11}", RISCV::F27_F)
                        .Cases("{f28}", "{ft8}", RISCV::F28_F)
                        .Cases("{f29}", "{ft9}", RISCV::F29_F)
                        .Cases("{f30}", "{ft10}", RISCV::F30_F)
                        .Cases("{f31}", "{ft11}", RISCV::F31_F)
                        .Default(RISCV::NoRegister);
    if (FReg != RISCV::NoRegister) {
      assert(RISCV::F0_F <= FReg && FReg <= RISCV::F31_F && "Unknown fp-reg");
      if (Subtarget.hasStdExtD() && (VT == MVT::f64 || VT == MVT::Other)) {
        unsigned RegNo = FReg - RISCV::F0_F;
        unsigned DReg = RISCV::F0_D + RegNo;
        return std::make_pair(DReg, &RISCV::FPR64RegClass);
      }
      if (VT == MVT::f32 || VT == MVT::Other)
        return std::make_pair(FReg, &RISCV::FPR32RegClass);
      if (Subtarget.hasStdExtZfh() && VT == MVT::f16) {
        unsigned RegNo = FReg - RISCV::F0_F;
        unsigned HReg = RISCV::F0_H + RegNo;
        return std::make_pair(HReg, &RISCV::FPR16RegClass);
      }
    }
  }

  if (Subtarget.hasVInstructions()) {
    Register VReg = StringSwitch<Register>(Constraint.lower())
                        .Case("{v0}", RISCV::V0)
                        .Case("{v1}", RISCV::V1)
                        .Case("{v2}", RISCV::V2)
                        .Case("{v3}", RISCV::V3)
                        .Case("{v4}", RISCV::V4)
                        .Case("{v5}", RISCV::V5)
                        .Case("{v6}", RISCV::V6)
                        .Case("{v7}", RISCV::V7)
                        .Case("{v8}", RISCV::V8)
                        .Case("{v9}", RISCV::V9)
                        .Case("{v10}", RISCV::V10)
                        .Case("{v11}", RISCV::V11)
                        .Case("{v12}", RISCV::V12)
                        .Case("{v13}", RISCV::V13)
                        .Case("{v14}", RISCV::V14)
                        .Case("{v15}", RISCV::V15)
                        .Case("{v16}", RISCV::V16)
                        .Case("{v17}", RISCV::V17)
                        .Case("{v18}", RISCV::V18)
                        .Case("{v19}", RISCV::V19)
                        .Case("{v20}", RISCV::V20)
                        .Case("{v21}", RISCV::V21)
                        .Case("{v22}", RISCV::V22)
                        .Case("{v23}", RISCV::V23)
                        .Case("{v24}", RISCV::V24)
                        .Case("{v25}", RISCV::V25)
                        .Case("{v26}", RISCV::V26)
                        .Case("{v27}", RISCV::V27)
                        .Case("{v28}", RISCV::V28)
                        .Case("{v29}", RISCV::V29)
                        .Case("{v30}", RISCV::V30)
                        .Case("{v31}", RISCV::V31)
                        .Default(RISCV::NoRegister);
    if (VReg != RISCV::NoRegister) {
      if (TRI->isTypeLegalForClass(RISCV::VGPRRegClass, VT.SimpleTy))
        return std::make_pair(VReg, &RISCV::VGPRRegClass);
    }
  }

  std::pair<Register, const TargetRegisterClass *> Res =
      TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);

  // If we picked one of the Zfinx register classes, remap it to the GPR class.
  // FIXME: When Zfinx is supported in CodeGen this will need to take the
  // Subtarget into account.
  if (Res.second == &RISCV::GPRF16RegClass ||
      Res.second == &RISCV::GPRF32RegClass ||
      Res.second == &RISCV::GPRF64RegClass)
    return std::make_pair(Res.first, &RISCV::GPRRegClass);

  return Res;
}

unsigned
RISCVTargetLowering::getInlineAsmMemConstraint(StringRef ConstraintCode) const {
  // Currently only support length 1 constraints.
  if (ConstraintCode.size() == 1) {
    switch (ConstraintCode[0]) {
    case 'A':
      return InlineAsm::Constraint_A;
    default:
      break;
    }
  }

  return TargetLowering::getInlineAsmMemConstraint(ConstraintCode);
}

void RISCVTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, std::string &Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  // Currently only support length 1 constraints.
  if (Constraint.length() == 1) {
    switch (Constraint[0]) {
    case 'I':
      // Validate & create a 12-bit signed immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getSExtValue();
        if (isInt<12>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, SDLoc(Op), Subtarget.getXLenVT()));
      }
      return;
    case 'J':
      // Validate & create an integer zero operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op))
        if (C->getZExtValue() == 0)
          Ops.push_back(
              DAG.getTargetConstant(0, SDLoc(Op), Subtarget.getXLenVT()));
      return;
    case 'K':
      // Validate & create a 5-bit unsigned immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getZExtValue();
        if (isUInt<5>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, SDLoc(Op), Subtarget.getXLenVT()));
      }
      return;
    case 'S':
      if (const auto *GA = dyn_cast<GlobalAddressSDNode>(Op)) {
        Ops.push_back(DAG.getTargetGlobalAddress(GA->getGlobal(), SDLoc(Op),
                                                 GA->getValueType(0)));
      } else if (const auto *BA = dyn_cast<BlockAddressSDNode>(Op)) {
        Ops.push_back(DAG.getTargetBlockAddress(BA->getBlockAddress(),
                                                BA->getValueType(0)));
      }
      return;
    default:
      break;
    }
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

Instruction *RISCVTargetLowering::emitLeadingFence(IRBuilderBase &Builder,
                                                   Instruction *Inst,
                                                   AtomicOrdering Ord) const {
  if (isa<LoadInst>(Inst) && Ord == AtomicOrdering::SequentiallyConsistent)
    return Builder.CreateFence(Ord);
  if (isa<StoreInst>(Inst) && isReleaseOrStronger(Ord))
    return Builder.CreateFence(AtomicOrdering::Release);
  return nullptr;
}

Instruction *RISCVTargetLowering::emitTrailingFence(IRBuilderBase &Builder,
                                                    Instruction *Inst,
                                                    AtomicOrdering Ord) const {
  if (isa<LoadInst>(Inst) && isAcquireOrStronger(Ord))
    return Builder.CreateFence(AtomicOrdering::Acquire);
  return nullptr;
}

TargetLowering::AtomicExpansionKind
RISCVTargetLowering::shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const {
  // atomicrmw {fadd,fsub} must be expanded to use compare-exchange, as floating
  // point operations can't be used in an lr/sc sequence without breaking the
  // forward-progress guarantee.
  if (AI->isFloatingPointOperation())
    return AtomicExpansionKind::CmpXChg;

  // Don't expand forced atomics, we want to have __sync libcalls instead.
  if (Subtarget.hasForcedAtomics())
    return AtomicExpansionKind::None;

  unsigned Size = AI->getType()->getPrimitiveSizeInBits();
  if (Size == 8 || Size == 16)
    return AtomicExpansionKind::MaskedIntrinsic;
  return AtomicExpansionKind::None;
}

static Intrinsic::ID
getIntrinsicForMaskedAtomicRMWBinOp(unsigned XLen, AtomicRMWInst::BinOp BinOp) {
  if (XLen == 32) {
    switch (BinOp) {
    default:
      llvm_unreachable("Unexpected AtomicRMW BinOp");
    case AtomicRMWInst::Xchg:
      return Intrinsic::riscv_masked_atomicrmw_xchg_i32;
    case AtomicRMWInst::Add:
      return Intrinsic::riscv_masked_atomicrmw_add_i32;
    case AtomicRMWInst::Sub:
      return Intrinsic::riscv_masked_atomicrmw_sub_i32;
    case AtomicRMWInst::Nand:
      return Intrinsic::riscv_masked_atomicrmw_nand_i32;
    case AtomicRMWInst::Max:
      return Intrinsic::riscv_masked_atomicrmw_max_i32;
    case AtomicRMWInst::Min:
      return Intrinsic::riscv_masked_atomicrmw_min_i32;
    case AtomicRMWInst::UMax:
      return Intrinsic::riscv_masked_atomicrmw_umax_i32;
    case AtomicRMWInst::UMin:
      return Intrinsic::riscv_masked_atomicrmw_umin_i32;
    }
  }

  if (XLen == 64) {
    switch (BinOp) {
    default:
      llvm_unreachable("Unexpected AtomicRMW BinOp");
    case AtomicRMWInst::Xchg:
      return Intrinsic::riscv_masked_atomicrmw_xchg_i64;
    case AtomicRMWInst::Add:
      return Intrinsic::riscv_masked_atomicrmw_add_i64;
    case AtomicRMWInst::Sub:
      return Intrinsic::riscv_masked_atomicrmw_sub_i64;
    case AtomicRMWInst::Nand:
      return Intrinsic::riscv_masked_atomicrmw_nand_i64;
    case AtomicRMWInst::Max:
      return Intrinsic::riscv_masked_atomicrmw_max_i64;
    case AtomicRMWInst::Min:
      return Intrinsic::riscv_masked_atomicrmw_min_i64;
    case AtomicRMWInst::UMax:
      return Intrinsic::riscv_masked_atomicrmw_umax_i64;
    case AtomicRMWInst::UMin:
      return Intrinsic::riscv_masked_atomicrmw_umin_i64;
    }
  }

  llvm_unreachable("Unexpected XLen\n");
}

Value *RISCVTargetLowering::emitMaskedAtomicRMWIntrinsic(
    IRBuilderBase &Builder, AtomicRMWInst *AI, Value *AlignedAddr, Value *Incr,
    Value *Mask, Value *ShiftAmt, AtomicOrdering Ord) const {
  unsigned XLen = Subtarget.getXLen();
  Value *Ordering =
      Builder.getIntN(XLen, static_cast<uint64_t>(AI->getOrdering()));
  Type *Tys[] = {AlignedAddr->getType()};
  Function *LrwOpScwLoop = Intrinsic::getDeclaration(
      AI->getModule(),
      getIntrinsicForMaskedAtomicRMWBinOp(XLen, AI->getOperation()), Tys);

  if (XLen == 64) {
    Incr = Builder.CreateSExt(Incr, Builder.getInt64Ty());
    Mask = Builder.CreateSExt(Mask, Builder.getInt64Ty());
    ShiftAmt = Builder.CreateSExt(ShiftAmt, Builder.getInt64Ty());
  }

  Value *Result;

  // Must pass the shift amount needed to sign extend the loaded value prior
  // to performing a signed comparison for min/max. ShiftAmt is the number of
  // bits to shift the value into position. Pass XLen-ShiftAmt-ValWidth, which
  // is the number of bits to left+right shift the value in order to
  // sign-extend.
  if (AI->getOperation() == AtomicRMWInst::Min ||
      AI->getOperation() == AtomicRMWInst::Max) {
    const DataLayout &DL = AI->getModule()->getDataLayout();
    unsigned ValWidth =
        DL.getTypeStoreSizeInBits(AI->getValOperand()->getType());
    Value *SextShamt =
        Builder.CreateSub(Builder.getIntN(XLen, XLen - ValWidth), ShiftAmt);
    Result = Builder.CreateCall(LrwOpScwLoop,
                                {AlignedAddr, Incr, Mask, SextShamt, Ordering});
  } else {
    Result =
        Builder.CreateCall(LrwOpScwLoop, {AlignedAddr, Incr, Mask, Ordering});
  }

  if (XLen == 64)
    Result = Builder.CreateTrunc(Result, Builder.getInt32Ty());
  return Result;
}

TargetLowering::AtomicExpansionKind
RISCVTargetLowering::shouldExpandAtomicCmpXchgInIR(
    AtomicCmpXchgInst *CI) const {
  // Don't expand forced atomics, we want to have __sync libcalls instead.
  if (Subtarget.hasForcedAtomics())
    return AtomicExpansionKind::None;

  unsigned Size = CI->getCompareOperand()->getType()->getPrimitiveSizeInBits();
  if (Size == 8 || Size == 16)
    return AtomicExpansionKind::MaskedIntrinsic;
  return AtomicExpansionKind::None;
}

Value *RISCVTargetLowering::emitMaskedAtomicCmpXchgIntrinsic(
    IRBuilderBase &Builder, AtomicCmpXchgInst *CI, Value *AlignedAddr,
    Value *CmpVal, Value *NewVal, Value *Mask, AtomicOrdering Ord) const {
  unsigned XLen = Subtarget.getXLen();
  Value *Ordering = Builder.getIntN(XLen, static_cast<uint64_t>(Ord));
  Intrinsic::ID CmpXchgIntrID = Intrinsic::riscv_masked_cmpxchg_i32;
  if (XLen == 64) {
    CmpVal = Builder.CreateSExt(CmpVal, Builder.getInt64Ty());
    NewVal = Builder.CreateSExt(NewVal, Builder.getInt64Ty());
    Mask = Builder.CreateSExt(Mask, Builder.getInt64Ty());
    CmpXchgIntrID = Intrinsic::riscv_masked_cmpxchg_i64;
  }
  Type *Tys[] = {AlignedAddr->getType()};
  Function *MaskedCmpXchg =
      Intrinsic::getDeclaration(CI->getModule(), CmpXchgIntrID, Tys);
  Value *Result = Builder.CreateCall(
      MaskedCmpXchg, {AlignedAddr, CmpVal, NewVal, Mask, Ordering});
  if (XLen == 64)
    Result = Builder.CreateTrunc(Result, Builder.getInt32Ty());
  return Result;
}

bool RISCVTargetLowering::shouldRemoveExtendFromGSIndex(EVT IndexVT,
                                                        EVT DataVT) const {
  return false;
}

bool RISCVTargetLowering::shouldConvertFpToSat(unsigned Op, EVT FPVT,
                                               EVT VT) const {
  if (!isOperationLegalOrCustom(Op, VT) || !FPVT.isSimple())
    return false;

  switch (FPVT.getSimpleVT().SimpleTy) {
  case MVT::f16:
    return Subtarget.hasStdExtZfh();
  case MVT::f32:
    return Subtarget.hasStdExtF();
  case MVT::f64:
    return Subtarget.hasStdExtD();
  default:
    return false;
  }
}

unsigned RISCVTargetLowering::getJumpTableEncoding() const {
  // If we are using the small code model, we can reduce size of jump table
  // entry to 4 bytes.
  if (Subtarget.is64Bit() && !isPositionIndependent() &&
      getTargetMachine().getCodeModel() == CodeModel::Small) {
    return MachineJumpTableInfo::EK_Custom32;
  }
  return TargetLowering::getJumpTableEncoding();
}

const MCExpr *RISCVTargetLowering::LowerCustomJumpTableEntry(
    const MachineJumpTableInfo *MJTI, const MachineBasicBlock *MBB,
    unsigned uid, MCContext &Ctx) const {
  assert(Subtarget.is64Bit() && !isPositionIndependent() &&
         getTargetMachine().getCodeModel() == CodeModel::Small);
  return MCSymbolRefExpr::create(MBB->getSymbol(), Ctx);
}

bool RISCVTargetLowering::isVScaleKnownToBeAPowerOfTwo() const {
  // We define vscale to be VLEN/RVVBitsPerBlock.  VLEN is always a power
  // of two >= 32, and RVVBitsPerBlock is 32.  Thus, vscale must be
  // a power of two as well.
  // FIXME: This doesn't work for zve32, but that's already broken
  // elsewhere for the same reason.
  assert(Subtarget.getRealMinVLen() >= 32 && "zve32* unsupported");
  static_assert(RISCV::RVVBitsPerBlock == 32,
                "RVVBitsPerBlock changed, audit needed");
  return true;
}

bool RISCVTargetLowering::isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                                     EVT VT) const {
  VT = VT.getScalarType();

  if (!VT.isSimple())
    return false;

  switch (VT.getSimpleVT().SimpleTy) {
  case MVT::f16:
    return Subtarget.hasStdExtZfh();
  case MVT::f32:
    return Subtarget.hasStdExtF();
  case MVT::f64:
    return Subtarget.hasStdExtD();
  default:
    break;
  }

  return false;
}

Register RISCVTargetLowering::getExceptionPointerRegister(
    const Constant *PersonalityFn) const {
  return RISCV::X10;
}

Register RISCVTargetLowering::getExceptionSelectorRegister(
    const Constant *PersonalityFn) const {
  return RISCV::X11;
}

bool RISCVTargetLowering::shouldExtendTypeInLibCall(EVT Type) const {
  // Return false to suppress the unnecessary extensions if the LibCall
  // arguments or return value is f32 type for LP64 ABI.
  RISCVABI::ABI ABI = Subtarget.getTargetABI();
  if (ABI == RISCVABI::ABI_LP64 && (Type == MVT::f32))
    return false;

  return true;
}

bool RISCVTargetLowering::shouldSignExtendTypeInLibCall(EVT Type, bool IsSigned) const {
  if (Subtarget.is64Bit() && Type == MVT::i32)
    return true;

  return IsSigned;
}

bool RISCVTargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                                 SDValue C) const {
  // Check integral scalar types.
  const bool HasExtMOrZmmul =
      Subtarget.hasStdExtM() || Subtarget.hasStdExtZmmul();
  if (VT.isScalarInteger()) {
    // Omit the optimization if the sub target has the M extension and the data
    // size exceeds XLen.
    if (HasExtMOrZmmul && VT.getSizeInBits() > Subtarget.getXLen())
      return false;
    if (auto *ConstNode = dyn_cast<ConstantSDNode>(C.getNode())) {
      // Break the MUL to a SLLI and an ADD/SUB.
      const APInt &Imm = ConstNode->getAPIntValue();
      if ((Imm + 1).isPowerOf2() || (Imm - 1).isPowerOf2() ||
          (1 - Imm).isPowerOf2() || (-1 - Imm).isPowerOf2())
        return true;
      // Optimize the MUL to (SH*ADD x, (SLLI x, bits)) if Imm is not simm12.
      if (Subtarget.hasStdExtZba() && !Imm.isSignedIntN(12) &&
          ((Imm - 2).isPowerOf2() || (Imm - 4).isPowerOf2() ||
           (Imm - 8).isPowerOf2()))
        return true;
      // Omit the following optimization if the sub target has the M extension
      // and the data size >= XLen.
      if (HasExtMOrZmmul && VT.getSizeInBits() >= Subtarget.getXLen())
        return false;
      // Break the MUL to two SLLI instructions and an ADD/SUB, if Imm needs
      // a pair of LUI/ADDI.
      if (!Imm.isSignedIntN(12) && Imm.countTrailingZeros() < 12) {
        APInt ImmS = Imm.ashr(Imm.countTrailingZeros());
        if ((ImmS + 1).isPowerOf2() || (ImmS - 1).isPowerOf2() ||
            (1 - ImmS).isPowerOf2())
          return true;
      }
    }
  }

  return false;
}

bool RISCVTargetLowering::isMulAddWithConstProfitable(SDValue AddNode,
                                                      SDValue ConstNode) const {
  // Let the DAGCombiner decide for vectors.
  EVT VT = AddNode.getValueType();
  if (VT.isVector())
    return true;

  // Let the DAGCombiner decide for larger types.
  if (VT.getScalarSizeInBits() > Subtarget.getXLen())
    return true;

  // It is worse if c1 is simm12 while c1*c2 is not.
  ConstantSDNode *C1Node = cast<ConstantSDNode>(AddNode.getOperand(1));
  ConstantSDNode *C2Node = cast<ConstantSDNode>(ConstNode);
  const APInt &C1 = C1Node->getAPIntValue();
  const APInt &C2 = C2Node->getAPIntValue();
  if (C1.isSignedIntN(12) && !(C1 * C2).isSignedIntN(12))
    return false;

  // Default to true and let the DAGCombiner decide.
  return true;
}

bool RISCVTargetLowering::allowsMisalignedMemoryAccesses(
    EVT VT, unsigned AddrSpace, Align Alignment, MachineMemOperand::Flags Flags,
    unsigned *Fast) const {
  if (!VT.isVector()) {
    if (Fast)
      *Fast = 0;
    return Subtarget.enableUnalignedScalarMem();
  }

  // All vector implementations must support element alignment
  EVT ElemVT = VT.getVectorElementType();
  if (Alignment >= ElemVT.getStoreSize()) {
    if (Fast)
      *Fast = 1;
    return true;
  }

  return false;
}

bool RISCVTargetLowering::splitValueIntoRegisterParts(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Val, SDValue *Parts,
    unsigned NumParts, MVT PartVT, std::optional<CallingConv::ID> CC) const {
  bool IsABIRegCopy = CC.has_value();
  EVT ValueVT = Val.getValueType();
  if (IsABIRegCopy && ValueVT == MVT::f16 && PartVT == MVT::f32) {
    // Cast the f16 to i16, extend to i32, pad with ones to make a float nan,
    // and cast to f32.
    Val = DAG.getNode(ISD::BITCAST, DL, MVT::i16, Val);
    Val = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i32, Val);
    Val = DAG.getNode(ISD::OR, DL, MVT::i32, Val,
                      DAG.getConstant(0xFFFF0000, DL, MVT::i32));
    Val = DAG.getNode(ISD::BITCAST, DL, MVT::f32, Val);
    Parts[0] = Val;
    return true;
  }

  if (ValueVT.isScalableVector() && PartVT.isScalableVector()) {
    LLVMContext &Context = *DAG.getContext();
    EVT ValueEltVT = ValueVT.getVectorElementType();
    EVT PartEltVT = PartVT.getVectorElementType();
    unsigned ValueVTBitSize = ValueVT.getSizeInBits().getKnownMinSize();
    unsigned PartVTBitSize = PartVT.getSizeInBits().getKnownMinSize();
    if (PartVTBitSize % ValueVTBitSize == 0) {
      assert(PartVTBitSize >= ValueVTBitSize);
      // If the element types are different, bitcast to the same element type of
      // PartVT first.
      // Give an example here, we want copy a <vscale x 1 x i8> value to
      // <vscale x 4 x i16>.
      // We need to convert <vscale x 1 x i8> to <vscale x 8 x i8> by insert
      // subvector, then we can bitcast to <vscale x 4 x i16>.
      if (ValueEltVT != PartEltVT) {
        if (PartVTBitSize > ValueVTBitSize) {
          unsigned Count = PartVTBitSize / ValueEltVT.getFixedSizeInBits();
          assert(Count != 0 && "The number of element should not be zero.");
          EVT SameEltTypeVT =
              EVT::getVectorVT(Context, ValueEltVT, Count, /*IsScalable=*/true);
          Val = DAG.getNode(ISD::INSERT_SUBVECTOR, DL, SameEltTypeVT,
                            DAG.getUNDEF(SameEltTypeVT), Val,
                            DAG.getVectorIdxConstant(0, DL));
        }
        Val = DAG.getNode(ISD::BITCAST, DL, PartVT, Val);
      } else {
        Val =
            DAG.getNode(ISD::INSERT_SUBVECTOR, DL, PartVT, DAG.getUNDEF(PartVT),
                        Val, DAG.getVectorIdxConstant(0, DL));
      }
      Parts[0] = Val;
      return true;
    }
  }
  return false;
}

SDValue RISCVTargetLowering::joinRegisterPartsIntoValue(
    SelectionDAG &DAG, const SDLoc &DL, const SDValue *Parts, unsigned NumParts,
    MVT PartVT, EVT ValueVT, std::optional<CallingConv::ID> CC) const {
  bool IsABIRegCopy = CC.has_value();
  if (IsABIRegCopy && ValueVT == MVT::f16 && PartVT == MVT::f32) {
    SDValue Val = Parts[0];

    // Cast the f32 to i32, truncate to i16, and cast back to f16.
    Val = DAG.getNode(ISD::BITCAST, DL, MVT::i32, Val);
    Val = DAG.getNode(ISD::TRUNCATE, DL, MVT::i16, Val);
    Val = DAG.getNode(ISD::BITCAST, DL, MVT::f16, Val);
    return Val;
  }

  if (ValueVT.isScalableVector() && PartVT.isScalableVector()) {
    LLVMContext &Context = *DAG.getContext();
    SDValue Val = Parts[0];
    EVT ValueEltVT = ValueVT.getVectorElementType();
    EVT PartEltVT = PartVT.getVectorElementType();
    unsigned ValueVTBitSize = ValueVT.getSizeInBits().getKnownMinSize();
    unsigned PartVTBitSize = PartVT.getSizeInBits().getKnownMinSize();
    if (PartVTBitSize % ValueVTBitSize == 0) {
      assert(PartVTBitSize >= ValueVTBitSize);
      EVT SameEltTypeVT = ValueVT;
      // If the element types are different, convert it to the same element type
      // of PartVT.
      // Give an example here, we want copy a <vscale x 1 x i8> value from
      // <vscale x 4 x i16>.
      // We need to convert <vscale x 4 x i16> to <vscale x 8 x i8> first,
      // then we can extract <vscale x 1 x i8>.
      if (ValueEltVT != PartEltVT) {
        unsigned Count = PartVTBitSize / ValueEltVT.getFixedSizeInBits();
        assert(Count != 0 && "The number of element should not be zero.");
        SameEltTypeVT =
            EVT::getVectorVT(Context, ValueEltVT, Count, /*IsScalable=*/true);
        Val = DAG.getNode(ISD::BITCAST, DL, SameEltTypeVT, Val);
      }
      Val = DAG.getNode(ISD::EXTRACT_SUBVECTOR, DL, ValueVT, Val,
                        DAG.getVectorIdxConstant(0, DL));
      return Val;
    }
  }
  return SDValue();
}

bool RISCVTargetLowering::isIntDivCheap(EVT VT, AttributeList Attr) const {
  // When aggressively optimizing for code size, we prefer to use a div
  // instruction, as it is usually smaller than the alternative sequence.
  // TODO: Add vector division?
  bool OptSize = Attr.hasFnAttr(Attribute::MinSize);
  return OptSize && !VT.isVector();
}

#define GET_REGISTER_MATCHER
#include "RISCVGenAsmMatcher.inc"

Register
RISCVTargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                       const MachineFunction &MF) const {
  Register Reg = MatchRegisterAltName(RegName);
  if (Reg == RISCV::NoRegister)
    Reg = MatchRegisterName(RegName);
  if (Reg == RISCV::NoRegister)
    report_fatal_error(
        Twine("Invalid register name \"" + StringRef(RegName) + "\"."));
  BitVector ReservedRegs = Subtarget.getRegisterInfo()->getReservedRegs(MF);
  if (!ReservedRegs.test(Reg) && !Subtarget.isRegisterReservedByUser(Reg))
    report_fatal_error(Twine("Trying to obtain non-reserved register \"" +
                             StringRef(RegName) + "\"."));
  return Reg;
}

namespace llvm::RISCVVIntrinsicsTable {

#define GET_RISCVVIntrinsicsTable_IMPL
#include "RISCVGenSearchableTables.inc"

} // namespace llvm::RISCVVIntrinsicsTable
