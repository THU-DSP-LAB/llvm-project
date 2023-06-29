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
  const MachineRegisterInfo *MR;

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
                    MachineInstr &VVMI);

  bool swapRegOperands(MachineInstr &MI);

  bool isGPR2VGPRCopy(MachineInstr &MI);
};

char VentusVVInstrConversion::ID = 0;

/// Swap register operands of instruction such as
/// vadd.vv v0, v2, v1
/// into
/// vadd.vv v0, v1, v2
bool VentusVVInstrConversion::swapRegOperands(MachineInstr &MI) {
  MachineOperand &MO1 = MI.getOperand(1);
  MachineOperand &MO2 = MI.getOperand(2);
  assert((MO1.isReg() && MO2.isReg()) && "Operand is not register");
  Register Reg1 = MO1.getReg();
  Register Reg2 = MO2.getReg();
  MO1.setReg(Reg2);
  MO2.setReg(Reg1);
  return true;
}

bool VentusVVInstrConversion::runOnMachineFunction(MachineFunction &MF) {
  bool isChanged = false;
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  MRI = MF.getSubtarget<RISCVSubtarget>().getRegisterInfo();
  MR = &MF.getRegInfo();
  for (auto &MBB : MF)
    isChanged |= runOnMachineBasicBlock(MBB);
  return isChanged;
}

bool VentusVVInstrConversion::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool isMBBChanged = false;
  for (auto &MI : MBB) {
    MachineInstr *NextMI = MI.getNextNode();
    // Check RISCV::COPY instructions' format and its next instruction's format
    if (isGPR2VGPRCopy(MI) && NextMI && isVVALUInstruction(*NextMI)) {
      // When met here, we can ensure the coding logic goes to the conversion
      isMBBChanged |= convertInstr(MBB, MI, *NextMI);
    }
  }
  return isMBBChanged;
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
                                           MachineInstr &VVMI) {
  bool isMBBChanged = false;
  if (isVALUCommutableInstr(VVMI) &&
      CopyMI.getOperand(0).getReg() != VVMI.getOperand(2).getReg())
    isMBBChanged |= swapRegOperands(VVMI);
  // Other incommutable instructions check
  if (CopyMI.getOperand(0).getReg() != VVMI.getOperand(2).getReg())
    return isMBBChanged;
  if(VV2VXOpcodeMap.find(VVMI.getOpcode()) == VV2VXOpcodeMap.end())
    return isMBBChanged;
  unsigned NewOpcode = VV2VXOpcodeMap[VVMI.getOpcode()];
  assert(NewOpcode && "No VV instruction reflection to VX/VF "
                      "instruction, please check the mapping");
  Register Dst = VVMI.getOperand(0).getReg();
  DebugLoc DL = VVMI.getDebugLoc();
  if (VVMI.getNumExplicitOperands() == 3) {
    BuildMI(MBB, VVMI, DL, TII->get(NewOpcode), Dst)
        .addReg(VVMI.getOperand(1).getReg())
        .addReg(CopyMI.getOperand(1).getReg());
    VVMI.eraseFromParent();
  }
  // Three-operands VV ALU instruction conversion
  else if (VVMI.getNumExplicitOperands() == 4 &&
           CopyMI.getOperand(0).getReg() != VVMI.getOperand(3).getReg()) {
    BuildMI(MBB, VVMI, DL, TII->get(NewOpcode), VVMI.getOperand(0).getReg())
        .addReg(VVMI.getOperand(1).getReg())
        .addReg(CopyMI.getOperand(1).getReg())
        .addReg(VVMI.getOperand(3).getReg());
    VVMI.eraseFromParent();
  }
  // FIXME: maybe we need to take other unsupported instructions into
  // consideration, so we add an else statement here and return false directly
  else
    return isMBBChanged;
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
  case RISCV::VMULHSU_VV:
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