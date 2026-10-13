//===-- VentusVVInstrConversion.cpp - VV instruction conversion -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that convert vop.vv instructions to vop.vx/vf
// instructions because currently, the objects stored in sGPR and sGPRF32 will
// be moved to VGPR in divergent nodes, so the patterns which match VX/VF
// instructions will not be matched
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "RISCVTargetMachine.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define VENTUS_VV_INSTRUCTION_CONVRSION "Ventus VV instruction conversion pass"
#define DEBUG_TYPE "Ventus VV instruction conversion"

using namespace llvm;

namespace {

/// This map is a reflection of VV instruction to VX/VF instruction
/// currently, we use enum to represent all the reflections
DenseMap<unsigned, unsigned> VV2VXOpcodeMap = {
    {RISCV::VADD_VV    ,    RISCV::VADD_VX},
    {RISCV::VSUB_VV    ,    RISCV::VSUB_VX},
    {RISCV::VMINU_VV   ,    RISCV::VMINU_VX},
    {RISCV::VMIN_VV    ,    RISCV::VMINU_VX},
    {RISCV::VMAX_VV    ,    RISCV::VMAX_VX},
    {RISCV::VMAXU_VV   ,    RISCV::VMAXU_VX},
    {RISCV::VAND_VV    ,    RISCV::VAND_VX},
    {RISCV::VOR_VV     ,    RISCV::VOR_VX},
    {RISCV::VXOR_VV    ,    RISCV::VXOR_VX},
    {RISCV::VMSEQ_VV   ,    RISCV::VMSEQ_VX},
    {RISCV::VMSNE_VV   ,    RISCV::VMSNE_VX},
    {RISCV::VMSLTU_VV  ,    RISCV::VMSLTU_VX},
    {RISCV::VMSLT_VV   ,    RISCV::VMSLT_VX},
    {RISCV::VMSLEU_VV  ,    RISCV::VMSLEU_VX},
    {RISCV::VMSLE_VV   ,    RISCV::VMSLE_VX},
    {RISCV::VSLL_VV    ,    RISCV::VSLL_VX},
    {RISCV::VSRL_VV    ,    RISCV::VSRL_VX},
    {RISCV::VSRA_VV    ,    RISCV::VSRA_VX},
    {RISCV::VSSRL_VV   ,    RISCV::VSSRL_VX},
    {RISCV::VSSRA_VV   ,    RISCV::VSSRA_VX},
    {RISCV::VDIVU_VV   ,    RISCV::VDIVU_VX},
    {RISCV::VDIV_VV    ,    RISCV::VDIV_VX},
    {RISCV::VREMU_VV   ,    RISCV::VREMU_VX},
    // {RISCV::VFSUB_VV   ,    RISCV::VFSUB_VF},
    {RISCV::VREM_VV    ,    RISCV::VREM_VX},
    {RISCV::VMULHU_VV  ,    RISCV::VMULHU_VX},
    {RISCV::VMUL_VV    ,    RISCV::VMUL_VX},
    {RISCV::VMULHSU_VV ,    RISCV::VMULHSU_VX},
    {RISCV::VMULH_VV   ,    RISCV::VMULH_VX},
    {RISCV::VMADD_VV   ,    RISCV::VMADD_VX},
    {RISCV::VNMSUB_VV  ,    RISCV::VNMSUB_VX},
    {RISCV::VMACC_VV   ,    RISCV::VMACC_VX},
    {RISCV::VNMSAC_VV  ,    RISCV::VNMSAC_VX},
    // {RISCV::VFADD_VV   ,    RISCV::VFADD_VF},
    // {RISCV::VFMSUB_VV  ,    RISCV::VFMSUB_VF},
    // {RISCV::VFMIN_VV   ,    RISCV::VFMIN_VF},
    // {RISCV::VFMAX_VV   ,    RISCV::VFMAX_VF},
    // {RISCV::VFSGNJ_VV  ,    RISCV::VFSGNJ_VF},
    // {RISCV::VFSGNJN_VV ,    RISCV::VFSGNJN_VF},
    // {RISCV::VFSGNJX_VV ,    RISCV::VFSGNJX_VF},
    // {RISCV::VMFEQ_VV   ,    RISCV::VMFEQ_VF},
    // {RISCV::VMFLE_VV   ,    RISCV::VMFLE_VF},
    // {RISCV::VMFLT_VV   ,    RISCV::VMFLT_VF},
    // {RISCV::VMFNE_VV   ,    RISCV::VMFNE_VF},
    // {RISCV::VFDIV_VV   ,    RISCV::VFDIV_VF},
    // {RISCV::VFMUL_VV   ,    RISCV::VFMUL_VF},
    // {RISCV::VFMADD_VV  ,    RISCV::VFMADD_VF},
    // {RISCV::VFNMADD_VV ,    RISCV::VFNMADD_VF},
    // {RISCV::VFMACC_VV  ,    RISCV::VFMACC_VF},
    // {RISCV::VFNMACC_VV ,    RISCV::VFNMACC_VF},
    // {RISCV::VFNMSUB_VV ,    RISCV::VFNMSUB_VF},
    // {RISCV::VFMSAC_VV  ,    RISCV::VFMSAC_VF},
    // {RISCV::VFNMSAC_VV ,    RISCV::VFNMSAC_VF}
    };

class VentusVVInstrConversion : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;
  const RISCVRegisterInfo *MRI;
  MachineRegisterInfo *MR;
  bool EnableLegacyVXConversion;

  VentusVVInstrConversion() : MachineFunctionPass(ID) {
    initializeVentusVVInstrConversionPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return VENTUS_VV_INSTRUCTION_CONVRSION;
  }

private:
  bool runOnMachineBasicBlock(MachineBasicBlock &MBB);

  /// Check if the instruction is VV ALU instruction or not
  bool isVVALUInstruction(MachineInstr &MI) {
    return RISCVII::isVVALUInstr(MI.getDesc().TSFlags);
  };

  bool isVALUCommutableInstr(MachineInstr &MI);

  bool convertInstr(MachineBasicBlock &MBB, MachineInstr &CopyMI,
                    MachineInstr &VVMI, bool &ErasedVVMI);

  bool lowerFloatVFInstr(MachineBasicBlock &MBB, MachineInstr &MI);

  bool removeDeadScalarizationInstrs(MachineBasicBlock &MBB);

  const TargetRegisterClass *getRegClass(Register Reg) const;

  bool isGPR2VGPRCopy(MachineInstr &MI);
};

char VentusVVInstrConversion::ID = 0;

bool VentusVVInstrConversion::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  bool isChanged = false;
  TII = ST.getInstrInfo();
  MRI = ST.getRegisterInfo();
  MR = &MF.getRegInfo();
  EnableLegacyVXConversion = ST.isVentusGPGPU();
  for (auto &MBB : MF)
    isChanged |= runOnMachineBasicBlock(MBB);
  return isChanged;
}

bool VentusVVInstrConversion::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool isMBBChanged = false;
  for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
    MachineInstr &MI = *I;
    auto NextI = std::next(I);
    if (lowerFloatVFInstr(MBB, MI)) {
      isMBBChanged = true;
      I = NextI;
      continue;
    }

    MachineInstr *NextMI = MI.getNextNode();
    // Check RISCV::COPY instructions' format and its next instruction's format
    if (EnableLegacyVXConversion && isGPR2VGPRCopy(MI) && NextMI &&
        isVVALUInstruction(*NextMI)) {
      // When met here, we can ensure the coding logic goes to the conversion
      auto AfterNextI = std::next(NextI);
      bool ErasedVVMI = false;
      isMBBChanged |= convertInstr(MBB, MI, *NextMI, ErasedVVMI);
      I = ErasedVVMI ? AfterNextI : NextI;
      continue;
    }

    I = NextI;
  }
  isMBBChanged |= removeDeadScalarizationInstrs(MBB);
  return isMBBChanged;
}

const TargetRegisterClass *
VentusVVInstrConversion::getRegClass(Register Reg) const {
  if (!Reg)
    return nullptr;
  if (Reg.isVirtual())
    return MR->getRegClass(Reg);
  return MRI->getPhysRegClass(Reg.asMCReg());
}

bool VentusVVInstrConversion::lowerFloatVFInstr(MachineBasicBlock &MBB,
                                                MachineInstr &MI) {
  unsigned VVOpcode = 0;
  bool Reverse = false;
  switch (MI.getOpcode()) {
  default:
    return false;
  case RISCV::VFADD_VF:
    VVOpcode = RISCV::VFADD_VV;
    break;
  case RISCV::VFMUL_VF:
    VVOpcode = RISCV::VFMUL_VV;
    break;
  case RISCV::VFSUB_VF:
    VVOpcode = RISCV::VFSUB_VV;
    break;
  case RISCV::VFDIV_VF:
    VVOpcode = RISCV::VFDIV_VV;
    break;
  case RISCV::VFRSUB_VF:
    VVOpcode = RISCV::VFSUB_VV;
    Reverse = true;
    break;
  case RISCV::VFRDIV_VF:
    VVOpcode = RISCV::VFDIV_VV;
    Reverse = true;
    break;
  }

  if (MI.getNumOperands() < 3 || !MI.getOperand(0).isReg() ||
      !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
    return false;

  Register Dst = MI.getOperand(0).getReg();
  Register Vec = MI.getOperand(1).getReg();
  Register Scalar = MI.getOperand(2).getReg();
  const TargetRegisterClass *VecRC = getRegClass(Vec);
  const TargetRegisterClass *ScalarRC = getRegClass(Scalar);
  if (!VecRC || !ScalarRC || !RISCVRegisterInfo::isVGPRClass(VecRC))
    return false;

  DebugLoc DL = MI.getDebugLoc();
  Register ScalarVec;
  if (Scalar.isVirtual()) {
    MachineInstr *ScalarDef = MR->getVRegDef(Scalar);
    if (ScalarDef && ScalarDef->getOpcode() == RISCV::COPY &&
        ScalarDef->getOperand(1).isReg()) {
      Register CopySrc = ScalarDef->getOperand(1).getReg();
      const TargetRegisterClass *CopySrcRC = getRegClass(CopySrc);
      if (CopySrcRC && RISCVRegisterInfo::isVGPRClass(CopySrcRC))
        ScalarVec = CopySrc;
    }
  }

  if (!ScalarVec) {
    bool IsUniformScalar = RISCVRegisterInfo::isSGPRClass(ScalarRC) ||
                           RISCVRegisterInfo::isFPRClass(ScalarRC);
    if (!IsUniformScalar)
      return false;

    unsigned MoveOpcode = RISCVRegisterInfo::isFPRClass(ScalarRC)
                              ? RISCV::VFMV_V_F
                              : RISCV::VMV_V_X;

    if (RISCVRegisterInfo::isFPRClass(ScalarRC) && Scalar.isVirtual()) {
      MachineInstr *FCvt = MR->getVRegDef(Scalar);
      unsigned VecCvtOpcode = 0;
      if (FCvt && FCvt->getOpcode() == RISCV::FCVT_S_WU)
        VecCvtOpcode = RISCV::VFCVT_F_XU_V;
      else if (FCvt && FCvt->getOpcode() == RISCV::FCVT_S_W)
        VecCvtOpcode = RISCV::VFCVT_F_X_V;

      if (VecCvtOpcode && FCvt->getNumOperands() > 1 &&
          FCvt->getOperand(1).isReg()) {
        Register IntScalar = FCvt->getOperand(1).getReg();
        MachineInstr *Copy = IntScalar.isVirtual() ? MR->getVRegDef(IntScalar)
                                                   : nullptr;
        if (Copy && Copy->getOpcode() == RISCV::COPY &&
            Copy->getNumOperands() > 1 && Copy->getOperand(1).isReg()) {
          Register CopySrc = Copy->getOperand(1).getReg();
          const TargetRegisterClass *CopySrcRC = getRegClass(CopySrc);
          if (CopySrcRC && RISCVRegisterInfo::isVGPRClass(CopySrcRC)) {
            ScalarVec = MR->createVirtualRegister(&RISCV::VGPRNoV0RegClass);
            BuildMI(MBB, MI, DL, TII->get(VecCvtOpcode), ScalarVec)
                .addReg(CopySrc);
          }
        }
      }
    }

    if (!ScalarVec) {
      // Ventus VF reads uniform FP bits from GPRs. Keep that legal operand,
      // but only after repairing any lane-varying scalar conversion above.
      if (RISCVRegisterInfo::isSGPRClass(ScalarRC) ||
          RISCV::GPRF32RegClass.hasSubClassEq(ScalarRC))
        return false;

      ScalarVec = MR->createVirtualRegister(&RISCV::VGPRNoV0RegClass);
      BuildMI(MBB, MI, DL, TII->get(MoveOpcode), ScalarVec).addReg(Scalar);
    }
  }

  MachineInstrBuilder NewMI = BuildMI(MBB, MI, DL, TII->get(VVOpcode), Dst);
  if (Reverse)
    NewMI.addReg(ScalarVec).addReg(Vec);
  else
    NewMI.addReg(Vec).addReg(ScalarVec);
  MI.eraseFromParent();
  return true;
}

bool VentusVVInstrConversion::removeDeadScalarizationInstrs(
    MachineBasicBlock &MBB) {
  bool Changed = false;
  bool Removed;
  do {
    Removed = false;
    for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstr &MI = *I++;
      switch (MI.getOpcode()) {
      default:
        continue;
      case RISCV::COPY:
      case RISCV::FCVT_S_W:
      case RISCV::FCVT_S_WU:
        break;
      }

      if (MI.getNumOperands() == 0 || !MI.getOperand(0).isReg())
        continue;

      Register Dst = MI.getOperand(0).getReg();
      if (!Dst.isVirtual() || !MR->use_empty(Dst))
        continue;

      MI.eraseFromParent();
      Removed = true;
      Changed = true;
    }
  } while (Removed);
  return Changed;
}

/// This function tries to convert
///     vmv.s.x v2, a0
///     vadd.vv v0, v0, v2
/// into
///     vadd.vx v0, v0, a0
/// *****************************************************
///     vmv.s.x v2, a0
///     vmadd.vv v0, v2, v1
/// into
///     vmadd.vx v0, a0, v1
/// VV to VF conversion follows the same routine
/// TODO: vrsub has VX and VI version, need to deal with this specifically?
bool VentusVVInstrConversion::convertInstr(MachineBasicBlock &MBB,
                                           MachineInstr &CopyMI,
                                           MachineInstr &VVMI,
                                           bool &ErasedVVMI) {
  auto Opcode = VV2VXOpcodeMap.find(VVMI.getOpcode());
  unsigned NumOps = VVMI.getNumExplicitOperands();
  if (Opcode == VV2VXOpcodeMap.end() || (NumOps != 3 && NumOps != 4))
    return false;

  Register Broadcast = CopyMI.getOperand(0).getReg();
  // Decide before changing anything. In particular, a broadcast addend of
  // VMADD cannot be folded into its scalar multiplier operand.
  if (NumOps == 4 && Broadcast == VVMI.getOperand(3).getReg())
    return false;
  unsigned VectorOp = 1;
  if (Broadcast != VVMI.getOperand(2).getReg()) {
    if (!isVALUCommutableInstr(VVMI) ||
        Broadcast != VVMI.getOperand(1).getReg())
      return false;
    VectorOp = 2;
  }

  Register Scalar = CopyMI.getOperand(1).getReg();
  // The COPY may remain live. Adding a later scalar use invalidates its
  // former last-use marker, including when Scalar is also another operand.
  MR->clearKillFlags(Scalar);
  auto NewMI = BuildMI(MBB, VVMI, VVMI.getDebugLoc(), TII->get(Opcode->second))
                   .add(VVMI.getOperand(0))
                   .add(VVMI.getOperand(VectorOp))
                   .addReg(Scalar);
  if (NumOps == 4)
    NewMI.add(VVMI.getOperand(3));
  NewMI.setMIFlags(VVMI.getFlags());
  NewMI.copyImplicitOps(VVMI);
  VVMI.eraseFromParent();
  ErasedVVMI = true;
  return true;
}

/// FIXME: we also can add attribute in VentusInstrInfoV.td file, but changes
/// are very trivial which can happen in many separated places, for now we use
/// enum to accomplish our purpose
/// In ventus : V+X = X+V, V*X=X*V
bool VentusVVInstrConversion::isVALUCommutableInstr(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    return false;
  case RISCV::VADD_VV:
  case RISCV::VMUL_VV:
  // case RISCV::VFADD_VV:
  // case RISCV::VFMUL_VV:
  case RISCV::VMADD_VV:
  // case RISCV::VFMADD_VV:
  case RISCV::VMULH_VV:
  case RISCV::VMULHU_VV:
    return true;
  };
}

/// Instruction shall be like this: %1:vgpr = COPY %2:gpr
bool VentusVVInstrConversion::isGPR2VGPRCopy(MachineInstr &MI) {
  return MI.getOpcode() == RISCV::COPY &&
         MRI->isSGPRReg(*MR, MI.getOperand(1).getReg()) &&
         !MRI->isSGPRReg(*MR, MI.getOperand(0).getReg());
}
} // end of anonymous namespace

INITIALIZE_PASS(VentusVVInstrConversion, "ventus-VV-instructions-conversion",
                VENTUS_VV_INSTRUCTION_CONVRSION, false, false)

namespace llvm {
FunctionPass *createVentusVVInstrConversionPass() {
  return new VentusVVInstrConversion();
}
} // end of namespace llvm
