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

bool VentusFixMixedPHI::processPHINode(MachineInstr &MI) {
  Register PHIRes = MI.getOperand(0).getReg();
  const TargetRegisterClass *RC = MRI->getRegClass(PHIRes);
  
  LLVM_DEBUG(dbgs() << "Processing PHI: " << MI << "\n");
  LLVM_DEBUG(dbgs() << "  PHI result register class: " << TRI->getRegClassName(RC) << "\n");
  
  // Only process PHI nodes where result is VGPR
  if (!RISCVRegisterInfo::isVGPRClass(RC)) {
    LLVM_DEBUG(dbgs() << "  PHI result is not VGPR, skipping\n");
    return false;
  }
  
  bool HasGPRInput = false;
  
  // Check if PHI has GPR inputs
  for (unsigned I = 1, N = MI.getNumOperands(); I != N; I += 2) {
    const MachineOperand &RegOp = MI.getOperand(I);
    if (RegOp.isReg() && RegOp.getReg().isVirtual()) {
      Register SrcReg = RegOp.getReg();
      const TargetRegisterClass *SrcRC = MRI->getRegClass(SrcReg);
      LLVM_DEBUG(dbgs() << "  PHI input " << printReg(SrcReg, TRI) 
                       << " class: " << TRI->getRegClassName(SrcRC) << "\n");
      if (RISCVRegisterInfo::isSGPRClass(SrcRC) || RISCVRegisterInfo::isFPRClass(SrcRC) ||
          (SrcRC && std::string(TRI->getRegClassName(SrcRC)) == "GPRF32")) {
        HasGPRInput = true;
        LLVM_DEBUG(dbgs() << "  Found GPR/FPR/GPRF32 input!\n");
      }
    }
  }
  
  // If PHI result is VGPR but has GPR inputs, we need to fix it
  if (HasGPRInput) {
    LLVM_DEBUG(dbgs() << "Fixing PHI node with VGPR result and GPR/FPR inputs: " << MI << "\n");
    
    const TargetRegisterClass *VGPRClass = &RISCV::VGPRRegClass;
    
    // Insert COPY instructions from GPR to VGPR for each GPR PHI operand
    for (unsigned I = 1, N = MI.getNumOperands(); I != N; I += 2) {
      MachineOperand &RegOp = MI.getOperand(I);
      MachineBasicBlock *PredBB = MI.getOperand(I + 1).getMBB();
      
      if (!RegOp.isReg() || !RegOp.getReg().isVirtual())
        continue;
      
      Register SrcReg = RegOp.getReg();
      const TargetRegisterClass *SrcRC = MRI->getRegClass(SrcReg);
      
      // If source is GPR-based register (GPR, FPR, GPRF32), convert it to VGPR
      if (RISCVRegisterInfo::isSGPRClass(SrcRC) || RISCVRegisterInfo::isFPRClass(SrcRC) ||
          (SrcRC && std::string(TRI->getRegClassName(SrcRC)) == "GPRF32")) {
        // Create new VGPR register
        Register NewVGPR = MRI->createVirtualRegister(VGPRClass);
        
        // Find insertion point in predecessor block (before terminators)
        MachineBasicBlock::iterator InsertPos = PredBB->getFirstTerminator();
        
        // Get debug location - use PHI's location if insertion point is invalid
        DebugLoc DL;
        if (InsertPos != PredBB->end()) {
          DL = InsertPos->getDebugLoc();
        } else {
          DL = MI.getDebugLoc();
        }
        
        // Detect immediate values
        MachineInstr *SrcDef = MRI->getVRegDef(SrcReg);
        bool IsImmediate = false;
        int64_t ImmValue = 0;
        
        if (SrcDef) {
          if (SrcDef->isMoveImmediate()) {
            IsImmediate = true;
            for (const auto &MO : SrcDef->operands()) {
              if (MO.isImm()) {
                ImmValue = MO.getImm();
                break;
              }
            }
          }
          // Check ADDI patterns (X0 and LUI+ADDI)
          else if (SrcDef->getOpcode() == RISCV::ADDI) {
            const MachineOperand &SrcOp = SrcDef->getOperand(1);
            if (SrcOp.isReg() && SrcOp.getReg() == RISCV::X0) {
              IsImmediate = true;
              ImmValue = SrcDef->getOperand(2).getImm();
            } else if (SrcOp.isReg() && SrcOp.getReg().isVirtual()) {
              MachineInstr *LUIDef = MRI->getVRegDef(SrcOp.getReg());
              if (LUIDef && LUIDef->getOpcode() == RISCV::LUI) {
                IsImmediate = true;
                int64_t LUIImm = LUIDef->getOperand(1).getImm();
                int64_t ADDIImm = SrcDef->getOperand(2).getImm();
                ImmValue = (LUIImm << 12) + ADDIImm;
                
                LLVM_DEBUG(dbgs() << "  LUI+ADDI: " << LUIImm 
                                 << " + " << ADDIImm << " = " << ImmValue << "\n");
              }
            }
          }
        }
        
        if (IsImmediate) {
          LLVM_DEBUG(dbgs() << "  Processing immediate " << ImmValue << "\n");
          
          Register SourceGPR;
          
          if (ImmValue < -2048 || ImmValue > 2047) {
            // Generate LUI+ADDI for large constants
            const TargetRegisterClass *GPRClass = &RISCV::GPRRegClass;
            int64_t LUIImm = (ImmValue + 0x800) >> 12;
            int64_t ADDIImm = ImmValue - (LUIImm << 12);
            
            // Check for existing constant in predecessor block
            Register ExistingGPR = Register();
            for (auto &PredMI : *PredBB) {
              if (PredMI.getOpcode() == RISCV::ADDI) {
                const MachineOperand &SrcOp = PredMI.getOperand(1);
                if (SrcOp.isReg() && SrcOp.getReg().isVirtual()) {
                  MachineInstr *LUIDef = MRI->getVRegDef(SrcOp.getReg());
                  if (LUIDef && LUIDef->getOpcode() == RISCV::LUI) {
                    int64_t ExistingLUIImm = LUIDef->getOperand(1).getImm();
                    int64_t ExistingADDIImm = PredMI.getOperand(2).getImm();
                    int64_t ExistingValue = (ExistingLUIImm << 12) + ExistingADDIImm;
                    if (ExistingValue == ImmValue) {
                      ExistingGPR = PredMI.getOperand(0).getReg();
                      LLVM_DEBUG(dbgs() << "  Found existing LUI+ADDI sequence: " 
                                       << printReg(ExistingGPR, TRI) << "\n");
                      break;
                    }
                  }
                }
              }
            }
            
            if (ExistingGPR.isValid()) {
              SourceGPR = ExistingGPR;
            } else {
              // Generate new LUI+ADDI sequence
              Register LUIReg = MRI->createVirtualRegister(GPRClass);
              SourceGPR = MRI->createVirtualRegister(GPRClass);
              
              // Generate LUI
              BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::LUI), LUIReg)
                  .addImm(LUIImm);
              
              // Generate ADDI
              BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::ADDI), SourceGPR)
                  .addReg(LUIReg)
                  .addImm(ADDIImm);
                  
              LLVM_DEBUG(dbgs() << "  Generated LUI+ADDI sequence for " << ImmValue 
                               << " in " << printReg(SourceGPR, TRI) << "\n");
            }
          } else {
            // Small immediate, can use ADDI with X0
            // Look for existing immediate load in predecessor block
            Register ExistingGPR = Register();
            for (auto &PredMI : *PredBB) {
              // Check for ADDI $x0, immediate with same value
              if (PredMI.getOpcode() == RISCV::ADDI && 
                  PredMI.getOperand(1).isReg() && 
                  PredMI.getOperand(1).getReg() == RISCV::X0 &&
                  PredMI.getOperand(2).isImm() &&
                  PredMI.getOperand(2).getImm() == ImmValue) {
                // Found existing load with same immediate value
                ExistingGPR = PredMI.getOperand(0).getReg();
                LLVM_DEBUG(dbgs() << "  Found existing immediate load: " 
                                 << printReg(ExistingGPR, TRI) << "\n");
                break;
              }
            }
            
            if (ExistingGPR.isValid()) {
              // Reuse existing GPR with the immediate value
              SourceGPR = ExistingGPR;
              LLVM_DEBUG(dbgs() << "  Reusing existing GPR " << printReg(SourceGPR, TRI) 
                               << " with immediate " << ImmValue << "\n");
            } else {
              // Create a temporary GPR for this immediate only if none exists
              const TargetRegisterClass *GPRClass = &RISCV::GPRRegClass;
              SourceGPR = MRI->createVirtualRegister(GPRClass);
              
              // Generate immediate in GPR using ADDI (li instruction)
              BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::ADDI), SourceGPR)
                  .addReg(RISCV::X0)
                  .addImm(ImmValue);
                  
              LLVM_DEBUG(dbgs() << "  Created new GPR " << printReg(SourceGPR, TRI) 
                               << " for immediate " << ImmValue << "\n");
            }
          }
          
          // Convert to VGPR using VMV.V.X
          BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::VMV_V_X), NewVGPR)
              .addReg(SourceGPR);
              
          LLVM_DEBUG(dbgs() << "  Inserted VMV.V.X from " << printReg(SourceGPR, TRI)
                           << " to " << printReg(NewVGPR, TRI) << "\n");
        } else {
          // Use VMV.V.X for non-immediate GPR-based registers
          bool SrcAvailable = false;
          if (SrcDef) {
            MachineBasicBlock *SrcDefBB = SrcDef->getParent();
            if (SrcDefBB == PredBB || 
                std::find(PredBB->pred_begin(), PredBB->pred_end(), SrcDefBB) != PredBB->pred_end()) {
              SrcAvailable = true;
            }
          }
          
          if (SrcAvailable) {
            // Use VMV.V.X for all GPR-based registers (including GPRF32)
            BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::VMV_V_X), NewVGPR)
                .addReg(SrcReg);
            LLVM_DEBUG(dbgs() << "  Inserted VMV.V.X from " << printReg(SrcReg, TRI) 
                             << " to " << printReg(NewVGPR, TRI) << "\n");
          } else {
            // Source register may not be available in this predecessor block
            // This is a more complex case that might need special handling
            LLVM_DEBUG(dbgs() << "  Warning: Source register " << printReg(SrcReg, TRI)
                             << " may not be available in " << printMBBReference(*PredBB) << "\n");
            // For now, still insert VMV.V.X but this might need more sophisticated handling
            BuildMI(*PredBB, InsertPos, DL, TII->get(RISCV::VMV_V_X), NewVGPR)
                .addReg(SrcReg);
          }
        }
        
        // Update PHI operand to use the new VGPR
        RegOp.setReg(NewVGPR);
        
        LLVM_DEBUG(dbgs() << "  Updated PHI operand from " << printReg(SrcReg, TRI)
                         << " to " << printReg(NewVGPR, TRI) << " in "
                         << printMBBReference(*PredBB) << "\n");
      }
    }
    return true; // We made changes
  }
  return false; // No changes made
}

bool VentusFixMixedPHI::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  
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
  
  // Process all PHI nodes
  for (MachineInstr *MI : PHINodes) {
    if (processPHINode(*MI)) {
      Changed = true;
    }
  }
  
  return Changed;
} 
