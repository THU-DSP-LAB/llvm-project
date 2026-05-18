//===-- VentusRegDomainVerifier.cpp - Ventus register domain checks -------===//
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
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "ventus-reg-domain-verifier"

namespace {

class VentusRegDomainVerifier : public MachineFunctionPass {
public:
  static char ID;

  VentusRegDomainVerifier() : MachineFunctionPass(ID) {
    initializeVentusRegDomainVerifierPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Ventus Register Domain Verifier";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // namespace

INITIALIZE_PASS(VentusRegDomainVerifier, DEBUG_TYPE,
                "Ventus Register Domain Verifier", false, true)

char VentusRegDomainVerifier::ID = 0;

FunctionPass *llvm::createVentusRegDomainVerifierPass() {
  return new VentusRegDomainVerifier();
}

static const TargetRegisterClass *
getRegClass(const RISCVRegisterInfo &TRI, const MachineRegisterInfo &MRI,
            Register Reg) {
  if (!Reg)
    return nullptr;
  if (Reg.isVirtual())
    return MRI.getRegClass(Reg);
  return TRI.getPhysRegClass(Reg.asMCReg());
}

static StringRef getRegClassName(const RISCVRegisterInfo &TRI,
                                 const TargetRegisterClass *RC) {
  if (!RC)
    return "<unknown>";
  return TRI.getRegClassName(RC);
}

static void reportDomainError(const MachineFunction &MF,
                              const MachineBasicBlock &MBB,
                              const MachineInstr &MI, const Twine &Message) {
  std::string Text;
  raw_string_ostream OS(Text);

  OS << "Ventus register domain verifier failed in function '" << MF.getName()
     << "', bb." << MBB.getNumber() << ": " << Message << "\n  ";
  MI.print(OS);
  OS.flush();

  report_fatal_error(StringRef(Text), false);
}

static void verifyCopy(const MachineFunction &MF, const MachineBasicBlock &MBB,
                       const MachineInstr &MI, const RISCVRegisterInfo &TRI,
                       const MachineRegisterInfo &MRI) {
  Register DstReg = MI.getOperand(0).getReg();
  Register SrcReg = MI.getOperand(1).getReg();
  const TargetRegisterClass *DstRC = getRegClass(TRI, MRI, DstReg);
  const TargetRegisterClass *SrcRC = getRegClass(TRI, MRI, SrcReg);

  if (isLegalCopyDirection(DstRC, SrcRC))
    return;

  reportDomainError(MF, MBB, MI,
                    Twine("illegal VGPR to scalar COPY: dst class ") +
                        getRegClassName(TRI, DstRC) + ", src class " +
                        getRegClassName(TRI, SrcRC));
}

static void verifyPHI(const MachineFunction &MF, const MachineBasicBlock &MBB,
                      const MachineInstr &MI, const RISCVRegisterInfo &TRI,
                      const MachineRegisterInfo &MRI) {
  Register DstReg = MI.getOperand(0).getReg();
  const TargetRegisterClass *DstRC = getRegClass(TRI, MRI, DstReg);

  for (unsigned I = 1, E = MI.getNumOperands(); I < E; I += 2) {
    const MachineOperand &SrcOp = MI.getOperand(I);
    if (!SrcOp.isReg())
      continue;

    const TargetRegisterClass *SrcRC = getRegClass(TRI, MRI, SrcOp.getReg());
    if (isLegalCopyDirection(DstRC, SrcRC))
      continue;

    reportDomainError(MF, MBB, MI,
                      Twine("illegal VGPR to scalar PHI input at operand ") +
                          Twine(I) + ": dst class " +
                          getRegClassName(TRI, DstRC) + ", src class " +
                          getRegClassName(TRI, SrcRC));
  }
}

static void verifyInstructionOperands(const MachineFunction &MF,
                                      const MachineBasicBlock &MBB,
                                      const MachineInstr &MI,
                                      const RISCVInstrInfo &TII,
                                      const RISCVRegisterInfo &TRI,
                                      const MachineRegisterInfo &MRI) {
  const MCInstrDesc &Desc = MI.getDesc();

  for (unsigned I = 0, E = MI.getNumOperands(); I != E; ++I) {
    const MachineOperand &MO = MI.getOperand(I);
    if (!MO.isReg() || !MO.getReg())
      continue;

    const TargetRegisterClass *ActualRC = getRegClass(TRI, MRI, MO.getReg());
    const TargetRegisterClass *ExpectedRC =
        I < Desc.getNumOperands() ? TII.getRegClass(Desc, I, &TRI, MF)
                                  : nullptr;

    if (ExpectedRC && isGPRLikeScalarClass(ExpectedRC) &&
        isVentusVGPRClass(ActualRC))
      reportDomainError(MF, MBB, MI,
                        Twine("scalar register constraint uses VGPR at "
                              "operand ") +
                            Twine(I) + ": expected class " +
                            getRegClassName(TRI, ExpectedRC) +
                            ", actual class " +
                            getRegClassName(TRI, ActualRC));

    if (ExpectedRC && isVentusVGPRClass(ExpectedRC) &&
        isGPRLikeScalarClass(ActualRC))
      reportDomainError(MF, MBB, MI,
                        Twine("vector register constraint uses scalar at "
                              "operand ") +
                            Twine(I) + ": expected class " +
                            getRegClassName(TRI, ExpectedRC) +
                            ", actual class " +
                            getRegClassName(TRI, ActualRC));
  }
}

bool VentusRegDomainVerifier::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.isVentusGPGPU())
    return false;

  const RISCVRegisterInfo &TRI = *ST.getRegisterInfo();
  const RISCVInstrInfo &TII = *ST.getInstrInfo();
  const MachineRegisterInfo &MRI = MF.getRegInfo();

  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB) {
      if (MI.isCopy())
        verifyCopy(MF, MBB, MI, TRI, MRI);
      else if (MI.isPHI())
        verifyPHI(MF, MBB, MI, TRI, MRI);
      else
        verifyInstructionOperands(MF, MBB, MI, TII, TRI, MRI);
    }
  }

  return false;
}
