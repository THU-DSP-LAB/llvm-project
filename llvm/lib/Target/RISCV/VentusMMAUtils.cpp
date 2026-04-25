//===-- VentusMMAUtils.cpp - Ventus MMA utilities -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VentusMMAUtils.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

bool llvm::hasVentusDedicatedRegextHandling(const RISCVInstrInfo &TII,
                                            unsigned Opcode) {
  StringRef Name = TII.getName(Opcode);
  return Name.starts_with("PseudoMMA_") || Name.starts_with("PseudoVFMA_");
}

static unsigned getVentusRegextOffsets(const MachineInstr &MI,
                                       const TargetRegisterInfo &TRI) {
  unsigned Offsets = 0;

  for (unsigned I = 0, E = MI.getNumOperands(); I != E; ++I) {
    const MachineOperand &Op = MI.getOperand(I);
    if (!Op.isReg() || Op.isImplicit() ||
        MI.getDesc().getOperandConstraint(I, MCOI::TIED_TO) != -1)
      continue;

    Register Reg = Op.getReg();
    if (!Reg)
      continue;

    uint16_t RegEncodingValue = TRI.getEncodingValue(Reg);
    if (RegEncodingValue <= 31)
      continue;

    int Pos = MI.getDesc().getOperandConstraint(I, MCOI::CUSTOM);
    assert(Pos != -1 && "register operand must have CUSTOM constraint");
    Offsets |= ((RegEncodingValue >> 5) & 0x7) << (3 * Pos);
  }

  return Offsets;
}

static void maybeEmitVentusRegext(MachineBasicBlock &MBB, MachineInstr &MI,
                                  const RISCVInstrInfo &TII,
                                  const TargetRegisterInfo &TRI) {
  unsigned Offsets = getVentusRegextOffsets(MI, TRI);
  if (!Offsets)
    return;

  BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(RISCV::REGEXT), RISCV::X0)
      .addReg(RISCV::X0)
      .addImm(Offsets);
}

static unsigned getVentusMMARealOpcode(unsigned PseudoOpc) {
#define VENTUS_MMA_REAL_OPCODE_CASE(Shape, Layout, Type)                       \
  case RISCV::PseudoMMA_##Shape##_##Layout##_##Type:                           \
    return RISCV::MMA_##Layout##_##Shape##_##Type;

  switch (PseudoOpc) {
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, ROW_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K16, COL_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, ROW_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K16, COL_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, ROW_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K16, COL_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, ROW_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_ROW, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_ROW, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_ROW, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_COL, F32_F16_F16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_COL, F16_F16_F16_F16)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K16, COL_COL, F32_BF16_BF16_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K8, ROW_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K8, ROW_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K8, COL_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N8K8, COL_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K8, ROW_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K8, ROW_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K8, COL_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N8K8, COL_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K8, ROW_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K8, ROW_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K8, COL_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M8N16K8, COL_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K8, ROW_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K8, ROW_COL, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K8, COL_ROW, F32_TF32_TF32_F32)
    VENTUS_MMA_REAL_OPCODE_CASE(M16N16K8, COL_COL, F32_TF32_TF32_F32)
  default:
    return 0;
  }

#undef VENTUS_MMA_REAL_OPCODE_CASE
}

static unsigned getVentusAccVOpRealOpcode(unsigned PseudoOpc) {
  switch (PseudoOpc) {
  default:
    return 0;
  case RISCV::PseudoVFMA_F16X2:
    return RISCV::VFMA_F16X2;
  case RISCV::PseudoVFMA_BF16X2:
    return RISCV::VFMA_BF16X2;
  }
}

static bool expandVentusMMAPseudo(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI,
                                  const RISCVInstrInfo &TII) {
  unsigned RealOpc = getVentusMMARealOpcode(MBBI->getOpcode());
  if (!RealOpc)
    return false;

  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  const MachineInstr &MI = *MBBI;
  const MCInstrDesc &MCID = TII.get(RealOpc);

  Register DstTuple = MI.getOperand(0).getReg();
  Register CTuple = MI.getOperand(1).getReg();
  Register ATuple = MI.getOperand(2).getReg();
  Register BTuple = MI.getOperand(3).getReg();

  auto BaseReg = [&](Register Reg) {
    Register Sub = TRI->getSubReg(Reg, RISCV::sub0);
    return Sub ? Sub : Reg;
  };

  MachineInstrBuilder MIB =
      BuildMI(MBB, MBBI, MI.getDebugLoc(), MCID, BaseReg(DstTuple))
          .addReg(BaseReg(CTuple))
          .addReg(BaseReg(ATuple))
          .addReg(BaseReg(BTuple));

  maybeEmitVentusRegext(MBB, *MIB, TII, *TRI);

  MIB.addReg(DstTuple, RegState::ImplicitDefine)
      .addReg(CTuple, RegState::Implicit)
      .addReg(ATuple, RegState::Implicit)
      .addReg(BTuple, RegState::Implicit);

  MBBI->eraseFromParent();
  return true;
}

static bool expandVentusAccVOpPseudo(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI,
                                     const RISCVInstrInfo &TII) {
  unsigned RealOpc = getVentusAccVOpRealOpcode(MBBI->getOpcode());
  if (!RealOpc)
    return false;

  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  const MachineInstr &MI = *MBBI;
  const MCInstrDesc &MCID = TII.get(RealOpc);

  Register DstReg = MI.getOperand(0).getReg();
  Register AccReg = MI.getOperand(1).getReg();
  Register Src1Reg = MI.getOperand(2).getReg();
  Register Src2Reg = MI.getOperand(3).getReg();

  MachineInstrBuilder MIB = BuildMI(MBB, MBBI, MI.getDebugLoc(), MCID, DstReg)
                                .addReg(Src1Reg)
                                .addReg(Src2Reg);

  maybeEmitVentusRegext(MBB, *MIB, TII, *TRI);

  MIB.addReg(DstReg, RegState::ImplicitDefine)
      .addReg(AccReg, RegState::Implicit);

  MBBI->eraseFromParent();
  return true;
}

bool llvm::expandVentusCustomPseudo(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const RISCVInstrInfo &TII) {
  return expandVentusMMAPseudo(MBB, MBBI, TII) ||
         expandVentusAccVOpPseudo(MBB, MBBI, TII);
}
