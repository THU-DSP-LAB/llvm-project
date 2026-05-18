//===-- VentusBroadcastCopyPropagation.cpp - Propagate broadcasts ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "VentusRegisterDomain.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-broadcast-copy-propagation"

namespace {

constexpr unsigned CopyWorklistInitialSize = 16;

struct BroadcastCopyPropagationContext {
  const RISCVRegisterInfo &TRI;
  MachineRegisterInfo &MRI;
};

class VentusBroadcastCopyPropagation : public MachineFunctionPass {
public:
  static char ID;

  VentusBroadcastCopyPropagation() : MachineFunctionPass(ID) {
    initializeVentusBroadcastCopyPropagationPass(
        *PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Ventus Broadcast Copy Propagation";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // namespace

INITIALIZE_PASS(VentusBroadcastCopyPropagation, DEBUG_TYPE,
                "Ventus Broadcast Copy Propagation", false, false)

char VentusBroadcastCopyPropagation::ID = 0;

FunctionPass *llvm::createVentusBroadcastCopyPropagationPass() {
  return new VentusBroadcastCopyPropagation();
}

static const TargetRegisterClass *
getRegClass(const BroadcastCopyPropagationContext &Ctx, Register Reg) {
  if (!Reg)
    return nullptr;
  if (Reg.isVirtual())
    return Ctx.MRI.getRegClass(Reg);
  return Ctx.TRI.getPhysRegClass(Reg.asMCReg());
}

static Register getPureBroadcastSource(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case TargetOpcode::COPY:
  case RISCV::VMV_V_X:
  case RISCV::VFMV_V_F:
    return MI.getOperand(1).getReg();
  default:
    return Register();
  }
}

static Register findScalarSourceForBroadcast(
    const BroadcastCopyPropagationContext &Ctx, const MachineInstr &UseMI,
    Register VGPR) {
  if (!VGPR.isVirtual() || !Ctx.MRI.hasOneDef(VGPR))
    return Register();

  const MachineInstr *DefMI = Ctx.MRI.getVRegDef(VGPR);
  if (!DefMI)
    return Register();

  Register SrcReg = getPureBroadcastSource(*DefMI);
  if (!SrcReg)
    return Register();

  const TargetRegisterClass *DstRC = getRegClass(Ctx, VGPR);
  const TargetRegisterClass *SrcRC = getRegClass(Ctx, SrcReg);
  if (!isVentusVGPRClass(DstRC) || !isGPRLikeScalarClass(SrcRC))
    return Register();

  if (SrcReg.isPhysical() && SrcReg != RISCV::X0)
      return Register();

  return SrcReg;
}

static bool canReplaceWithScalarSource(
    const BroadcastCopyPropagationContext &Ctx, const MachineInstr &MI,
    Register ScalarSrc) {
  const TargetRegisterClass *DstRC =
      getRegClass(Ctx, MI.getOperand(0).getReg());
  const TargetRegisterClass *ScalarSrcRC = getRegClass(Ctx, ScalarSrc);
  return isGPRLikeScalarClass(DstRC) && isGPRLikeScalarClass(ScalarSrcRC) &&
         isLegalCopyDirection(DstRC, ScalarSrcRC);
}

static bool rewriteIllegalCopy(BroadcastCopyPropagationContext &Ctx,
                               MachineInstr &MI) {
  if (!MI.isCopy())
    return false;

  Register DstReg = MI.getOperand(0).getReg();
  Register SrcReg = MI.getOperand(1).getReg();
  const TargetRegisterClass *DstRC = getRegClass(Ctx, DstReg);
  const TargetRegisterClass *SrcRC = getRegClass(Ctx, SrcReg);
  if (isLegalCopyDirection(DstRC, SrcRC))
    return false;

  Register ScalarSrc = findScalarSourceForBroadcast(Ctx, MI, SrcReg);
  if (!ScalarSrc || !canReplaceWithScalarSource(Ctx, MI, ScalarSrc))
    return false;

  Ctx.MRI.clearKillFlags(ScalarSrc);
  MI.getOperand(1).setReg(ScalarSrc);
  MI.getOperand(1).setIsKill(false);
  LLVM_DEBUG(dbgs() << "Rewrote scalar use of broadcast: " << MI);
  return true;
}

bool VentusBroadcastCopyPropagation::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.isVentusGPGPU())
    return false;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const RISCVRegisterInfo &TRI = *ST.getRegisterInfo();
  BroadcastCopyPropagationContext Ctx{TRI, MRI};
  SmallVector<MachineInstr *, CopyWorklistInitialSize> Copies;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.isCopy())
        Copies.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *MI : Copies)
    Changed |= rewriteIllegalCopy(Ctx, *MI);

  return Changed;
}
