//===--- RISCV.h - Declare RISCV target feature support ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares RISCV TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_RISCV_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_RISCV_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/RISCVISAInfo.h"

namespace clang {
namespace targets {

// RISC-V Target
class RISCVTargetInfo : public TargetInfo {

  enum AddrSpace {
    Generic = 0,
    Global = 1,
    Local = 3,
    Constant = 4,
    Private = 5
  };

  // OpenCL addressing space to target architecture addressing mapping
  static const LangASMap VentusAddrSpaceMap;

protected:
  std::string ABI, CPU;
  std::unique_ptr<llvm::RISCVISAInfo> ISAInfo;
  static const Builtin::Info BuiltinInfo[];

public:
  RISCVTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    LongDoubleWidth = 128;
    LongDoubleAlign = 128;
    LongDoubleFormat = &llvm::APFloat::IEEEquad();
    SuitableAlign = 128;
    WCharType = SignedInt;
    WIntType = UnsignedInt;
    HasRISCVVTypes = true;
    MCountName = "_mcount";
    HasFloat16 = true;

  }

  bool setCPU(const std::string &Name) override {
    if (!isValidCPUName(Name))
      return false;
    CPU = Name;
    return true;
  }

  void adjust(DiagnosticsEngine &Diags, LangOptions &Opts) override;

  StringRef getABI() const override { return ABI; }
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<Builtin::Info> getTargetBuiltins() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  const char *getClobbers() const override { return ""; }

  StringRef getConstraintRegister(StringRef Constraint,
                                  StringRef Expression) const override {
    return Expression;
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  int getEHDataRegisterNumber(unsigned RegNo) const override {
    if (RegNo == 0)
      return 10;
    else if (RegNo == 1)
      return 11;
    else
      return -1;
  }

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  std::string convertConstraint(const char *&Constraint) const override;

  bool
  initFeatureMap(llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags,
                 StringRef CPU,
                 const std::vector<std::string> &FeaturesVec) const override;

  Optional<std::pair<unsigned, unsigned>>
  getVScaleRange(const LangOptions &LangOpts) const override;

  bool hasFeature(StringRef Feature) const override;

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override;

  bool hasBitIntType() const override { return true; }

  bool useFP16ConversionIntrinsics() const override {
    return false;
  }

  bool isValidCPUName(StringRef Name) const override;
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override;
  bool isValidTuneCPUName(StringRef Name) const override;
  void fillValidTuneCPUList(SmallVectorImpl<StringRef> &Values) const override;

  void setSupportedOpenCLOpts() override {
    auto &Opts = getSupportedOpenCLOpts();
    // Opts["cl_khr_fp16"] = true;
    Opts["cl_clang_storage_class_specifiers"] = true;
    Opts["__cl_clang_variadic_functions"] = true;
    Opts["__opencl_c_images"] = true;
    Opts["__opencl_c_3d_image_writes"] = true;
    Opts["cl_khr_3d_image_writes"] = true;
    Opts["cl_khr_byte_addressable_store"] = true;
    Opts["cl_khr_fp64"] = true;
    Opts["__cl_clang_variadic_functions"] = true;
    Opts["cl_khr_global_int32_base_atomics"] = true;
    Opts["cl_khr_global_int32_extended_atomics"] = true;
    Opts["cl_khr_local_int32_base_atomics"] = true;
    Opts["cl_khr_local_int32_extended_atomics"] = true;
  }

  LangAS getOpenCLTypeAddrSpace(OpenCLTypeKind TK) const override {
    switch (TK) {
    case OCLTK_Image:
    case OCLTK_Sampler:
    case OCLTK_Pipe:
    case OCLTK_ClkEvent:
    case OCLTK_Queue:
    case OCLTK_ReserveID:
      return LangAS::opencl_global;

    default:
      return TargetInfo::getOpenCLTypeAddrSpace(TK);
    }
  }


  LangAS getOpenCLBuiltinAddressSpace(unsigned AS) const override {
    switch (AS) {
    case 0:
      return LangAS::opencl_generic;
    case 1:
      return LangAS::opencl_global;
    case 3:
      return LangAS::opencl_local;
    case 4:
      return LangAS::opencl_constant;
    case 5:
      return LangAS::opencl_private;
    default:
      return getLangASFromTargetAS(AS);
    }
  }

  llvm::Optional<LangAS> getConstantAddressSpace() const override {
    return getLangASFromTargetAS(Constant);
  }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    switch (CC) {
    default:
      return CCCR_Warning;
    case CC_C:
    case CC_OpenCLKernel:
      return CCCR_OK;
    }
  }
};
class LLVM_LIBRARY_VISIBILITY RISCV32TargetInfo : public RISCVTargetInfo {
public:
  RISCV32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : RISCVTargetInfo(Triple, Opts) {
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SizeType = UnsignedInt;
    //HasLegalHalfType = true;
    //HasFloat16 = true;
    //resetDataLayout("e-m:e-p:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256"
    //       "-v256:256-v512:512-v1024:1024-n32:64-S128-A5-G1");
  }

  bool setABI(const std::string &Name) override {
    if (Name == "ilp32" || Name == "ilp32f" || Name == "ilp32d") {
      ABI = Name;
      return true;
    }
    return false;
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = 128;

    if (ISAInfo->hasExtension("a"))
      MaxAtomicInlineWidth = 32;
  }
};
class LLVM_LIBRARY_VISIBILITY RISCV64TargetInfo : public RISCVTargetInfo {
public:
  RISCV64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : RISCVTargetInfo(Triple, Opts) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    IntMaxType = Int64Type = SignedLong;
  }

  bool setABI(const std::string &Name) override {
    if (Name == "lp64" || Name == "lp64f" || Name == "lp64d") {
      ABI = Name;
      return true;
    }
    return false;
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = 128;

    if (ISAInfo->hasExtension("a"))
      MaxAtomicInlineWidth = 64;
  }
};
} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_RISCV_H
