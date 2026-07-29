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
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-broadcast-copy-propagation"

namespace {

constexpr unsigned CopyWorklistInitialSize = 16;

struct BroadcastCopyPropagationContext {
  const RISCVInstrInfo &TII;
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

static Register
findScalarSourceForBroadcast(const BroadcastCopyPropagationContext &Ctx,
                             const MachineInstr &UseMI, Register VGPR) {
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

static bool
canReplaceWithScalarSource(const BroadcastCopyPropagationContext &Ctx,
                           const MachineInstr &MI, Register ScalarSrc) {
  const TargetRegisterClass *DstRC =
      getRegClass(Ctx, MI.getOperand(0).getReg());
  const TargetRegisterClass *ScalarSrcRC = getRegClass(Ctx, ScalarSrc);
  return isGPRLikeScalarClass(DstRC) && isGPRLikeScalarClass(ScalarSrcRC) &&
         isLegalCopyDirection(DstRC, ScalarSrcRC);
}

static MachineInstr *
getSingleNonDebugUser(const BroadcastCopyPropagationContext &Ctx,
                      Register Reg) {
  if (!Reg.isVirtual() || !Ctx.MRI.hasOneNonDBGUse(Reg))
    return nullptr;
  return &*Ctx.MRI.use_instr_nodbg_begin(Reg);
}

static unsigned getVVOpcodeForVX(unsigned Opcode) {
  switch (Opcode) {
  default: return 0;
  case RISCV::VADD_VX: return RISCV::VADD_VV;
  case RISCV::VSUB_VX: return RISCV::VSUB_VV;
  case RISCV::VMINU_VX: return RISCV::VMINU_VV;
  case RISCV::VMIN_VX: return RISCV::VMIN_VV;
  case RISCV::VMAXU_VX: return RISCV::VMAXU_VV;
  case RISCV::VMAX_VX: return RISCV::VMAX_VV;
  case RISCV::VAND_VX: return RISCV::VAND_VV;
  case RISCV::VOR_VX: return RISCV::VOR_VV;
  case RISCV::VXOR_VX: return RISCV::VXOR_VV;
  case RISCV::VMSEQ_VX: return RISCV::VMSEQ_VV;
  case RISCV::VMSNE_VX: return RISCV::VMSNE_VV;
  case RISCV::VMSLTU_VX: return RISCV::VMSLTU_VV;
  case RISCV::VMSLT_VX: return RISCV::VMSLT_VV;
  case RISCV::VMSLEU_VX: return RISCV::VMSLEU_VV;
  case RISCV::VMSLE_VX: return RISCV::VMSLE_VV;
  case RISCV::VSLL_VX: return RISCV::VSLL_VV;
  case RISCV::VSRL_VX: return RISCV::VSRL_VV;
  case RISCV::VSRA_VX: return RISCV::VSRA_VV;
  case RISCV::VSSRL_VX: return RISCV::VSSRL_VV;
  case RISCV::VSSRA_VX: return RISCV::VSSRA_VV;
  case RISCV::VDIVU_VX: return RISCV::VDIVU_VV;
  case RISCV::VDIV_VX: return RISCV::VDIV_VV;
  case RISCV::VREMU_VX: return RISCV::VREMU_VV;
  case RISCV::VREM_VX: return RISCV::VREM_VV;
  case RISCV::VMULHU_VX: return RISCV::VMULHU_VV;
  case RISCV::VMUL_VX: return RISCV::VMUL_VV;
  case RISCV::VMULHSU_VX: return RISCV::VMULHSU_VV;
  case RISCV::VMULH_VX: return RISCV::VMULH_VV;
  }
}

/* Inlined bit-manipulation helpers can temporarily type a divergent value as
 * scalar. Only erase the bridge when every use has an exact lane-wise form;
 * otherwise leave it for the register-domain verifier to reject. */
static bool rewriteVGPRScalarVectorUses(BroadcastCopyPropagationContext &Ctx,
                                        MachineInstr &MI) {
  if (!MI.isCopy())
    return false;

  Register ScalarReg = MI.getOperand(0).getReg();
  Register VGPRSrc = MI.getOperand(1).getReg();
  if (!isGPRLikeScalarClass(getRegClass(Ctx, ScalarReg)) ||
      !isVentusVGPRClass(getRegClass(Ctx, VGPRSrc)) ||
      MI.getOperand(0).getSubReg() || MI.getOperand(1).getSubReg())
    return false;

  SmallVector<MachineOperand *, CopyWorklistInitialSize> Uses;
  for (MachineOperand &Use : Ctx.MRI.use_operands(ScalarReg)) {
    MachineInstr *User = Use.getParent();
    if (User->isDebugInstr())
      continue;
    if (User->isCopy() && User->getNumOperands() >= 2 &&
        User->getOperand(1).getReg() == ScalarReg &&
        !User->getOperand(0).getSubReg() && !Use.getSubReg() &&
        isVentusVGPRClass(getRegClass(Ctx, User->getOperand(0).getReg()))) {
      Uses.push_back(&Use);
      continue;
    }

    unsigned VVOpcode = getVVOpcodeForVX(User->getOpcode());
    if (VVOpcode && User->getNumOperands() >= 3 &&
        User->getOperand(2).isReg() &&
        User->getOperand(2).getReg() == ScalarReg && !Use.getSubReg() &&
        isVentusVGPRClass(getRegClass(Ctx, User->getOperand(0).getReg())) &&
        isVentusVGPRClass(getRegClass(Ctx, User->getOperand(1).getReg()))) {
      Uses.push_back(&Use);
      continue;
    }
    return false;
  }
  if (Uses.empty())
    return false;

  Ctx.MRI.clearKillFlags(VGPRSrc);
  for (MachineOperand *Use : Uses) {
    MachineInstr *User = Use->getParent();
    if (!User->isCopy())
      User->setDesc(Ctx.TII.get(getVVOpcodeForVX(User->getOpcode())));
    Use->setReg(VGPRSrc);
    Use->setIsKill(false);
  }
  MI.eraseFromParent();
  LLVM_DEBUG(dbgs() << "Vectorized divergent scalar bridge\n");
  return true;
}

static bool rewriteVGPRScalarRoundTrip(BroadcastCopyPropagationContext &Ctx,
                                       MachineInstr &MI) {
  if (!MI.isCopy())
    return false;

  Register ScalarReg = MI.getOperand(0).getReg();
  Register VGPRSrc = MI.getOperand(1).getReg();
  const TargetRegisterClass *ScalarRC = getRegClass(Ctx, ScalarReg);
  const TargetRegisterClass *VGPRSrcRC = getRegClass(Ctx, VGPRSrc);
  if (isLegalCopyDirection(ScalarRC, VGPRSrcRC))
    return false;

  if (VGPRSrcRC != &RISCV::VGPRRegClass || MI.getOperand(0).getSubReg() ||
      MI.getOperand(1).getSubReg())
    return false;

  MachineInstr *CopyToVGPR = getSingleNonDebugUser(Ctx, ScalarReg);
  if (!CopyToVGPR ||
      (!CopyToVGPR->isCopy() && CopyToVGPR->getOpcode() != RISCV::VMV_V_X))
    return false;

  if (CopyToVGPR->getOperand(1).getReg() != ScalarReg ||
      CopyToVGPR->getOperand(0).getSubReg() ||
      CopyToVGPR->getOperand(1).getSubReg())
    return false;

  Register FinalDst = CopyToVGPR->getOperand(0).getReg();
  if (getRegClass(Ctx, FinalDst) != &RISCV::VGPRRegClass)
    return false;

  Ctx.MRI.clearKillFlags(VGPRSrc);
  if (CopyToVGPR->isCopy()) {
    CopyToVGPR->getOperand(1).setReg(VGPRSrc);
    CopyToVGPR->getOperand(1).setIsKill(false);
  } else {
    BuildMI(*CopyToVGPR->getParent(), CopyToVGPR,
            CopyToVGPR->getDebugLoc(), Ctx.TII.get(TargetOpcode::COPY),
            FinalDst)
        .addReg(VGPRSrc);
    CopyToVGPR->eraseFromParent();
  }
  MI.eraseFromParent();
  LLVM_DEBUG(dbgs() << "Rewrote VGPR scalar round trip: " << *CopyToVGPR);
  return true;
}

/* SelectionDAG can choose scalar FSQRT_S before discovering that its input is
 * divergent.  Keep this legalization next to the analogous COPY round-trip:
 * replacing the bridge with VFSQRT_V preserves every lane, unlike allowing a
 * VGPR-to-GPR copy through the register-domain verifier. */
static bool rewriteVFSqrtScalarRoundTrip(
    BroadcastCopyPropagationContext &Ctx, MachineInstr &MI) {
  if (!MI.isCopy())
    return false;

  Register ScalarReg = MI.getOperand(0).getReg();
  Register VGPRSrc = MI.getOperand(1).getReg();
  if (!isVentusVGPRClass(getRegClass(Ctx, VGPRSrc)) ||
      !isGPRLikeScalarClass(getRegClass(Ctx, ScalarReg)))
    return false;

  MachineInstr *Sqrt = getSingleNonDebugUser(Ctx, ScalarReg);
  if (!Sqrt || Sqrt->getOpcode() != RISCV::FSQRT_S ||
      Sqrt->getOperand(1).getReg() != ScalarReg)
    return false;

  Register ScalarResult = Sqrt->getOperand(0).getReg();
  MachineInstr *CopyToVGPR = getSingleNonDebugUser(Ctx, ScalarResult);
  if (!CopyToVGPR || !CopyToVGPR->isCopy() ||
      CopyToVGPR->getOperand(1).getReg() != ScalarResult)
    return false;

  Register FinalDst = CopyToVGPR->getOperand(0).getReg();
  if (!isVentusVGPRClass(getRegClass(Ctx, FinalDst)))
    return false;

  Ctx.MRI.clearKillFlags(VGPRSrc);
  BuildMI(*CopyToVGPR->getParent(), CopyToVGPR, CopyToVGPR->getDebugLoc(),
          Ctx.TII.get(RISCV::VFSQRT_V), FinalDst)
      .addReg(VGPRSrc);
  CopyToVGPR->eraseFromParent();
  Sqrt->eraseFromParent();
  MI.eraseFromParent();
  LLVM_DEBUG(dbgs() << "Rewrote divergent scalar sqrt round trip\n");
  return true;
}

static bool rewriteF16ScalarRoundTrip(BroadcastCopyPropagationContext &Ctx,
                                      MachineInstr &MI) {
  if (!MI.isCopy())
    return false;

  Register ScalarReg = MI.getOperand(0).getReg();
  Register VGPRSrc = MI.getOperand(1).getReg();
  const TargetRegisterClass *ScalarRC = getRegClass(Ctx, ScalarReg);
  const TargetRegisterClass *VGPRSrcRC = getRegClass(Ctx, VGPRSrc);
  if (isLegalCopyDirection(ScalarRC, VGPRSrcRC))
    return false;

  MachineInstr *Fmv = getSingleNonDebugUser(Ctx, ScalarReg);
  if (!Fmv || Fmv->getOpcode() != RISCV::FMV_H_X)
    return false;

  Register FPRReg = Fmv->getOperand(0).getReg();
  MachineInstr *CopyToVGPR = getSingleNonDebugUser(Ctx, FPRReg);
  if (!CopyToVGPR || !CopyToVGPR->isCopy())
    return false;

  Register FinalDst = CopyToVGPR->getOperand(0).getReg();
  if (!isVentusVGPRClass(getRegClass(Ctx, FinalDst)))
    return false;

  Ctx.MRI.clearKillFlags(VGPRSrc);
  CopyToVGPR->getOperand(1).setReg(VGPRSrc);
  CopyToVGPR->getOperand(1).setIsKill(false);
  Fmv->eraseFromParent();
  MI.eraseFromParent();
  LLVM_DEBUG(dbgs() << "Rewrote f16 VGPR scalar round trip: " << *CopyToVGPR);
  return true;
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
  const RISCVInstrInfo &TII = *ST.getInstrInfo();
  const RISCVRegisterInfo &TRI = *ST.getRegisterInfo();
  BroadcastCopyPropagationContext Ctx{TII, TRI, MRI};
  SmallVector<MachineInstr *, CopyWorklistInitialSize> Copies;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.isCopy())
        Copies.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *MI : Copies) {
    if (rewriteVGPRScalarVectorUses(Ctx, *MI)) {
      Changed = true;
      continue;
    }
    if (rewriteVFSqrtScalarRoundTrip(Ctx, *MI)) {
      Changed = true;
      continue;
    }
    if (rewriteVGPRScalarRoundTrip(Ctx, *MI)) {
      Changed = true;
      continue;
    }
    if (rewriteF16ScalarRoundTrip(Ctx, *MI)) {
      Changed = true;
      continue;
    }
    Changed |= rewriteIllegalCopy(Ctx, *MI);
  }

  return Changed;
}
