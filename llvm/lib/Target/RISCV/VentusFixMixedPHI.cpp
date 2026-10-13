//===-- VentusFixMixedPHI.cpp - Fix mixed register class PHI nodes -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This pass fixes PHI nodes with VGPR results but GPR/GPRF32/FPR inputs.
/// It inserts VMV.V.X instructions in predecessor blocks to convert 
/// non-VGPR inputs to VGPR, making the PHI nodes legal.
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "VentusRegisterDomain.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-fix-mixed-phi"

namespace {

class VentusFixMixedPHI : public MachineFunctionPass {
private:
  MachineRegisterInfo *MRI;
  const RISCVRegisterInfo *TRI;
  const RISCVInstrInfo *TII;

public:
  static char ID;

  VentusFixMixedPHI() : MachineFunctionPass(ID) {
    initializeVentusFixMixedPHIPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool promoteDivergentPHI(MachineInstr &MI);
  bool removeInvalidVGPRBroadcasts(MachineFunction &MF);
  bool processPHINode(MachineInstr &MI);

  StringRef getPassName() const override {
    return "Ventus Fix Mixed PHI";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

INITIALIZE_PASS(VentusFixMixedPHI, DEBUG_TYPE,
                "Ventus Fix Mixed PHI", false, false)

char VentusFixMixedPHI::ID = 0;

FunctionPass *llvm::createVentusFixMixedPHIPass() {
  return new VentusFixMixedPHI();
}

static unsigned getBroadcastOpcodeOrReport(const MachineFunction &MF,
                                           const RISCVRegisterInfo &TRI,
                                           const TargetRegisterClass *SrcRC) {
  if (SrcRC == &RISCV::GPRRegClass)
    return RISCV::VMV_V_X;

  if (SrcRC == &RISCV::GPRF32RegClass)
    return RISCV::VFMV_V_F;

  report_fatal_error(
      Twine("VentusFixMixedPHI unsupported scalar-to-VGPR PHI broadcast in '") +
      MF.getName() + "': source class " + TRI.getRegClassName(SrcRC));
}

static Register copiedVGPRSource(const MachineRegisterInfo &MRI,
                                 Register Reg) {
  if (!Reg.isVirtual())
    return Register();

  const MachineInstr *Def = MRI.getVRegDef(Reg);
  if (!Def || !Def->isCopy() || Def->getNumOperands() < 2 ||
      !Def->getOperand(1).isReg())
    return Register();

  Register Source = Def->getOperand(1).getReg();
  if (!Source.isVirtual() || !isVentusVGPRClass(MRI.getRegClass(Source)))
    return Register();

  return Source;
}

bool VentusFixMixedPHI::promoteDivergentPHI(MachineInstr &MI) {
  Register PHIRes = MI.getOperand(0).getReg();
  const TargetRegisterClass *ResultRC = MRI->getRegClass(PHIRes);
  if (!isGPRLikeScalarClass(ResultRC))
    return false;

  bool HasVGPRInput = false;
  for (unsigned I = 1, N = MI.getNumOperands(); I != N; I += 2) {
    MachineOperand &RegOp = MI.getOperand(I);
    if (!RegOp.isReg() || !RegOp.getReg().isVirtual())
      continue;

    Register Source = RegOp.getReg();
    if (isVentusVGPRClass(MRI->getRegClass(Source)) ||
        copiedVGPRSource(*MRI, Source)) {
      HasVGPRInput = true;
      break;
    }
  }

  if (!HasVGPRInput)
    return false;

  /*
   * A divergent boolean PHI can arrive here as a scalar i32 PHI even though
   * its incoming comparisons already live in VGPRs.  Retain its lane-private
   * representation and bypass the invalid VGPR-to-GPR edge copies.
   */
  MRI->setRegClass(PHIRes, &RISCV::VGPRRegClass);
  for (unsigned I = 1, N = MI.getNumOperands(); I != N; I += 2) {
    MachineOperand &RegOp = MI.getOperand(I);
    if (!RegOp.isReg())
      continue;
    if (Register Source = copiedVGPRSource(*MRI, RegOp.getReg()))
      RegOp.setReg(Source);
  }

  return true;
}

bool VentusFixMixedPHI::removeInvalidVGPRBroadcasts(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto It = MBB.begin(), End = MBB.end(); It != End;) {
      MachineInstr &MI = *It++;
      if (MI.getOpcode() != RISCV::VMV_V_X || MI.getNumOperands() < 2 ||
          !MI.getOperand(0).isReg() || !MI.getOperand(1).isReg())
        continue;

      Register Dst = MI.getOperand(0).getReg();
      Register Src = MI.getOperand(1).getReg();
      if (!Dst.isVirtual() || !Src.isVirtual() ||
          !isVentusVGPRClass(MRI->getRegClass(Dst)) ||
          !isVentusVGPRClass(MRI->getRegClass(Src)))
        continue;

      MRI->replaceRegWith(Dst, Src);
      MI.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

bool VentusFixMixedPHI::processPHINode(MachineInstr &MI) {
  Register PHIRes = MI.getOperand(0).getReg();
  const TargetRegisterClass *RC = MRI->getRegClass(PHIRes);
  bool Changed = false;

  LLVM_DEBUG(dbgs() << "Processing PHI: " << MI << "\n");
  LLVM_DEBUG(dbgs() << "  PHI result register class: "
                    << TRI->getRegClassName(RC) << "\n");

  for (unsigned I = 1, N = MI.getNumOperands(); I != N; I += 2) {
    MachineOperand &RegOp = MI.getOperand(I);
    if (!RegOp.isReg() || !RegOp.getReg().isVirtual())
      continue;

    Register SrcReg = RegOp.getReg();
    const TargetRegisterClass *SrcRC = MRI->getRegClass(SrcReg);
    LLVM_DEBUG(dbgs() << "  PHI input " << printReg(SrcReg, TRI)
                      << " class: " << TRI->getRegClassName(SrcRC) << "\n");

    if (!isLegalCopyDirection(RC, SrcRC)) {
      report_fatal_error(
          Twine("VentusFixMixedPHI refuses implicit VGPR to scalar PHI in '") +
              MI.getMF()->getName() + "': " + TRI->getRegClassName(SrcRC) +
              " -> " + TRI->getRegClassName(RC));
    }

    if (!requiresExplicitBroadcast(RC, SrcRC))
      continue;

    MachineBasicBlock *PredBB = MI.getOperand(I + 1).getMBB();
    MachineBasicBlock::iterator InsertPos = PredBB->getFirstTerminator();
    DebugLoc DL =
        InsertPos != PredBB->end() ? InsertPos->getDebugLoc() : MI.getDebugLoc();
    Register NewVGPR = MRI->createVirtualRegister(&RISCV::VGPRRegClass);
    unsigned BroadcastOpcode =
        getBroadcastOpcodeOrReport(*MI.getMF(), *TRI, SrcRC);

    BuildMI(*PredBB, InsertPos, DL, TII->get(BroadcastOpcode), NewVGPR)
        .addReg(SrcReg);
    RegOp.setReg(NewVGPR);
    Changed = true;

    LLVM_DEBUG(dbgs() << "  Inserted explicit broadcast from "
                      << printReg(SrcReg, TRI) << " to "
                      << printReg(NewVGPR, TRI) << " in "
                      << printMBBReference(*PredBB) << "\n");
  }

  return Changed;
}

bool VentusFixMixedPHI::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.isVentusGPGPU())
    return false;
  
  MRI = &MF.getRegInfo();
  TRI = ST.getRegisterInfo();
  TII = ST.getInstrInfo();
  
  bool Changed = false;
  SmallVector<MachineInstr*, 16> PHINodes;
  
  LLVM_DEBUG(dbgs() << "Running VentusFixMixedPHI on " << MF.getName() << "\n");
  
  // Collect all PHI nodes
  for (MachineFunction::iterator BI = MF.begin(), BE = MF.end(); BI != BE; ++BI) {
    MachineBasicBlock *MBB = &*BI;
    for (MachineBasicBlock::iterator I = MBB->begin(), E = MBB->end(); I != E; ++I) {
      MachineInstr &MI = *I;
      if (MI.isPHI()) {
        PHINodes.push_back(&MI);
      }
    }
  }
  
  LLVM_DEBUG(dbgs() << "Found " << PHINodes.size() << " PHI nodes\n");
  
  // Promote the PHI result before fixing its inputs.  This makes a boolean
  // PHI with divergent comparisons use the same lane-private domain as those
  // comparisons instead of forcing an invalid VGPR-to-GPR copy.
  for (MachineInstr *MI : PHINodes)
    Changed |= promoteDivergentPHI(*MI);

  // Process all PHI nodes
  for (MachineInstr *MI : PHINodes) {
    if (processPHINode(*MI)) {
      Changed = true;
    }
  }

  Changed |= removeInvalidVGPRBroadcasts(MF);
  
  return Changed;
}
