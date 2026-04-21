//===-- RISCVAsmPrinter.cpp - RISCV LLVM assembly writer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the RISCV assembly language.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVInstPrinter.h"
#include "MCTargetDesc/RISCVMCExpr.h"
#include "MCTargetDesc/RISCVTargetStreamer.h"
#include "RISCV.h"
#include "RISCVFrameLowering.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVTargetMachine.h"
#include "TargetInfo/RISCVTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation/HWAddressSanitizer.h"
#include <functional>
#include <limits>
#include <optional>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

STATISTIC(RISCVNumInstrsCompressed,
          "Number of RISC-V Compressed instructions emitted");

namespace {
// Binary payload of `.ventus.resource.<kernel>`.
//
// This section is emitted once per entry kernel and is consumed as a plain
// C-style binary struct in field order. The payload is not self-describing
// beyond the leading `Version`, so readers must branch on `Version` before
// decoding the remaining bytes.
//
// Encoding rules:
// - Fields are emitted sequentially with `emitInt32` / `emitInt64`, so the
//   binary byte order follows the target object endianness.
// - There is no implicit trailer, checksum, relocation table, or tagged field
//   directory in the payload itself.
// - All `uint64_t` size fields are raw byte counts, not aligned-up words.
//
// Version 2 layout:
//   uint32_t Version
//   uint32_t Flags
//   uint64_t VGPRUsed
//   uint64_t SGPRUsed
//   uint64_t LDSStaticBytes
//   uint64_t PDSStaticBytes
//   uint64_t LDSStackPeakBytes
//   uint64_t PDSStackPeakBytes
//
// Field semantics:
// - `Version`:
//   ABI version discriminator. Current value is `2`.
// - `Flags`:
//   Bitmask defined by `VentusKernelResourceFlags`.
//   Bit 0 (`VKRF_HasDynamicAlloca`): visible reachable call graph contains a
//     var-sized stack object.
//   Bit 1 (`VKRF_HasRecursion`): visible reachable call graph contains a
//     recursive SCC.
//   Bit 2 (`VKRF_HasIndirectCall`): visible reachable call graph contains an
//     indirect call.
//   Bit 3 (`VKRF_HasUnknownExternalCallee`): visible reachable call graph
//     contains a direct callee whose body is unavailable in the current
//     module.
//   Bit 4 (`VKRF_StackPeakUnavailable`): one or both stack-peak fields are
//     emitted as `UINT64_MAX`.
//   Bit 5 (`VKRF_RegisterUsageIncomplete`): register counts are only the best
//     visible-call-graph lower bound because unresolved callees may use higher
//     physical registers.
// - `VGPRUsed` / `SGPRUsed`:
//   Register counts, not byte sizes. These are the maximum physical register
//   indices observed on the visible reachable call graph plus one.
// - `LDSStaticBytes`:
//   Per-workgroup, compile-time-known fixed local-memory allocation that does
//   not grow with stack depth.
// - `PDSStaticBytes`:
//   Per-thread, compile-time-known fixed private-memory allocation that does
//   not grow with stack depth.
// - `LDSStackPeakBytes`:
//   Per-workgroup peak local-backed stack growth excluding `LDSStaticBytes`.
// - `PDSStackPeakBytes`:
//   Per-thread peak private-backed stack growth excluding `PDSStaticBytes`.
//
// Total peak memory, when the stack peak is known, is therefore:
//   LDS total peak = LDSStaticBytes + LDSStackPeakBytes
//   PDS total peak = PDSStaticBytes + PDSStackPeakBytes

// Binary payload of `.ventus.resobj`.
//
// This is an object-local temporary summary consumed by a future link-time
// finalizer. The backend emits one `.ventus.resobj` section per regular `.o`.
// Unlike `.ventus.resource.<kernel>`, this payload contains function-level
// facts and graph edges, not final aggregated kernel results.
//
// Version 1 layout:
//   ResObjHeader {
//     uint32_t Magic                // 'VRSO'
//     uint32_t Version              // 1
//     uint32_t Flags
//     uint32_t FunctionCount
//     uint32_t EdgeCount
//     uint32_t StaticRefCount
//     uint32_t FunctionTableOffset
//     uint32_t EdgeTableOffset
//     uint32_t StaticRefTableOffset
//   }
//
//   FunctionRecord[FunctionCount] {
//     uint32_t OwnerSymbol
//     uint32_t Flags
//     uint64_t VGPRUsed
//     uint64_t SGPRUsed
//     uint64_t LDSStackSelf
//     uint64_t PDSStackSelf
//     uint64_t OutgoingPrivateMax
//     uint32_t EdgeBegin
//     uint32_t EdgeCount
//     uint32_t StaticRefBegin
//     uint32_t StaticRefCount
//   }
//
//   EdgeRecord[EdgeCount] {
//     uint32_t CalleeSymbol
//     uint32_t Flags
//   }
//
//   StaticRefRecord[StaticRefCount] {
//     uint32_t ResourceSymbol
//     uint32_t Flags
//     uint64_t Size
//   }
//
// Symbol fields are emitted as relocatable 32-bit references because Ventus is
// currently a RISCV32 target.
constexpr uint32_t VentusResObjMagic = 0x5652534fu; // 'VRSO'
constexpr uint32_t VentusResObjVersion = 1u;
constexpr uint32_t VentusResObjSymbolRefSize = 4u;
constexpr uint32_t VentusResObjHeaderSize = 9u * sizeof(uint32_t);
constexpr uint32_t VentusResObjFunctionRecordSize =
    2u * sizeof(uint32_t) + 5u * sizeof(uint64_t) + 4u * sizeof(uint32_t);
constexpr uint32_t VentusResObjEdgeRecordSize = 2u * sizeof(uint32_t);
constexpr uint32_t VentusResObjStaticRefRecordSize =
    2u * sizeof(uint32_t) + sizeof(uint64_t);

enum VentusResObjFunctionFlags : uint32_t {
  VROF_IsEntry = 1u << 0,
  VROF_HasDynamicAlloca = 1u << 1,
  VROF_HasIndirectCall = 1u << 2,
};

enum VentusResObjEdgeFlags : uint32_t {
  VROE_IsTail = 1u << 0,
};

enum VentusResObjStaticRefFlags : uint32_t {
  VROS_IsLDSStatic = 1u << 0,
  VROS_IsPDSStatic = 1u << 1,
};
//
// Unknown values:
// - `LDSStackPeakBytes` / `PDSStackPeakBytes` use `UINT64_MAX` when the peak
//   cannot be determined conservatively at compile time.
// - Static bytes stay as precise known values even when stack peak bytes are
//   unknown.
// - `VGPRUsed` / `SGPRUsed` remain the best visible-call-graph counts. If
//   unresolved callees make them incomplete, `Flags` carries
//   `VKRF_RegisterUsageIncomplete`.
struct VentusKernelResourceV3 {
  uint32_t Version = 3;
  uint32_t Flags = 0;
  uint64_t VGPRUsed = 0;
  uint64_t SGPRUsed = 0;
  uint64_t LDSStaticBytes = 0;
  uint64_t PDSStaticBytes = 0;
  uint64_t LDSStackPeakBytes = 0;
  uint64_t PDSStackPeakBytes = 0;
};

enum VentusKernelResourceFlags : uint32_t {
  // Var-sized stack object exists, so exact stack peak is unavailable.
  VKRF_HasDynamicAlloca = 1u << 0,
  // Recursion exists on the visible call graph, so exact stack peak is
  // unavailable.
  VKRF_HasRecursion = 1u << 1,
  // At least one indirect call exists on the reachable call graph.
  VKRF_HasIndirectCall = 1u << 2,
  // At least one direct callee is unresolved in the current module.
  VKRF_HasUnknownExternalCallee = 1u << 3,
  // `LDSStackPeakBytes` / `PDSStackPeakBytes` are emitted as `UINT64_MAX`.
  VKRF_StackPeakUnavailable = 1u << 4,
  // `VGPRUsed` / `SGPRUsed` are only the best visible-call-graph counts because
  // unresolved callees may use higher-numbered physical registers.
  VKRF_RegisterUsageIncomplete = 1u << 5,
};

struct VentusDirectCallEdge {
  const GlobalValue *TargetGV = nullptr;
  const Function *ResolvedCallee = nullptr;
  std::string TargetName;
  bool IsTail = false;
};

struct VentusFunctionSummary {
  uint64_t VGPRUsed = 0;
  uint64_t SGPRUsed = 0;
  uint64_t LDSStackSelf = 0;
  uint64_t PDSStackSelf = 0;
  uint64_t OutgoingPrivateMax = 0;
  uint32_t LocalFlags = 0;
  SmallVector<const GlobalVariable *, 4> ReferencedStaticGlobals;
  SmallVector<VentusDirectCallEdge, 4> DirectCallEdges;
  SmallVector<const Function *, 4> DirectCallees;
  SmallVector<const Function *, 4> NonTailCallees;
  SmallVector<const Function *, 4> TailCallees;
};

struct VentusPeakSummary {
  uint64_t LDSStackPeakBytes = 0;
  uint64_t PDSStackPeakBytes = 0;
  uint32_t Flags = 0;
  bool Finalized = false;
  bool InProgress = false;
};

struct VentusStaticResourceSummary {
  uint64_t LDSStaticBytes = 0;
  uint64_t PDSStaticBytes = 0;
  uint32_t Flags = 0;
};

struct VentusLocalMemUsage {
  uint64_t StaticBytes = 0;
  uint64_t StackBytes = 0;
};

class RISCVAsmPrinter : public AsmPrinter {
  const MCSubtargetInfo *MCSTI;
  const RISCVSubtarget *STI;
  DenseMap<const Function *, VentusFunctionSummary> VentusSummaries;
  SmallPtrSet<const Function *, 8> VentusEntryFunctions;

public:
  explicit RISCVAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)), MCSTI(TM.getMCSubtargetInfo()) {}

  StringRef getPassName() const override { return "RISCV Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void emitInstruction(const MachineInstr *MI) override;

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;

  void EmitToStreamer(MCStreamer &S, const MCInst &Inst);

  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer,
                                   const MachineInstr *MI);

  typedef std::tuple<unsigned, uint32_t> HwasanMemaccessTuple;
  std::map<HwasanMemaccessTuple, MCSymbol *> HwasanMemaccessSymbols;
  void LowerHWASAN_CHECK_MEMACCESS(const MachineInstr &MI);
  void EmitHwasanMemaccessSymbols(Module &M);

  // Wrapper needed for tblgenned pseudo lowering.
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const {
    return lowerRISCVMachineOperandToMCOperand(MO, MCOp, *this);
  }

  void emitStartOfAsmFile(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;

private:
  void emitAttributes();
};
} // namespace

#define GEN_COMPRESS_INSTR
#include "RISCVGenCompressInstEmitter.inc"

constexpr uint64_t VentusUnknownResourceSize =
    std::numeric_limits<uint64_t>::max();

static bool isIgnoredVentusLibcall(StringRef Name) {
  return Name.startswith("__riscv_save_") ||
         Name.startswith("__riscv_restore_");
}

static void updateVentusRegUsage(const RISCVRegisterInfo &TRI,
                                 uint64_t &VGPRUsed, uint64_t &SGPRUsed,
                                 Register Reg) {
  if (!Reg.isPhysical())
    return;

  for (MCPhysReg PhysReg : TRI.subregs_inclusive(Reg.asMCReg())) {
    if (PhysReg >= RISCV::V0 && PhysReg <= RISCV::V255) {
      const uint64_t VGPRIndex = PhysReg - RISCV::V0;
      VGPRUsed = std::max(VGPRUsed, VGPRIndex + 1);
      continue;
    }

    if (PhysReg >= RISCV::X0 && PhysReg <= RISCV::X63) {
      const uint64_t SGPRIndex = PhysReg - RISCV::X0;
      SGPRUsed = std::max(SGPRUsed, SGPRIndex + 1);
    }
  }
}

static const GlobalValue *
getVentusDirectCalleeGlobalValue(const MachineInstr &MI, const Module &M) {
  for (const MachineOperand &Op : MI.operands()) {
    if (Op.isGlobal())
      return dyn_cast<GlobalValue>(Op.getGlobal());

    if (Op.isSymbol())
      return M.getNamedValue(Op.getSymbolName());
  }

  return nullptr;
}

static std::optional<StringRef>
getVentusCallTargetName(const MachineInstr &MI) {
  for (const MachineOperand &Op : MI.operands()) {
    if (Op.isGlobal()) {
      if (const auto *GV = dyn_cast<GlobalValue>(Op.getGlobal()))
        return GV->getName();
      return std::nullopt;
    }

    if (Op.isSymbol())
      return StringRef(Op.getSymbolName());
  }

  return std::nullopt;
}

static const Function *resolveVentusDirectCallee(const GlobalValue *TargetGV) {
  if (!TargetGV)
    return nullptr;
  return dyn_cast<Function>(TargetGV->stripPointerCastsAndAliases());
}

static VentusLocalMemUsage
computeVentusLocalMemUsage(const MachineFunction &MF) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &LocalMemGlobals =
      MF.getInfo<RISCVMachineFunctionInfo>()->getLocalMemGlobalFrameIndices();
  SmallDenseSet<int, 4> StaticFrameIndices;
  for (const auto &It : LocalMemGlobals)
    StaticFrameIndices.insert(It.second);

  VentusLocalMemUsage Usage;
  uint64_t TotalBytes = 0;
  for (int I = 0; I != MFI.getObjectIndexEnd(); ++I) {
    if (static_cast<unsigned>(MFI.getStackID(I)) != RISCVStackID::LocalMemSpill)
      continue;

    const Align Alignment =
        MFI.getObjectAlign(I).value() <= 4 ? Align(4) : MFI.getObjectAlign(I);
    const uint64_t NewTotal =
        alignTo(TotalBytes + MFI.getObjectSize(I), Alignment);
    const uint64_t Delta = NewTotal - TotalBytes;
    if (StaticFrameIndices.contains(I))
      Usage.StaticBytes += Delta;
    else
      Usage.StackBytes += Delta;
    TotalBytes = NewTotal;
  }

  return Usage;
}

static void collectVentusReferencedGlobals(
    const Constant &C, SmallPtrSetImpl<const GlobalVariable *> &Globals,
    SmallPtrSetImpl<const Constant *> &VisitedConstants);

static VentusFunctionSummary
collectVentusFunctionSummary(const MachineFunction &MF, const Module &M) {
  const auto &FrameInfo = MF.getFrameInfo();
  const auto &FrameLowering =
      *MF.getSubtarget<RISCVSubtarget>().getFrameLowering();
  const auto &TRI = *MF.getSubtarget<RISCVSubtarget>().getRegisterInfo();
  const auto &TII = *MF.getSubtarget<RISCVSubtarget>().getInstrInfo();
  const auto &LocalMemGlobals =
      MF.getInfo<RISCVMachineFunctionInfo>()->getLocalMemGlobalFrameIndices();
  const VentusLocalMemUsage LocalMemUsage = computeVentusLocalMemUsage(MF);

  VentusFunctionSummary Summary;
  Summary.LDSStackSelf =
      FrameLowering.getStackSize(MF, RISCVStackID::SGPRSpill) +
      LocalMemUsage.StackBytes;
  Summary.PDSStackSelf =
      FrameLowering.getStackSize(MF, RISCVStackID::VGPRSpill);
  Summary.OutgoingPrivateMax = FrameInfo.getMaxCallFrameSize();

  SmallPtrSet<const GlobalVariable *, 8> ReferencedStaticGlobals;
  SmallPtrSet<const Constant *, 16> VisitedConstants;
  for (const auto &It : LocalMemGlobals)
    ReferencedStaticGlobals.insert(It.first);
  for (const BasicBlock &BB : MF.getFunction()) {
    for (const Instruction &I : BB) {
      for (const Value *Operand : I.operand_values()) {
        const auto *OperandConstant = dyn_cast<Constant>(Operand);
        if (!OperandConstant)
          continue;
        collectVentusReferencedGlobals(*OperandConstant, ReferencedStaticGlobals,
                                       VisitedConstants);
      }
    }
  }

  for (const GlobalVariable *GV : ReferencedStaticGlobals) {
    if (GV->getAddressSpace() != RISCVAS::LOCAL_ADDRESS &&
        GV->getAddressSpace() != RISCVAS::PRIVATE_ADDRESS)
      continue;
    Summary.ReferencedStaticGlobals.push_back(GV);
  }
  llvm::sort(Summary.ReferencedStaticGlobals,
             [](const GlobalVariable *LHS, const GlobalVariable *RHS) {
               return LHS->getName() < RHS->getName();
             });

  if (FrameInfo.hasVarSizedObjects())
    Summary.LocalFlags |= VKRF_HasDynamicAlloca;

  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB) {
      if (MI.isMetaInstruction())
        continue;

      for (const MachineOperand &Op : MI.operands()) {
        if (!Op.isReg())
          continue;
        updateVentusRegUsage(TRI, Summary.VGPRUsed, Summary.SGPRUsed,
                             Op.getReg());
      }

      if (!MI.isCall())
        continue;

      std::optional<StringRef> TargetName = getVentusCallTargetName(MI);
      if (!TargetName) {
        Summary.LocalFlags |= VKRF_HasIndirectCall;
        continue;
      }

      if (isIgnoredVentusLibcall(*TargetName))
        continue;

      const GlobalValue *TargetGV = getVentusDirectCalleeGlobalValue(MI, M);
      const Function *Callee = resolveVentusDirectCallee(TargetGV);
      const bool IsTail = TII.isTailCall(MI);

      VentusDirectCallEdge Edge;
      Edge.TargetGV = TargetGV;
      Edge.ResolvedCallee = Callee;
      Edge.TargetName = std::string(*TargetName);
      Edge.IsTail = IsTail;
      Summary.DirectCallEdges.push_back(std::move(Edge));

      if (!Callee || Callee->isDeclarationForLinker())
        continue;

      Summary.DirectCallees.push_back(Callee);
      if (IsTail)
        Summary.TailCallees.push_back(Callee);
      else
        Summary.NonTailCallees.push_back(Callee);
    }
  }

  // Keep the historical behavior where RA counts as a used SGPR.
  updateVentusRegUsage(TRI, Summary.VGPRUsed, Summary.SGPRUsed, RISCV::X1);
  return Summary;
}

static bool
hasUnknownVisibleVentusDirectCallee(const VentusFunctionSummary &Summary) {
  for (const VentusDirectCallEdge &Edge : Summary.DirectCallEdges) {
    if (!Edge.ResolvedCallee || Edge.ResolvedCallee->isDeclarationForLinker())
      return true;
  }
  return false;
}

static uint32_t getVentusResObjFunctionFlags(const Function &F,
                                             const VentusFunctionSummary &S) {
  uint32_t Flags = 0;
  if (F.getCallingConv() == CallingConv::SPIR_KERNEL ||
      F.getCallingConv() == CallingConv::VENTUS_KERNEL)
    Flags |= VROF_IsEntry;
  if (S.LocalFlags & VKRF_HasDynamicAlloca)
    Flags |= VROF_HasDynamicAlloca;
  if (S.LocalFlags & VKRF_HasIndirectCall)
    Flags |= VROF_HasIndirectCall;
  return Flags;
}

static uint32_t getVentusResObjStaticRefFlags(const GlobalVariable &GV) {
  switch (GV.getAddressSpace()) {
  case RISCVAS::LOCAL_ADDRESS:
    return VROS_IsLDSStatic;
  case RISCVAS::PRIVATE_ADDRESS:
    return VROS_IsPDSStatic;
  default:
    return 0;
  }
}

static void emitVentusResObjSymbolRef(AsmPrinter &AP, const GlobalValue *GV,
                                      StringRef Name) {
  const MCSymbol *Sym =
      GV ? AP.getSymbol(GV) : AP.GetExternalSymbolSymbol(Name);
  AP.OutStreamer->emitValue(MCSymbolRefExpr::create(Sym, AP.OutContext),
                            VentusResObjSymbolRefSize);
}

static void emitVentusResObj(AsmPrinter &AP, MCContext &Ctx, const Module &M,
                             const DenseMap<const Function *, VentusFunctionSummary>
                                 &VentusSummaries) {
  SmallVector<const Function *, 16> Functions;
  uint32_t EdgeCount = 0;
  uint32_t StaticRefCount = 0;
  for (const Function &F : M) {
    auto It = VentusSummaries.find(&F);
    if (It == VentusSummaries.end())
      continue;
    Functions.push_back(&F);
    EdgeCount += It->second.DirectCallEdges.size();
    StaticRefCount += It->second.ReferencedStaticGlobals.size();
  }

  if (Functions.empty())
    return;

  const uint32_t FunctionTableOffset = VentusResObjHeaderSize;
  const uint32_t EdgeTableOffset =
      FunctionTableOffset +
      Functions.size() * VentusResObjFunctionRecordSize;
  const uint32_t StaticRefTableOffset =
      EdgeTableOffset + EdgeCount * VentusResObjEdgeRecordSize;

  MCSectionELF *ResObjSection =
      Ctx.getELFSection(".ventus.resobj", ELF::SHT_PROGBITS, 0);
  AP.OutStreamer->switchSection(ResObjSection);
  AP.emitAlignment(Align(8));

  AP.emitInt32(VentusResObjMagic);
  AP.emitInt32(VentusResObjVersion);
  AP.emitInt32(0);
  AP.emitInt32(Functions.size());
  AP.emitInt32(EdgeCount);
  AP.emitInt32(StaticRefCount);
  AP.emitInt32(FunctionTableOffset);
  AP.emitInt32(EdgeTableOffset);
  AP.emitInt32(StaticRefTableOffset);

  uint32_t CurrentEdgeBegin = 0;
  uint32_t CurrentStaticRefBegin = 0;
  for (const Function *F : Functions) {
    const VentusFunctionSummary &Summary = VentusSummaries.find(F)->second;
    emitVentusResObjSymbolRef(AP, F, F->getName());
    AP.emitInt32(getVentusResObjFunctionFlags(*F, Summary));
    AP.emitInt64(Summary.VGPRUsed);
    AP.emitInt64(Summary.SGPRUsed);
    AP.emitInt64(Summary.LDSStackSelf);
    AP.emitInt64(Summary.PDSStackSelf);
    AP.emitInt64(Summary.OutgoingPrivateMax);
    AP.emitInt32(CurrentEdgeBegin);
    AP.emitInt32(Summary.DirectCallEdges.size());
    AP.emitInt32(CurrentStaticRefBegin);
    AP.emitInt32(Summary.ReferencedStaticGlobals.size());
    CurrentEdgeBegin += Summary.DirectCallEdges.size();
    CurrentStaticRefBegin += Summary.ReferencedStaticGlobals.size();
  }

  for (const Function *F : Functions) {
    const VentusFunctionSummary &Summary = VentusSummaries.find(F)->second;
    for (const VentusDirectCallEdge &Edge : Summary.DirectCallEdges) {
      emitVentusResObjSymbolRef(AP, Edge.TargetGV, Edge.TargetName);
      AP.emitInt32(Edge.IsTail ? static_cast<uint32_t>(VROE_IsTail) : 0u);
    }
  }

  for (const Function *F : Functions) {
    const VentusFunctionSummary &Summary = VentusSummaries.find(F)->second;
    for (const GlobalVariable *GV : Summary.ReferencedStaticGlobals) {
      emitVentusResObjSymbolRef(AP, GV, GV->getName());
      AP.emitInt32(getVentusResObjStaticRefFlags(*GV));
      AP.emitInt64(GV->getValueType()->isSized()
                       ? M.getDataLayout().getTypeAllocSize(GV->getValueType())
                       : 0);
    }
  }
}

static void emitVentusKernelResource(AsmPrinter &AP, MCContext &Ctx,
                                     MCStreamer &Streamer, StringRef KernelName,
                                     const VentusKernelResourceV3 &Resource) {
  MCSectionELF *ResourceSection = Ctx.getELFSection(
      ".ventus.resource." + KernelName, ELF::SHT_PROGBITS, ELF::SHF_WRITE);
  Streamer.switchSection(ResourceSection);
  AP.emitAlignment(Align(8));
  AP.emitInt32(Resource.Version);
  AP.emitInt32(Resource.Flags);
  AP.emitInt64(Resource.VGPRUsed);
  AP.emitInt64(Resource.SGPRUsed);
  AP.emitInt64(Resource.LDSStaticBytes);
  AP.emitInt64(Resource.PDSStaticBytes);
  AP.emitInt64(Resource.LDSStackPeakBytes);
  AP.emitInt64(Resource.PDSStackPeakBytes);
}

static bool hasIncompleteVentusRegisterUsage(uint32_t Flags) {
  return Flags & VKRF_HasIndirectCall;
}

static void collectVentusReferencedGlobals(
    const Constant &C, SmallPtrSetImpl<const GlobalVariable *> &Globals,
    SmallPtrSetImpl<const Constant *> &VisitedConstants) {
  if (!VisitedConstants.insert(&C).second)
    return;

  if (const auto *GV = dyn_cast<GlobalVariable>(&C))
    Globals.insert(GV);

  for (const Value *Operand : C.operand_values()) {
    const auto *OperandConstant = dyn_cast<Constant>(Operand);
    if (!OperandConstant)
      continue;
    collectVentusReferencedGlobals(*OperandConstant, Globals, VisitedConstants);
  }
}

static VentusStaticResourceSummary collectVentusStaticResources(
    const Function &Entry,
    const DenseMap<const Function *, VentusFunctionSummary> &VentusSummaries,
    const DataLayout &DL) {
  SmallPtrSet<const Function *, 8> VisitedFunctions;
  SmallPtrSet<const GlobalVariable *, 8> ReferencedGlobals;
  SmallVector<const Function *, 8> Worklist = {&Entry};
  VentusStaticResourceSummary Result;

  while (!Worklist.empty()) {
    const Function *F = Worklist.pop_back_val();
    if (!VisitedFunctions.insert(F).second)
      continue;

    auto SummaryIt = VentusSummaries.find(F);
    if (SummaryIt == VentusSummaries.end())
      continue;

    const VentusFunctionSummary &Summary = SummaryIt->second;
    if (hasIncompleteVentusRegisterUsage(Summary.LocalFlags) ||
        hasUnknownVisibleVentusDirectCallee(Summary))
      Result.Flags |= VKRF_RegisterUsageIncomplete;

    for (const GlobalVariable *GV : Summary.ReferencedStaticGlobals)
      ReferencedGlobals.insert(GV);

    for (const Function *Callee : Summary.DirectCallees) {
      if (VentusSummaries.count(Callee))
        Worklist.push_back(Callee);
    }
  }

  for (const GlobalVariable *GV : ReferencedGlobals) {
    if (GV->isDeclarationForLinker() || !GV->getValueType()->isSized())
      continue;

    const uint64_t Size = DL.getTypeAllocSize(GV->getValueType());
    switch (GV->getAddressSpace()) {
    case RISCVAS::LOCAL_ADDRESS:
      Result.LDSStaticBytes += Size;
      break;
    case RISCVAS::PRIVATE_ADDRESS:
      Result.PDSStaticBytes += Size;
      break;
    default:
      break;
    }
  }

  return Result;
}

void RISCVAsmPrinter::EmitToStreamer(MCStreamer &S, const MCInst &Inst) {
  MCInst CInst;
  bool Res = compressInst(CInst, Inst, *STI, OutStreamer->getContext());
  if (Res)
    ++RISCVNumInstrsCompressed;
  AsmPrinter::EmitToStreamer(*OutStreamer, Res ? CInst : Inst);
}

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "RISCVGenMCPseudoLowering.inc"

void RISCVAsmPrinter::emitInstruction(const MachineInstr *MI) {
  RISCV_MC::verifyInstructionPredicates(MI->getOpcode(),
                                        getSubtargetInfo().getFeatureBits());

  // Do any auto-generated pseudo lowerings.
  if (emitPseudoExpansionLowering(*OutStreamer, MI))
    return;

  MCInst TmpInst;

  if (MI->getOpcode() == RISCV::HWASAN_CHECK_MEMACCESS_SHORTGRANULES) {
    LowerHWASAN_CHECK_MEMACCESS(*MI);
    return;
  }
  if (MI->getOpcode() == RISCV::PseudoVXOR_VI_IMM11)
    return;
  if (!lowerRISCVMachineInstrToMCInst(MI, TmpInst, *this))
    EmitToStreamer(*OutStreamer, TmpInst);
}

bool RISCVAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &OS) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'z':      // Print zero register if zero, regular printing otherwise.
      if (MO.isImm() && MO.getImm() == 0) {
        OS << RISCVInstPrinter::getRegisterName(RISCV::X0);
        return false;
      }
      break;
    case 'i': // Literal 'i' if operand is not a register.
      if (!MO.isReg())
        OS << 'i';
      return false;
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << RISCVInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, OS);
    return false;
  case MachineOperand::MO_BlockAddress: {
    MCSymbol *Sym = GetBlockAddressSymbol(MO.getBlockAddress());
    Sym->print(OS, MAI);
    return false;
  }
  default:
    break;
  }

  return true;
}

bool RISCVAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                            unsigned OpNo,
                                            const char *ExtraCode,
                                            raw_ostream &OS) {
  if (!ExtraCode) {
    const MachineOperand &MO = MI->getOperand(OpNo);
    // For now, we only support register memory operands in registers and
    // assume there is no addend
    if (!MO.isReg())
      return true;

    OS << "0(" << RISCVInstPrinter::getRegisterName(MO.getReg()) << ")";
    return false;
  }

  return AsmPrinter::PrintAsmMemoryOperand(MI, OpNo, ExtraCode, OS);
}

bool RISCVAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  // Set the current MCSubtargetInfo to a copy which has the correct
  // feature bits for the current MachineFunction
  MCSubtargetInfo &NewSTI =
      OutStreamer->getContext().getSubtargetCopy(*TM.getMCSubtargetInfo());
  NewSTI.setFeatureBits(MF.getSubtarget().getFeatureBits());
  MCSTI = &NewSTI;
  STI = &MF.getSubtarget<RISCVSubtarget>();
  VentusSummaries[&MF.getFunction()] =
      collectVentusFunctionSummary(MF, *MF.getFunction().getParent());
  if (MF.getInfo<RISCVMachineFunctionInfo>()->isEntryFunction())
    VentusEntryFunctions.insert(&MF.getFunction());
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

void RISCVAsmPrinter::emitStartOfAsmFile(Module &M) {
  RISCVTargetStreamer &RTS =
      static_cast<RISCVTargetStreamer &>(*OutStreamer->getTargetStreamer());
  if (const MDString *ModuleTargetABI =
          dyn_cast_or_null<MDString>(M.getModuleFlag("target-abi")))
    RTS.setTargetABI(RISCVABI::getTargetABI(ModuleTargetABI->getString()));
  if (TM.getTargetTriple().isOSBinFormatELF())
    emitAttributes();
}

void RISCVAsmPrinter::emitEndOfAsmFile(Module &M) {
  if (TM.getTargetTriple().isOSBinFormatELF()) {
    emitVentusResObj(*this, OutContext, M, VentusSummaries);

    struct VentusComponentRegSummary {
      uint64_t VGPRUsed = 0;
      uint64_t SGPRUsed = 0;
      SmallVector<unsigned, 4> Successors;
    };

    DenseMap<const Function *, unsigned> NodeIndex;
    DenseMap<const Function *, unsigned> LowLink;
    DenseMap<const Function *, unsigned> ComponentIndex;
    SmallVector<const Function *, 16> DFSStack;
    SmallPtrSet<const Function *, 16> OnStack;
    SmallPtrSet<const Function *, 16> RecursiveFunctions;
    SmallVector<SmallVector<const Function *, 4>, 8> Components;
    unsigned NextIndex = 0;

    std::function<void(const Function *)> StrongConnect =
        [&](const Function *F) {
          NodeIndex[F] = NextIndex;
          LowLink[F] = NextIndex++;
          DFSStack.push_back(F);
          OnStack.insert(F);

          const VentusFunctionSummary &Summary =
              VentusSummaries.find(F)->second;
          for (const Function *Callee : Summary.DirectCallees) {
            if (!VentusSummaries.count(Callee))
              continue;

            if (!NodeIndex.count(Callee)) {
              StrongConnect(Callee);
              LowLink[F] = std::min(LowLink[F], LowLink[Callee]);
            } else if (OnStack.count(Callee)) {
              LowLink[F] = std::min(LowLink[F], NodeIndex[Callee]);
            }
          }

          if (LowLink[F] != NodeIndex[F])
            return;

          SmallVector<const Function *, 4> Component;
          const Function *Node = nullptr;
          do {
            Node = DFSStack.pop_back_val();
            OnStack.erase(Node);
            Component.push_back(Node);
          } while (Node != F);

          const unsigned CurrentComponentIndex = Components.size();
          Components.push_back(Component);
          for (const Function *ComponentNode : Component)
            ComponentIndex[ComponentNode] = CurrentComponentIndex;

          bool IsRecursiveComponent = Component.size() > 1;
          if (!IsRecursiveComponent) {
            const VentusFunctionSummary &OnlySummary =
                VentusSummaries.find(Component.front())->second;
            IsRecursiveComponent = llvm::is_contained(OnlySummary.DirectCallees,
                                                      Component.front());
          }

          if (!IsRecursiveComponent)
            return;

          for (const Function *RecursiveFunction : Component)
            RecursiveFunctions.insert(RecursiveFunction);
        };

    for (const auto &It : VentusSummaries) {
      if (!NodeIndex.count(It.first))
        StrongConnect(It.first);
    }

    SmallVector<VentusComponentRegSummary, 8> ComponentRegs(Components.size());
    for (const auto &It : VentusSummaries) {
      const Function *F = It.first;
      const VentusFunctionSummary &Summary = It.second;
      VentusComponentRegSummary &ComponentReg =
          ComponentRegs[ComponentIndex[F]];
      ComponentReg.VGPRUsed = std::max(ComponentReg.VGPRUsed, Summary.VGPRUsed);
      ComponentReg.SGPRUsed = std::max(ComponentReg.SGPRUsed, Summary.SGPRUsed);

      for (const Function *Callee : Summary.DirectCallees) {
        if (!VentusSummaries.count(Callee))
          continue;

        const unsigned CalleeComponentIndex = ComponentIndex[Callee];
        if (CalleeComponentIndex == ComponentIndex[F])
          continue;
        if (!llvm::is_contained(ComponentReg.Successors, CalleeComponentIndex))
          ComponentReg.Successors.push_back(CalleeComponentIndex);
      }
    }

    SmallVector<std::optional<std::pair<uint64_t, uint64_t>>, 8>
        ComponentRegMax(Components.size());
    std::function<std::pair<uint64_t, uint64_t>(unsigned)>
        ComputeComponentRegs = [&](unsigned CurrentComponentIndex)
        -> std::pair<uint64_t, uint64_t> {
      if (ComponentRegMax[CurrentComponentIndex].has_value())
        return *ComponentRegMax[CurrentComponentIndex];

      const VentusComponentRegSummary &ComponentReg =
          ComponentRegs[CurrentComponentIndex];
      std::pair<uint64_t, uint64_t> Result = {ComponentReg.VGPRUsed,
                                              ComponentReg.SGPRUsed};
      for (unsigned SuccessorIndex : ComponentReg.Successors) {
        const std::pair<uint64_t, uint64_t> SuccessorRegs =
            ComputeComponentRegs(SuccessorIndex);
        Result.first = std::max(Result.first, SuccessorRegs.first);
        Result.second = std::max(Result.second, SuccessorRegs.second);
      }

      ComponentRegMax[CurrentComponentIndex] = Result;
      return Result;
    };

    DenseMap<const Function *, VentusPeakSummary> PeakCache;
    std::function<const VentusPeakSummary &(const Function *)> ComputePeak =
        [&](const Function *F) -> const VentusPeakSummary & {
      VentusPeakSummary &Peak = PeakCache[F];
      if (Peak.Finalized)
        return Peak;
      if (Peak.InProgress) {
        Peak.Flags |= VKRF_HasRecursion | VKRF_StackPeakUnavailable;
        Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
        Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
        Peak.Finalized = true;
        Peak.InProgress = false;
        return Peak;
      }

      Peak.InProgress = true;
      const VentusFunctionSummary &Summary = VentusSummaries.find(F)->second;
      Peak.Flags = Summary.LocalFlags;
      if (hasUnknownVisibleVentusDirectCallee(Summary))
        Peak.Flags |= VKRF_HasUnknownExternalCallee;

      if (RecursiveFunctions.count(F))
        Peak.Flags |= VKRF_HasRecursion;

      if (Peak.Flags & (VKRF_HasDynamicAlloca | VKRF_HasRecursion |
                        VKRF_HasIndirectCall | VKRF_HasUnknownExternalCallee)) {
        Peak.Flags |= VKRF_StackPeakUnavailable;
        Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
        Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
        Peak.Finalized = true;
        Peak.InProgress = false;
        return Peak;
      }

      uint64_t PeakLDS = Summary.LDSStackSelf;
      uint64_t PeakPDS = Summary.PDSStackSelf + Summary.OutgoingPrivateMax;
      for (const Function *Callee : Summary.NonTailCallees) {
        if (!VentusSummaries.count(Callee)) {
          Peak.Flags |=
              VKRF_HasUnknownExternalCallee | VKRF_StackPeakUnavailable;
          Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.Finalized = true;
          Peak.InProgress = false;
          return Peak;
        }

        const VentusPeakSummary &CalleePeak = ComputePeak(Callee);
        Peak.Flags |= CalleePeak.Flags;
        if (CalleePeak.Flags & VKRF_StackPeakUnavailable) {
          Peak.Flags |= VKRF_StackPeakUnavailable;
          Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.Finalized = true;
          Peak.InProgress = false;
          return Peak;
        }

        PeakLDS = std::max(PeakLDS,
                           Summary.LDSStackSelf + CalleePeak.LDSStackPeakBytes);
        PeakPDS = std::max(PeakPDS, Summary.PDSStackSelf +
                                        Summary.OutgoingPrivateMax +
                                        CalleePeak.PDSStackPeakBytes);
      }

      for (const Function *Callee : Summary.TailCallees) {
        if (!VentusSummaries.count(Callee)) {
          Peak.Flags |=
              VKRF_HasUnknownExternalCallee | VKRF_StackPeakUnavailable;
          Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.Finalized = true;
          Peak.InProgress = false;
          return Peak;
        }

        const VentusPeakSummary &CalleePeak = ComputePeak(Callee);
        Peak.Flags |= CalleePeak.Flags;
        if (CalleePeak.Flags & VKRF_StackPeakUnavailable) {
          Peak.Flags |= VKRF_StackPeakUnavailable;
          Peak.LDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.PDSStackPeakBytes = VentusUnknownResourceSize;
          Peak.Finalized = true;
          Peak.InProgress = false;
          return Peak;
        }

        PeakLDS = std::max(PeakLDS, CalleePeak.LDSStackPeakBytes);
        PeakPDS = std::max(PeakPDS, CalleePeak.PDSStackPeakBytes);
      }

      Peak.LDSStackPeakBytes = PeakLDS;
      Peak.PDSStackPeakBytes = PeakPDS;
      Peak.Finalized = true;
      Peak.InProgress = false;
      return Peak;
    };

    for (const Function &F : M) {
      if (!VentusEntryFunctions.count(&F))
        continue;

      const VentusPeakSummary &Peak = ComputePeak(&F);
      const VentusStaticResourceSummary StaticResources =
          collectVentusStaticResources(F, VentusSummaries, M.getDataLayout());

      VentusKernelResourceV3 Resource;
      Resource.Flags = Peak.Flags | StaticResources.Flags;
      const std::pair<uint64_t, uint64_t> ReachableRegUsage =
          ComputeComponentRegs(ComponentIndex[&F]);
      Resource.VGPRUsed = ReachableRegUsage.first;
      Resource.SGPRUsed = ReachableRegUsage.second;
      Resource.LDSStaticBytes = StaticResources.LDSStaticBytes;
      Resource.PDSStaticBytes = StaticResources.PDSStaticBytes;
      Resource.LDSStackPeakBytes = Peak.LDSStackPeakBytes;
      Resource.PDSStackPeakBytes = Peak.PDSStackPeakBytes;
      emitVentusKernelResource(*this, OutContext, *OutStreamer, F.getName(),
                               Resource);
    }
  }

  RISCVTargetStreamer &RTS =
      static_cast<RISCVTargetStreamer &>(*OutStreamer->getTargetStreamer());

  if (TM.getTargetTriple().isOSBinFormatELF())
    RTS.finishAttributeSection();
  EmitHwasanMemaccessSymbols(M);
}

void RISCVAsmPrinter::emitAttributes() {
  RISCVTargetStreamer &RTS =
      static_cast<RISCVTargetStreamer &>(*OutStreamer->getTargetStreamer());
  RTS.emitTargetAttributes(*MCSTI);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeRISCVAsmPrinter() {
  RegisterAsmPrinter<RISCVAsmPrinter> X(getTheRISCV32Target());
  RegisterAsmPrinter<RISCVAsmPrinter> Y(getTheRISCV64Target());
}

void RISCVAsmPrinter::LowerHWASAN_CHECK_MEMACCESS(const MachineInstr &MI) {
  Register Reg = MI.getOperand(0).getReg();
  uint32_t AccessInfo = MI.getOperand(1).getImm();
  MCSymbol *&Sym =
      HwasanMemaccessSymbols[HwasanMemaccessTuple(Reg, AccessInfo)];
  if (!Sym) {
    // FIXME: Make this work on non-ELF.
    if (!TM.getTargetTriple().isOSBinFormatELF())
      report_fatal_error("llvm.hwasan.check.memaccess only supported on ELF");

    std::string SymName = "__hwasan_check_x" + utostr(Reg - RISCV::X0) + "_" +
                          utostr(AccessInfo) + "_short";
    Sym = OutContext.getOrCreateSymbol(SymName);
  }
  auto Res = MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, OutContext);
  auto Expr = RISCVMCExpr::create(Res, RISCVMCExpr::VK_RISCV_CALL, OutContext);

  EmitToStreamer(*OutStreamer, MCInstBuilder(RISCV::PseudoCALL).addExpr(Expr));
}

void RISCVAsmPrinter::EmitHwasanMemaccessSymbols(Module &M) {
  if (HwasanMemaccessSymbols.empty())
    return;

  assert(TM.getTargetTriple().isOSBinFormatELF());

  MCSymbol *HwasanTagMismatchV2Sym =
      OutContext.getOrCreateSymbol("__hwasan_tag_mismatch_v2");

  const MCSymbolRefExpr *HwasanTagMismatchV2Ref =
      MCSymbolRefExpr::create(HwasanTagMismatchV2Sym, OutContext);

  for (auto &P : HwasanMemaccessSymbols) {
    unsigned Reg = std::get<0>(P.first);
    uint32_t AccessInfo = std::get<1>(P.first);
    const MCSymbolRefExpr *HwasanTagMismatchRef = HwasanTagMismatchV2Ref;
    MCSymbol *Sym = P.second;

    unsigned Size =
        1 << ((AccessInfo >> HWASanAccessInfo::AccessSizeShift) & 0xf);
    OutStreamer->switchSection(OutContext.getELFSection(
        ".text.hot", ELF::SHT_PROGBITS,
        ELF::SHF_EXECINSTR | ELF::SHF_ALLOC | ELF::SHF_GROUP, 0, Sym->getName(),
        /*IsComdat=*/true));

    OutStreamer->emitSymbolAttribute(Sym, MCSA_ELF_TypeFunction);
    OutStreamer->emitSymbolAttribute(Sym, MCSA_Weak);
    OutStreamer->emitSymbolAttribute(Sym, MCSA_Hidden);
    OutStreamer->emitLabel(Sym);

    // Extract shadow offset from ptr
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::SLLI).addReg(RISCV::X6).addReg(Reg).addImm(8),
        *STI);
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::SRLI)
                                     .addReg(RISCV::X6)
                                     .addReg(RISCV::X6)
                                     .addImm(12),
                                 *STI);
    // load shadow tag in X6, X5 contains shadow base
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::ADD)
                                     .addReg(RISCV::X6)
                                     .addReg(RISCV::X5)
                                     .addReg(RISCV::X6),
                                 *STI);
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::LBU).addReg(RISCV::X6).addReg(RISCV::X6).addImm(0),
        *STI);
    // Extract tag from X5 and compare it with loaded tag from shadow
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::SRLI).addReg(RISCV::X7).addReg(Reg).addImm(56),
        *STI);
    MCSymbol *HandleMismatchOrPartialSym = OutContext.createTempSymbol();
    // X7 contains tag from memory, while X6 contains tag from the pointer
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::BNE)
            .addReg(RISCV::X7)
            .addReg(RISCV::X6)
            .addExpr(MCSymbolRefExpr::create(HandleMismatchOrPartialSym,
                                             OutContext)),
        *STI);
    MCSymbol *ReturnSym = OutContext.createTempSymbol();
    OutStreamer->emitLabel(ReturnSym);
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::JALR)
                                     .addReg(RISCV::X0)
                                     .addReg(RISCV::X1)
                                     .addImm(0),
                                 *STI);
    OutStreamer->emitLabel(HandleMismatchOrPartialSym);

    OutStreamer->emitInstruction(MCInstBuilder(RISCV::ADDI)
                                     .addReg(RISCV::X28)
                                     .addReg(RISCV::X0)
                                     .addImm(16),
                                 *STI);
    MCSymbol *HandleMismatchSym = OutContext.createTempSymbol();
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::BGEU)
            .addReg(RISCV::X6)
            .addReg(RISCV::X28)
            .addExpr(MCSymbolRefExpr::create(HandleMismatchSym, OutContext)),
        *STI);

    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::ANDI).addReg(RISCV::X28).addReg(Reg).addImm(0xF),
        *STI);

    if (Size != 1)
      OutStreamer->emitInstruction(MCInstBuilder(RISCV::ADDI)
                                       .addReg(RISCV::X28)
                                       .addReg(RISCV::X28)
                                       .addImm(Size - 1),
                                   *STI);
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::BGE)
            .addReg(RISCV::X28)
            .addReg(RISCV::X6)
            .addExpr(MCSymbolRefExpr::create(HandleMismatchSym, OutContext)),
        *STI);

    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::ORI).addReg(RISCV::X6).addReg(Reg).addImm(0xF),
        *STI);
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::LBU).addReg(RISCV::X6).addReg(RISCV::X6).addImm(0),
        *STI);
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::BEQ)
            .addReg(RISCV::X6)
            .addReg(RISCV::X7)
            .addExpr(MCSymbolRefExpr::create(ReturnSym, OutContext)),
        *STI);

    OutStreamer->emitLabel(HandleMismatchSym);

    // | Previous stack frames...        |
    // +=================================+ <-- [SP + 256]
    // |              ...                |
    // |                                 |
    // | Stack frame space for x12 - x31.|
    // |                                 |
    // |              ...                |
    // +---------------------------------+ <-- [SP + 96]
    // | Saved x11(arg1), as             |
    // | __hwasan_check_* clobbers it.   |
    // +---------------------------------+ <-- [SP + 88]
    // | Saved x10(arg0), as             |
    // | __hwasan_check_* clobbers it.   |
    // +---------------------------------+ <-- [SP + 80]
    // |                                 |
    // | Stack frame space for x9.       |
    // +---------------------------------+ <-- [SP + 72]
    // |                                 |
    // | Saved x8(fp), as                |
    // | __hwasan_check_* clobbers it.   |
    // +---------------------------------+ <-- [SP + 64]
    // |              ...                |
    // |                                 |
    // | Stack frame space for x2 - x7.  |
    // |                                 |
    // |              ...                |
    // +---------------------------------+ <-- [SP + 16]
    // | Return address (x1) for caller  |
    // | of __hwasan_check_*.            |
    // +---------------------------------+ <-- [SP + 8]
    // | Reserved place for x0, possibly |
    // | junk, since we don't save it.   |
    // +---------------------------------+ <-- [x2 / SP]

    // Adjust sp
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::ADDI)
                                     .addReg(RISCV::X4)
                                     .addReg(RISCV::X4)
                                     .addImm(-256),
                                 *STI);

    // store x10(arg0) by new sp
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::SD)
                                     .addReg(RISCV::X10)
                                     .addReg(RISCV::X4)
                                     .addImm(8 * 10),
                                 *STI);
    // store x11(arg1) by new sp
    OutStreamer->emitInstruction(MCInstBuilder(RISCV::SD)
                                     .addReg(RISCV::X11)
                                     .addReg(RISCV::X4)
                                     .addImm(8 * 11),
                                 *STI);

    // store x8(fp) by new sp
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::SD).addReg(RISCV::X8).addReg(RISCV::X4).addImm(8 *
                                                                            8),
        *STI);
    // store x1(ra) by new sp
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::SD).addReg(RISCV::X1).addReg(RISCV::X4).addImm(1 *
                                                                            8),
        *STI);
    if (Reg != RISCV::X10)
      OutStreamer->emitInstruction(MCInstBuilder(RISCV::OR)
                                       .addReg(RISCV::X10)
                                       .addReg(RISCV::X0)
                                       .addReg(Reg),
                                   *STI);
    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::ADDI)
            .addReg(RISCV::X11)
            .addReg(RISCV::X0)
            .addImm(AccessInfo & HWASanAccessInfo::RuntimeMask),
        *STI);

    // Intentionally load the GOT entry and branch to it, rather than possibly
    // late binding the function, which may clobber the registers before we have
    // a chance to save them.
    RISCVMCExpr::VariantKind VKHi;
    unsigned SecondOpcode;
    if (OutContext.getObjectFileInfo()->isPositionIndependent()) {
      SecondOpcode = RISCV::LD;
      VKHi = RISCVMCExpr::VK_RISCV_GOT_HI;
    } else {
      SecondOpcode = RISCV::ADDI;
      VKHi = RISCVMCExpr::VK_RISCV_PCREL_HI;
    }
    auto ExprHi = RISCVMCExpr::create(HwasanTagMismatchRef, VKHi, OutContext);

    MCSymbol *TmpLabel =
        OutContext.createTempSymbol("pcrel_hi", /* AlwaysAddSuffix */ true);
    OutStreamer->emitLabel(TmpLabel);
    const MCExpr *ExprLo =
        RISCVMCExpr::create(MCSymbolRefExpr::create(TmpLabel, OutContext),
                            RISCVMCExpr::VK_RISCV_PCREL_LO, OutContext);

    OutStreamer->emitInstruction(
        MCInstBuilder(RISCV::AUIPC).addReg(RISCV::X6).addExpr(ExprHi), *STI);
    OutStreamer->emitInstruction(MCInstBuilder(SecondOpcode)
                                     .addReg(RISCV::X6)
                                     .addReg(RISCV::X6)
                                     .addExpr(ExprLo),
                                 *STI);

    OutStreamer->emitInstruction(MCInstBuilder(RISCV::JALR)
                                     .addReg(RISCV::X0)
                                     .addReg(RISCV::X6)
                                     .addImm(0),
                                 *STI);
  }
}
