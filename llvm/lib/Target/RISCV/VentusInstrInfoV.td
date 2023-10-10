//===-- VentusInstrInfoV.td - Ventus 'V' instructions ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file describes the Ventus vALU instructions which borrows instruction
/// encoding from the standard RISC-V 'V' Vector extension, but with completely
/// different execution paradigm.
/// Ventus is a SIMT architecture and vALU instructions are basically scalar
/// instructions.
///
//===----------------------------------------------------------------------===//

include "VentusInstrFormatsV.td"

/// Generic pattern classes

// RVV VV, VX, VI instruction pattern class for integer binary operations
multiclass PatVXIBin<SDPatternOperator Op, list<RVInst> Insts,
                                        Operand optype = simm5> {

  def : Pat<(Op (XLenVT VGPR:$rs1), (XLenVT VGPR:$rs2)),
            (XLenVT (Insts[0] VGPR:$rs1, VGPR:$rs2))>;

  def : Pat<(Op (XLenVT VGPR:$rs1), GPR:$rs2),
            (XLenVT (Insts[1] VGPR:$rs1, GPR:$rs2))>;

  if !eq(!size(Insts), 3) then
    def : Pat<(XLenVT (Op (XLenVT VGPR:$rs1), optype:$imm)),
              (XLenVT (Insts[2] VGPR:$rs1, optype:$imm))>;
}
// RVV VV, VF, FV instruction pattern class for floating point binary operations
multiclass PatVFRBin<SDPatternOperator Op, list<RVInst> Insts> {
  def : Pat<(Op (f32 VGPR:$rs1), (f32 VGPR:$rs2)),
            (Insts[0] VGPR:$rs1, VGPR:$rs2)>;

  def : Pat<(Op (f32 VGPR:$rs1), GPRF32:$rs2),
            (Insts[1] VGPR:$rs1, GPRF32:$rs2)>;

  if !eq(!size(Insts), 3) then
    def : Pat<(Op (f32 VGPR:$rs1), GPRF32:$rs2),
              (Insts[2] GPRF32:$rs2, VGPR:$rs1)>;
}

multiclass DivergentPriLdPat<PatFrag LoadOp, RVInst Inst,
                                  list<ValueType> ValueTypes = [XLenVT, f32]> {
  foreach val = ValueTypes in {
    def : Pat<(val (DivergentPrivateLoadFrag<LoadOp>
                (PriAddrRegImm (XLenVT VGPR:$rs1), (simm11:$imm11)))),
                (Inst VGPR:$rs1, simm11:$imm11)>;
  }
}

multiclass DivergentPriStPat<PatFrag StoreOp, RVInst Inst,
                                  list<ValueType> ValueTypes = [XLenVT, f32]> {
  foreach val = ValueTypes in {
    def : Pat<(DivergentPrivateStoreFrag<StoreOp>
         (val VGPR:$vs3), (PriAddrRegImm (XLenVT VGPR:$rs1), (simm11:$imm11))),
         (Inst VGPR:$vs3, VGPR:$rs1, simm11:$imm11)>;
  }
}
class DivergentNonPriLdPat<PatFrag LoadOp, RVInst Inst>
 : Pat<(XLenVT (DivergentNonPrivateLoadFrag<LoadOp>
                (AddrRegReg GPR:$rs1, (XLenVT VGPR:$vs2)))),
                (Inst GPR:$rs1, VGPR:$vs2)>;

multiclass DivergentNonPriLdImmPat<PatFrag LoadOp, RVInst Inst,
                                list<ValueType> ValueTypes = [XLenVT, f32]> {
    foreach val = ValueTypes in {
      def : Pat<(val (DivergentNonPrivateLoadFrag<LoadOp>
                (AddrRegImm (XLenVT VGPR:$vs1), simm12:$imm12))),
                (Inst VGPR:$vs1, simm12:$imm12)>;
    }
}

class DivergentNonPriStPat<PatFrag StoreOp, RVInst Inst>
  : Pat<(DivergentNonPrivateStoreFrag<StoreOp>
         (XLenVT VGPR:$vs3), (AddrRegReg GPR:$rs1, (XLenVT VGPR:$vs2))),
         (Inst VGPR:$vs3, GPR:$rs1, VGPR:$vs2)>;

multiclass DivergentNonPriStImmPat<PatFrag StoreOp, RVInst Inst,
                                list<ValueType> ValueTypes = [XLenVT, f32]> {
  foreach val = ValueTypes in {
    def : Pat<(DivergentNonPrivateStoreFrag<StoreOp>
         (val VGPR:$vs3), (AddrRegImm (XLenVT VGPR:$vs1), simm12:$imm12)),
         (Inst VGPR:$vs3, VGPR:$vs1, simm12:$imm12)>;
  }
 }
// RVV instruction pattern class for float/interger ternary operations
// Dst is used to identify vx and vf
multiclass PatVXFTer<list<SDPatternOperator> Ops, ValueType Ty, DAGOperand Dst,
                                                          list<RVInst> Insts> {

  def : Pat<(Ops[0] (Ops[1] (Ty VGPR:$rs1), (Ty VGPR:$rs2)),
        (Ty VGPR:$rs3)), (Insts[0] VGPR:$rs1, VGPR:$rs2, VGPR:$rs3)>;

  def : Pat<(Ops[0] (Ops[1] (Ty VGPR:$rs1), (Ty Dst:$rs2)),
        (Ty VGPR:$rs3)), (Insts[1] VGPR:$rs1, Dst:$rs2, VGPR:$rs3)>;

  // For fneg operation
  // TODO: fneg operation is a little complicated
  if !eq(!size(Insts), 4) then {
    def : Pat<(Ops[0] (Ops[1] (fneg (Ty VGPR:$rs1)), (Ty VGPR:$rs2)),
          (Ty VGPR:$rs3)), (Insts[2] VGPR:$rs1, VGPR:$rs2, VGPR:$rs3)>;

    def : Pat<(Ops[0] (Ops[1] (fneg (Ty VGPR:$rs1)), (Ty Dst:$rs2)),
          (Ty VGPR:$rs3)), (Insts[3] VGPR:$rs1, Dst:$rs2, VGPR:$rs3)>;
  }

}

// Complex VV, VX, VI instruction pattern class for integers select operation
multiclass SleOpePatVXIBin<list<PatFrags> Ops, list<RVInst> Insts,
                                          Operand optype = simm5> {

  def : Pat<(XLenVT (Ops[0] (Ops[1] (XLenVT VGPR:$rs1), (XLenVT VGPR:$rs2)), 1)),
          (Insts[0] VGPR:$rs1, VGPR:$rs2)>;

  def : Pat<(XLenVT (Ops[0] (Ops[1] (XLenVT VGPR:$rs1), (XLenVT GPR:$rs2)), 1)),
          (Insts[1] VGPR:$rs1, GPR:$rs2)>;

  def : Pat<(XLenVT (Ops[0] (Ops[1] (XLenVT VGPR:$rs1), optype:$rs2), 1)),
          (Insts[2] VGPR:$rs1, optype:$rs2)>;
}

// Setcc pattern for interger operations
// FIXME: this pattern class can substitude the multiclass above
// class PatIntSetCC<list<DAGOperand> Ty, CondCode Cond, RVInst Inst>
//  : Pat<(setcc (XLenVT Ty[0]:$rs1), (XLenVT Ty[1]:$rs2), Cond), (i32 (Inst Ty[0]:$rs1, Ty[1]:$rs2))>;
// Setcc pattern for float operations
multiclass PatFloatSetCC<list<DAGOperand> Ty, list<CondCode> Conds, RVInst Inst>
{
  foreach Cond = Conds in {
    def : Pat<(DivergentTernaryFrag<any_fsetcc> (f32 Ty[0]:$rs1),
              (f32 Ty[1]:$rs2), Cond), (XLenVT (Inst Ty[0]:$rs1, Ty[1]:$rs2))>;
    def : Pat<(DivergentTernaryFrag<setcc> (f32 Ty[0]:$rs1), (f32 Ty[1]:$rs2),
              Cond),  (XLenVT (Inst Ty[0]:$rs1, Ty[1]:$rs2))>;
  }
}


// Float/integer type convert pattern
class PatFXConvert<PatFrag Frag, list<ValueType> Ty, RVInst Inst>
  : Pat<(Ty[0] (Frag (Ty[1] VGPR:$rs1))),(Inst VGPR:$rs1)>;

//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//

class VTypeIAsmOperand<int VTypeINum> : AsmOperandClass {
  let Name = "VTypeI" # VTypeINum;
  let ParserMethod = "parseVTypeI";
  let DiagnosticType = "InvalidVTypeI";
  let RenderMethod = "addVTypeIOperands";
}

class VTypeIOp<int VTypeINum> : Operand<XLenVT> {
  let ParserMatchClass = VTypeIAsmOperand<VTypeINum>;
  let PrintMethod = "printVTypeI";
  let DecoderMethod = "decodeUImmOperand<"#VTypeINum#">";
  let OperandType = "OPERAND_VTYPEI" # VTypeINum;
  let OperandNamespace = "RISCVOp";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<VTypeINum>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def VTypeIOp10 : VTypeIOp<10>;
def VTypeIOp11 : VTypeIOp<11>;

//===----------------------------------------------------------------------===//
// Scheduling definitions.
//===----------------------------------------------------------------------===//

class VLESched : Sched <[WriteVLDE, ReadVLDX]>;

class VSESched : Sched <[WriteVSTE, ReadVSTEV, ReadVSTX]>;

class VLSSched<int n> : Sched <[!cast<SchedReadWrite>("WriteVLDS" # n),
                                ReadVLDX, ReadVLDSX]>;

class VSSSched<int n> : Sched <[!cast<SchedReadWrite>("WriteVSTS" # n),
                                !cast<SchedReadWrite>("ReadVSTS" # n # "V"),
                                ReadVSTX, ReadVSTSX]>;

class VLXSched<int n, string o> :
  Sched <[!cast<SchedReadWrite>("WriteVLD" # o # "X" # n),
          ReadVLDX, !cast<SchedReadWrite>("ReadVLD" # o # "XV")]>;

class VSXSched<int n, string o> :
  Sched <[!cast<SchedReadWrite>("WriteVST" # o # "X" # n),
          !cast<SchedReadWrite>("ReadVST" # o # "X" # n),
          ReadVSTX, !cast<SchedReadWrite>("ReadVST" # o # "XV")]>;

class VLFSched : Sched <[ReadVLDX]>;

//===----------------------------------------------------------------------===//
// Instruction class templates
//===----------------------------------------------------------------------===//

//
// FIXME: The encoding for vluxei/vsuxei is not correct!!
//

let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in {
// Indexed load vd, (rs1), vs2
class VectorLoad<RISCVWidth width, string opcodestr>
    : RVInstVLX<0b000, width.Value{3}, width.Value{2-0},
                (outs VGPR:$vd),
                (ins GPRMem:$rs1, VGPR:$vs2), opcodestr,
                "$vd, (${rs1}), $vs2">;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in {
// Indexed store vd, vs3, (rs1), vs2
class VectorStore<RISCVWidth width, string opcodestr>
    : RVInstVSX<0b000, width.Value{3}, width.Value{2-0}, (outs),
                (ins VGPR:$vs3, GPRMem:$rs1, VGPR:$vs2),
                opcodestr, "$vs3, (${rs1}), $vs2">;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
// op vd, vs2, vs1
class VALUVV<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVV<funct6, opv, (outs VGPR:$vd),
                (ins VGPR:$vs2, VGPR:$vs1),
                opcodestr, "$vd, $vs2, $vs1">;

// op vd, vs1, vs2 (reverse the order of vs1 and vs2)
class VALUrVV<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVV<funct6, opv, (outs VGPR:$vd_wb),
                (ins VGPR:$vd, VGPR:$vs1, VGPR:$vs2),
                opcodestr, "$vd, $vs1, $vs2">;

// op vd, vs2, rs1
class VALUVX<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVX<funct6, opv, (outs VGPR:$vd),
                (ins VGPR:$vs2, GPR:$rs1),
                opcodestr, "$vd, $vs2, $rs1">;

// op vd, rs1, vs2 (reverse the order of rs1 and vs2)
class VALUrVX<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVX<funct6, opv, (outs VGPR:$vd_wb),
                (ins VGPR:$vd, GPR:$rs1, VGPR:$vs2),
                opcodestr, "$vd, $rs1, $vs2">;

// op vd, vs2, imm
class VALUVI<bits<6> funct6, string opcodestr, Operand optype = simm5>
    : RVInstIVI<funct6, (outs VGPR:$vd),
                (ins VGPR:$vs2, optype:$imm),
                opcodestr, "$vd, $vs2, $imm">;

// op vd, vs2, rs1 (Float)
class VALUVF<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVX<funct6, opv, (outs VGPR:$vd),
                (ins VGPR:$vs2, GPRF32:$rs1),
                opcodestr, "$vd, $vs2, $rs1">;

// op vd, rs1, vs2 (Float) (reverse the order of rs1 and vs2)
class VALUrVF<bits<6> funct6, RISCVVFormat opv, string opcodestr>
    : RVInstVX<funct6, opv, (outs VGPR:$vd_wb),
                (ins VGPR:$vd, GPRF32:$rs1, VGPR:$vs2),
                opcodestr, "$vd, $rs1, $vs2">;

// op vd, vs2 (use vs1 as instruction encoding)
class VALUVs2<bits<6> funct6, bits<5> vs1, RISCVVFormat opv, string opcodestr>
    : RVInstV<funct6, vs1, opv, (outs VGPR:$vd),
               (ins VGPR:$vs2),
               opcodestr, "$vd, $vs2">;

// op vd, vs2 (use vs1 as instruction encoding)
class VALUVs2_frm<bits<6> funct6, bits<5> vs1, RISCVVFormat opv, string opcodestr>
    : RVInstV<funct6, vs1, opv, (outs VGPR:$vd),
               (ins VGPR:$vs2, frmarg:$frm),
               opcodestr, "$vd, $vs2">;
} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

//===----------------------------------------------------------------------===//
// Combination of instruction classes.
// Use these multiclasses to define instructions more easily.
//===----------------------------------------------------------------------===//

multiclass VALU_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIALUV_UpperBound, ReadVIALUV_UpperBound,
                  ReadVIALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIALUX_UpperBound, ReadVIALUV_UpperBound,
                  ReadVIALUX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVIALUI_UpperBound, ReadVIALUV_UpperBound]>;
}

multiclass VALU_IV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIALUV_UpperBound, ReadVIALUV_UpperBound,
                  ReadVIALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIALUX_UpperBound, ReadVIALUV_UpperBound,
                  ReadVIALUX_UpperBound]>;
}

//  Only VRSUB__VX use this class
multiclass VALU_IV_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  // def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
  //          Sched<[WriteVIALUV_UpperBound, ReadVIALUV_UpperBound,
  //                 ReadVIALUX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVIALUI_UpperBound, ReadVIALUV_UpperBound]>;
}

multiclass VALU_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIWALUV_UpperBound, ReadVIWALUV_UpperBound,
                  ReadVIWALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIWALUX_UpperBound, ReadVIWALUV_UpperBound,
                  ReadVIWALUX_UpperBound]>;
}

multiclass VMAC_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUrVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVIMulAddV_UpperBound, ReadVIMulAddV_UpperBound,
                 ReadVIMulAddV_UpperBound]>;
  def X : VALUrVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
          Sched<[WriteVIMulAddX_UpperBound, ReadVIMulAddV_UpperBound,
                 ReadVIMulAddX_UpperBound]>;
}

multiclass VWMAC_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUrVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVIWMulAddV_UpperBound, ReadVIWMulAddV_UpperBound,
                 ReadVIWMulAddV_UpperBound]>;
  def X : VALUrVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
          Sched<[WriteVIWMulAddX_UpperBound, ReadVIWMulAddV_UpperBound,
                 ReadVIWMulAddX_UpperBound]>;
}

multiclass VWMAC_MV_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def X : VALUrVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
          Sched<[WriteVIWMulAddX_UpperBound, ReadVIWMulAddV_UpperBound,
                 ReadVIWMulAddX_UpperBound]>;
}

multiclass VALU_MV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPMVV, opcodestr>,
           Sched<[WriteVExtV_UpperBound, ReadVExtV_UpperBound]>;
}

multiclass VALU_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFALUV_UpperBound, ReadVFALUV_UpperBound,
                 ReadVFALUV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFALUF_UpperBound, ReadVFALUV_UpperBound,
                 ReadVFALUF_UpperBound]>;
}

multiclass VALU_FV_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFALUF_UpperBound, ReadVFALUV_UpperBound,
                 ReadVFALUF_UpperBound]>;
}

multiclass VWALU_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFWALUV_UpperBound, ReadVFWALUV_UpperBound,
                 ReadVFWALUV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFWALUF_UpperBound, ReadVFWALUV_UpperBound,
                 ReadVFWALUF_UpperBound]>;
}

multiclass VMUL_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFMulV_UpperBound, ReadVFMulV_UpperBound,
                 ReadVFMulV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFMulF_UpperBound, ReadVFMulV_UpperBound,
                 ReadVFMulF_UpperBound]>;
}

multiclass VDIV_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFDivV_UpperBound, ReadVFDivV_UpperBound,
                 ReadVFDivV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFDivF_UpperBound, ReadVFDivV_UpperBound,
                 ReadVFDivF_UpperBound]>;
}

multiclass VRDIV_FV_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFDivF_UpperBound, ReadVFDivV_UpperBound,
                 ReadVFDivF_UpperBound]>;
}

multiclass VWMUL_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFWMulV_UpperBound, ReadVFWMulV_UpperBound,
                 ReadVFWMulV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFWMulF_UpperBound, ReadVFWMulV_UpperBound,
                 ReadVFWMulF_UpperBound]>;
}

multiclass VMAC_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUrVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFMulAddV_UpperBound, ReadVFMulAddV_UpperBound,
                 ReadVFMulAddV_UpperBound]>;
  def F : VALUrVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFMulAddF_UpperBound, ReadVFMulAddV_UpperBound,
                 ReadVFMulAddF_UpperBound]>;
}

multiclass VWMAC_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUrVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFWMulAddV_UpperBound, ReadVFWMulAddV_UpperBound,
                 ReadVFWMulAddV_UpperBound]>;
  def F : VALUrVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFWMulAddF_UpperBound, ReadVFWMulAddV_UpperBound,
                 ReadVFWMulAddF_UpperBound]>;
}

multiclass VSQR_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFSqrtV_UpperBound, ReadVFSqrtV_UpperBound]>;
}

multiclass VRCP_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFRecpV_UpperBound, ReadVFRecpV_UpperBound]>;
}

multiclass VCMP_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFCmpV_UpperBound, ReadVFCmpV_UpperBound,
                 ReadVFCmpV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFCmpF_UpperBound, ReadVFCmpV_UpperBound,
                 ReadVFCmpF_UpperBound]>;
}

multiclass VCMP_FV_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFCmpF_UpperBound, ReadVFCmpV_UpperBound,
                 ReadVFCmpF_UpperBound]>;
}

multiclass VSGNJ_FV_V_F<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V : VALUVV<funct6, OPFVV, opcodestr # "." # vw # "v">,
          Sched<[WriteVFSgnjV_UpperBound, ReadVFSgnjV_UpperBound,
                 ReadVFSgnjV_UpperBound]>;
  def F : VALUVF<funct6, OPFVF, opcodestr # "." # vw # "f">,
          Sched<[WriteVFSgnjF_UpperBound, ReadVFSgnjV_UpperBound,
                 ReadVFSgnjF_UpperBound]>;
}

multiclass VCLS_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFClassV_UpperBound, ReadVFClassV_UpperBound]>;
}

multiclass VCVTF_IV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFCvtIToFV_UpperBound, ReadVFCvtIToFV_UpperBound]>;
}

multiclass VCVTI_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFCvtFToIV_UpperBound, ReadVFCvtFToIV_UpperBound]>;
}

multiclass VCVTI_FV_VS2_FRM<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2_frm<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFCvtFToIV_UpperBound, ReadVFCvtFToIV_UpperBound]>;
}

multiclass VWCVTF_IV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFWCvtIToFV_UpperBound, ReadVFWCvtIToFV_UpperBound]>;
}

multiclass VWCVTI_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFWCvtFToIV_UpperBound, ReadVFWCvtFToIV_UpperBound]>;
}

multiclass VWCVTF_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFWCvtFToFV_UpperBound, ReadVFWCvtFToFV_UpperBound]>;
}

multiclass VNCVTF_IV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFNCvtIToFV_UpperBound, ReadVFNCvtIToFV_UpperBound]>;
}

multiclass VNCVTI_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFNCvtFToIV_UpperBound, ReadVFNCvtFToIV_UpperBound]>;
}

multiclass VNCVTF_FV_VS2<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPFVV, opcodestr>,
           Sched<[WriteVFNCvtFToFV_UpperBound, ReadVFNCvtFToFV_UpperBound]>;
}

multiclass VRED_MV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPMVV, opcodestr # ".vs">,
            Sched<[WriteVIRedV, ReadVIRedV, ReadVIRedV0]>;
}

multiclass VWRED_IV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPIVV, opcodestr # ".vs">,
            Sched<[WriteVIWRedV, ReadVIWRedV, ReadVIWRedV0]>;
}

multiclass VRED_FV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPFVV, opcodestr # ".vs">,
            Sched<[WriteVFRedV, ReadVFRedV, ReadVFRedV0]>;
}

multiclass VREDO_FV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPFVV, opcodestr # ".vs">,
            Sched<[WriteVFRedOV, ReadVFRedOV, ReadVFRedOV0]>;
}

multiclass VWRED_FV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPFVV, opcodestr # ".vs">,
            Sched<[WriteVFWRedV, ReadVFWRedV, ReadVFWRedV0]>;
}

multiclass VWREDO_FV_V<string opcodestr, bits<6> funct6> {
  def _VS : VALUVV<funct6, OPFVV, opcodestr # ".vs">,
            Sched<[WriteVFWRedOV, ReadVFWRedOV, ReadVFWRedOV0]>;
}

multiclass VMSFS_MV_V<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPMVV, opcodestr>,
           Sched<[WriteVMSFSV, ReadVMSFSV]>;
}

multiclass VMIOT_MV_V<string opcodestr, bits<6> funct6, bits<5> vs1> {
  def "" : VALUVs2<funct6, vs1, OPMVV, opcodestr>,
           Sched<[WriteVMIotV, ReadVMIotV]>;
}

multiclass VSHT_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVShiftV_UpperBound, ReadVShiftV_UpperBound,
                  ReadVShiftV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVShiftX_UpperBound, ReadVShiftV_UpperBound,
                  ReadVShiftX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVShiftI_UpperBound, ReadVShiftV_UpperBound]>;
}

multiclass VNSHT_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVNShiftV_UpperBound, ReadVNShiftV_UpperBound,
                  ReadVNShiftV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVNShiftX_UpperBound, ReadVNShiftV_UpperBound,
                  ReadVNShiftX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVNShiftI_UpperBound, ReadVNShiftV_UpperBound]>;
}

multiclass VCMP_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVICmpV_UpperBound, ReadVICmpV_UpperBound,
                  ReadVICmpV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVICmpX_UpperBound, ReadVICmpV_UpperBound,
                  ReadVICmpX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVICmpI_UpperBound, ReadVICmpV_UpperBound]>;
}

multiclass VCMP_IV_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVICmpV_UpperBound, ReadVICmpV_UpperBound,
                  ReadVICmpX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVICmpI_UpperBound, ReadVICmpV_UpperBound]>;
}

multiclass VCMP_IV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVICmpV_UpperBound, ReadVICmpV_UpperBound,
                  ReadVICmpV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVICmpX_UpperBound, ReadVICmpV_UpperBound,
                  ReadVICmpX_UpperBound]>;
}

multiclass VMUL_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIMulV_UpperBound, ReadVIMulV_UpperBound,
                  ReadVIMulV_UpperBound]>;
  def X  : VALUVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIMulX_UpperBound, ReadVIMulV_UpperBound,
                  ReadVIMulX_UpperBound]>;
}

multiclass VWMUL_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIWMulV_UpperBound, ReadVIWMulV_UpperBound,
                  ReadVIWMulV_UpperBound]>;
  def X  : VALUVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIWMulX_UpperBound, ReadVIWMulV_UpperBound,
                  ReadVIWMulX_UpperBound]>;
}

multiclass VDIV_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVIDivV_UpperBound, ReadVIDivV_UpperBound,
                  ReadVIDivV_UpperBound]>;
  def X  : VALUVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVIDivX_UpperBound, ReadVIDivV_UpperBound,
                  ReadVIDivX_UpperBound]>;
}

multiclass VSALU_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVSALUV_UpperBound, ReadVSALUV_UpperBound,
                  ReadVSALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVSALUX_UpperBound, ReadVSALUV_UpperBound,
                  ReadVSALUX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVSALUI_UpperBound, ReadVSALUV_UpperBound]>;
}

multiclass VSALU_IV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVSALUV_UpperBound, ReadVSALUV_UpperBound,
                  ReadVSALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVSALUX_UpperBound, ReadVSALUV_UpperBound,
                  ReadVSALUX_UpperBound]>;
}

multiclass VAALU_MV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPMVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVAALUV_UpperBound, ReadVAALUV_UpperBound,
                  ReadVAALUV_UpperBound]>;
  def X  : VALUVX<funct6, OPMVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVAALUX_UpperBound, ReadVAALUV_UpperBound,
                  ReadVAALUX_UpperBound]>;
}

multiclass VSMUL_IV_V_X<string opcodestr, bits<6> funct6, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVSMulV_UpperBound, ReadVSMulV_UpperBound,
                  ReadVSMulV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVSMulX_UpperBound, ReadVSMulV_UpperBound,
                  ReadVSMulX_UpperBound]>;
}

multiclass VSSHF_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVSShiftV_UpperBound, ReadVSShiftV_UpperBound,
                  ReadVSShiftV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVSShiftX_UpperBound, ReadVSShiftV_UpperBound,
                  ReadVSShiftX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVSShiftI_UpperBound, ReadVSShiftV_UpperBound]>;
}

multiclass VNCLP_IV_V_X_I<string opcodestr, bits<6> funct6, Operand optype = simm5, string vw = "v"> {
  def V  : VALUVV<funct6, OPIVV, opcodestr # "." # vw # "v">,
           Sched<[WriteVNClipV_UpperBound, ReadVNClipV_UpperBound,
                  ReadVNClipV_UpperBound]>;
  def X  : VALUVX<funct6, OPIVX, opcodestr # "." # vw # "x">,
           Sched<[WriteVNClipX_UpperBound, ReadVNClipV_UpperBound,
                  ReadVNClipX_UpperBound]>;
  def I  : VALUVI<funct6, opcodestr # "." # vw # "i", optype>,
           Sched<[WriteVNClipI_UpperBound, ReadVNClipV_UpperBound]>;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    isBranch = 1, isTerminator = 1 in
class BranchCC_vvi<bits<3> funct3, string opcodestr>
    : RVInstVB<funct3, (outs), (ins VGPR:$vs2, VGPR:$vs1, simm13_lsb0:$imm12),
               opcodestr, "$vs2, $vs1, $imm12">, Sched<[]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    isBarrier = 1, isBranch = 1, isTerminator = 1 in
class Branch_i<bits<3> funct3, string opcodestr>
    : RVInstVB<funct3, (outs), (ins simm13_lsb0:$imm12),
               opcodestr, "v0, v0, $imm12">, Sched<[]> {
  let vs1 = 0;
  let vs2 = 0;
}

// Private memory load/store instructions
class VENTUS_VL<bits<3> funct3, string opcodestr>
    : RVInstI<funct3, OPC_CUSTOM_1, (outs VGPR:$rd),
              (ins VGPRMem:$rs1, simm11:$imm12),
              opcodestr # ".v", "$rd, ${imm12}(${rs1})"> {
  let Inst{31} = 0;
  let Inst{30-20} = imm12{10-0};
}
class VENTUS_VS<bits<3> funct3, string opcodestr>
    : RVInstS<funct3, OPC_CUSTOM_1, (outs),
              (ins VGPR:$rs2, VGPRMem:$rs1, simm11:$imm12),
              opcodestr # ".v", "$rs2, ${imm12}(${rs1})"> {
  let Inst{31} = 1;
  let Inst{30-25} = imm12{10-5};
  let Inst{11-7} = imm12{4-0};
}

// Local/Global memory load/store instructions
class VENTUS_VLI12<bits<3> funct3, string opcodestr> :
              RVInstI<funct3, OPC_CUSTOM_3, (outs VGPR:$rd),
              (ins VGPRMem:$rs1, simm12:$imm12),
              opcodestr # ".v" , "$rd, ${imm12}(${rs1})">, Sched<[]>;
class VENTUS_VSI12<bits<3> funct3, string opcodestr> :
              RVInstS<funct3, OPC_CUSTOM_3, (outs),
              (ins VGPR:$rs2, VGPRMem:$rs1, simm12:$imm12),
              opcodestr # ".v", "$rs2, ${imm12}(${rs1})">, Sched<[]>;

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
def VSETVLI : RVInstSetVLi<(outs GPR:$rd), (ins GPR:$vs1, VTypeIOp11:$vtypei),
                           "vsetvli", "$rd, $vs1, $vtypei">,
                           Sched<[WriteVSETVLI, ReadVSETVLI]>;
}

// X6 will be defs by SETRPC.
let Defs = [X6] in {
def VBEQ  : BranchCC_vvi<0b000, "vbeq">;
def VBNE  : BranchCC_vvi<0b001, "vbne">;
def VBLT  : BranchCC_vvi<0b100, "vblt">;
def VBGE  : BranchCC_vvi<0b101, "vbge">;
def VBLTU : BranchCC_vvi<0b110, "vbltu">;
def VBGEU : BranchCC_vvi<0b111, "vbgeu">;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, Defs = [RPC] in
def SETRPC : RVInstI<0b011, OPC_CUSTOM_2, (outs GPR:$rd), (ins GPR:$rs1, simm12:$imm12),
                     "setrpc", "$rd, $rs1, $imm12">,
             Sched<[WriteIALU, ReadIALU]>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, CustomConstraints = "" in
def JOIN : RVInstVB<0b010, (outs), (ins GPR:$rs2, GPR:$rs1, simm12:$imm12),
                                    "join", "$rs2, $rs1, $imm12">, Sched<[]> {
  bits<5> rs2;
  bits<5> rs1;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
}

def VLUXEI8  : VectorLoad<LSWidth8,  "vluxei8.v">;
def VLUXEI16 : VectorLoad<LSWidth16, "vluxei16.v">;
def VLUXEI32 : VectorLoad<LSWidth32, "vluxei32.v">;

// TODO: Encoding for these 2 instructions are not correct.
//def VLUXEI8U  : VectorLoad<LSWidth8,  "vluxei8u.v">;
//def VLUXEI16U : VectorLoad<LSWidth16, "vluxei16u.v">;

def VSUXEI8  : VectorStore<LSWidth8,  "vsuxei8.v">;
def VSUXEI16 : VectorStore<LSWidth16, "vsuxei16.v">;
def VSUXEI32 : VectorStore<LSWidth32, "vsuxei32.v">;

// FIXME: simm11 is encoded into register name in assembly code.
let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in {
def VLW : VENTUS_VL<0b010, "vlw">;
def VLH : VENTUS_VL<0b001, "vlh">;
def VLB : VENTUS_VL<0b000, "vlb">;
def VLHU : VENTUS_VL<0b101, "vlhu">;
def VLBU : VENTUS_VL<0b100, "vlbu">;
def VLWI12 : VENTUS_VLI12<0b010, "vlw12">;
def VLHI12 : VENTUS_VLI12<0b001, "vlh12">;
def VLBI12 : VENTUS_VLI12<0b000, "vlb12">;
def VLHUI12 : VENTUS_VLI12<0b101, "vlhu12">;
def VLBUI12 : VENTUS_VLI12<0b100, "vlbu12">;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in {
def VSW : VENTUS_VS<0b010, "vsw">;
def VSH : VENTUS_VS<0b001, "vsh">;
def VSB : VENTUS_VS<0b000, "vsb">;
// FIXME: update funct3
def VSWI12 : VENTUS_VSI12<0b110, "vsw12">;
def VSHI12 : VENTUS_VSI12<0b011, "vsh12">;
def VSBI12 : VENTUS_VSI12<0b111, "vsb12">;
}

let Predicates = [HasVInstructions] in {
// Ventus extended immediate12 vALU instructions
class VALU_ri12<bits<3> funct3, string opcode> :
      RVInstI<funct3, OPC_CUSTOM_0,
              (outs VGPR:$rd), (ins VGPR:$rs1, uimm12:$imm12),
              "v" # opcode # "12.vi", "$rd, $rs1, $imm12">;
// vadd12.vi vd, vs1, imm
def VADDIMM12 : VALU_ri12<0b000, "add">;
// vsub12.vi vd, vs1, imm
def VSUBIMM12 : VALU_ri12<0b001, "sub">;

// Vector Single-Width Integer Add and Subtract
defm VADD_V : VALU_IV_V_X_I<"vadd", 0b000000>;
defm VSUB_V : VALU_IV_V_X<"vsub", 0b000010>;
defm VRSUB_V : VALU_IV_X_I<"vrsub", 0b000011>;

def : InstAlias<"vneg.v $vd, $vs", (VRSUB_VX VGPR:$vd, VGPR:$vs, X0)>;

// Vector Widening Integer Add/Subtract
// Refer to 11.2 Widening Vector Arithmetic Instructions
// The destination vector register group cannot overlap a source vector
// register group of a different element width, otherwise an illegal
// instruction exception is raised.
let Constraints = "@earlyclobber $vd" in {
let RVVConstraint = WidenV in {
// defm VWADDU_V : VALU_MV_V_X<"vwaddu", 0b110000>;
// defm VWSUBU_V : VALU_MV_V_X<"vwsubu", 0b110010>;
// defm VWADD_V : VALU_MV_V_X<"vwadd", 0b110001>;
// defm VWSUB_V : VALU_MV_V_X<"vwsub", 0b110011>;
} // RVVConstraint = WidenV
// Set earlyclobber for following instructions for second operands.
// This has the downside that the earlyclobber constraint is too coarse and
// will impose unnecessary restrictions by not allowing the destination to
// overlap with the first (wide) operand.
let RVVConstraint = WidenW in {
// defm VWADDU_W : VALU_MV_V_X<"vwaddu", 0b110100, "w">;
// defm VWSUBU_W : VALU_MV_V_X<"vwsubu", 0b110110, "w">;
// defm VWADD_W : VALU_MV_V_X<"vwadd", 0b110101, "w">;
// defm VWSUB_W : VALU_MV_V_X<"vwsub", 0b110111, "w">;
} // RVVConstraint = WidenW
} // Constraints = "@earlyclobber $vd"

// def : InstAlias<"vwcvt.x.x.v $vd, $vs",
//                 (VWADD_VX VGPR:$vd, VGPR:$vs, X0)>;
// def : InstAlias<"vwcvtu.x.x.v $vd, $vs",
//                 (VWADDU_VX VGPR:$vd, VGPR:$vs, X0)>;

// Vector Integer Extension
defm VZEXT_VF8 : VALU_MV_VS2<"vzext.vf8", 0b010010, 0b00010>;
defm VSEXT_VF8 : VALU_MV_VS2<"vsext.vf8", 0b010010, 0b00011>;
defm VZEXT_VF4 : VALU_MV_VS2<"vzext.vf4", 0b010010, 0b00100>;
defm VSEXT_VF4 : VALU_MV_VS2<"vsext.vf4", 0b010010, 0b00101>;
defm VZEXT_VF2 : VALU_MV_VS2<"vzext.vf2", 0b010010, 0b00110>;
defm VSEXT_VF2 : VALU_MV_VS2<"vsext.vf2", 0b010010, 0b00111>;

// Vector Bitwise Logical Instructions
defm VAND_V : VALU_IV_V_X_I<"vand", 0b001001>;
defm VOR_V : VALU_IV_V_X_I<"vor", 0b001010>;
defm VXOR_V : VALU_IV_V_X_I<"vxor", 0b001011>;

def : InstAlias<"vnot.v $vd, $vs",
                (VXOR_VI VGPR:$vd, VGPR:$vs, -1)>;

// Vector Single-Width Bit Shift Instructions
defm VSLL_V : VSHT_IV_V_X_I<"vsll", 0b100101, uimm5>;
defm VSRL_V : VSHT_IV_V_X_I<"vsrl", 0b101000, uimm5>;
defm VSRA_V : VSHT_IV_V_X_I<"vsra", 0b101001, uimm5>;

// Vector Narrowing Integer Right Shift Instructions
// Refer to 11.3. Narrowing Vector Arithmetic Instructions
// The destination vector register group cannot overlap the first source
// vector register group (specified by vs2).
let Constraints = "@earlyclobber $vd" in {
defm VNSRL_W : VNSHT_IV_V_X_I<"vnsrl", 0b101100, uimm5, "w">;
defm VNSRA_W : VNSHT_IV_V_X_I<"vnsra", 0b101101, uimm5, "w">;
} // Constraints = "@earlyclobber $vd"

def : InstAlias<"vncvt.x.x.w $vd, $vs",
                (VNSRL_WX VGPR:$vd, VGPR:$vs, X0)>;

// Vector Integer Comparison Instructions
let RVVConstraint = NoConstraint in {
defm VMSEQ_V : VCMP_IV_V_X_I<"vmseq", 0b011000>;
defm VMSNE_V : VCMP_IV_V_X_I<"vmsne", 0b011001>;
defm VMSLTU_V : VCMP_IV_V_X<"vmsltu", 0b011010>;
defm VMSLT_V : VCMP_IV_V_X<"vmslt", 0b011011>;
defm VMSLEU_V : VCMP_IV_V_X_I<"vmsleu", 0b011100, uimm5>;
defm VMSLE_V : VCMP_IV_V_X_I<"vmsle", 0b011101>;
defm VMSGTU_V : VCMP_IV_X_I<"vmsgtu", 0b011110, uimm5>;
defm VMSGT_V : VCMP_IV_X_I<"vmsgt", 0b011111>;
} // RVVConstraint = NoConstraint

def : InstAlias<"vmsgtu.vv $vd, $va, $vb",
                (VMSLTU_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;
def : InstAlias<"vmsgt.vv $vd, $va, $vb",
                (VMSLT_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;
def : InstAlias<"vmsgeu.vv $vd, $va, $vb",
                (VMSLEU_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;
def : InstAlias<"vmsge.vv $vd, $va, $vb",
                (VMSLE_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;

let isCodeGenOnly = 0, isAsmParserOnly = 1, hasSideEffects = 0, mayLoad = 0,
    mayStore = 0, CustomConstraints = "$vd = 0,$vs2 = 2" in {
// For unsigned comparisons we need to special case 0 immediate to maintain
// the always true/false semantics we would invert if we just decremented the
// immediate like we do for signed. To match the GNU assembler we will use
// vmseq/vmsne.vv with the same register for both operands which we can't do
// from an InstAlias.
def PseudoVMSGEU_VI : Pseudo<(outs VGPR:$vd),
                             (ins VGPR:$vs2, simm5_plus1:$imm),
      [(set (XLenVT VGPR:$vd), (setuge (XLenVT VGPR:$vs2), simm5_plus1:$imm))],
      "vmsgeu.vi", "$vd, $vs2, $imm">;
def PseudoVMSLTU_VI : Pseudo<(outs VGPR:$vd),
            (ins VGPR:$vs2, uimm5:$imm),
            [(set (XLenVT VGPR:$vd), (setult (XLenVT VGPR:$vs2), uimm5:$imm))],
            "vmsltu.vi", "$vd, $vs2, $imm">;
// Handle signed with pseudos as well for more consistency in the
// implementation.
def PseudoVMSGE_VI : Pseudo<(outs VGPR:$vd),
                            (ins VGPR:$vs2, simm5_plus1:$imm),
        [(set (XLenVT VGPR:$vd), (setge (XLenVT VGPR:$vs2), simm5_plus1:$imm))],
        "vmsge.vi", "$vd, $vs2, $imm">;

def PseudoVMSLT_VI : Pseudo<(outs VGPR:$vd), (ins VGPR:$vs2, simm5_plus1:$imm),
        [(set (XLenVT VGPR:$vd), (setlt (XLenVT VGPR:$vs2), simm5_plus1:$imm))],
        "vmslt.vi", "$vd, $vs2, $imm">;
}

let isCodeGenOnly = 0, isAsmParserOnly = 1, hasSideEffects = 0, mayLoad = 0,
    mayStore = 0 in {
def PseudoVMSGEU_VX : Pseudo<(outs VGPR:$vd),
                             (ins VGPR:$vs2, GPR:$rs1),
                             [], "vmsgeu.vx", "$vd, $vs2, $rs1">;
def PseudoVMSGE_VX : Pseudo<(outs VGPR:$vd),
                            (ins VGPR:$vs2, GPR:$rs1),
                            [], "vmsge.vx", "$vd, $vs2, $rs1">;
}

// Vector Integer Min/Max Instructions
defm VMINU_V : VCMP_IV_V_X<"vminu", 0b000100>;
defm VMIN_V : VCMP_IV_V_X<"vmin", 0b000101>;
defm VMAXU_V : VCMP_IV_V_X<"vmaxu", 0b000110>;
defm VMAX_V : VCMP_IV_V_X<"vmax", 0b000111>;

// Vector Single-Width Integer Multiply Instructions
defm VMUL_V : VMUL_MV_V_X<"vmul", 0b100101>;
defm VMULH_V : VMUL_MV_V_X<"vmulh", 0b100111>;
defm VMULHU_V : VMUL_MV_V_X<"vmulhu", 0b100100>;
defm VMULHSU_V : VMUL_MV_V_X<"vmulhsu", 0b100110>;

// Vector Integer Divide Instructions
defm VDIVU_V : VDIV_MV_V_X<"vdivu", 0b100000>;
defm VDIV_V : VDIV_MV_V_X<"vdiv", 0b100001>;
defm VREMU_V : VDIV_MV_V_X<"vremu", 0b100010>;
defm VREM_V : VDIV_MV_V_X<"vrem", 0b100011>;

// Vector Widening Integer Multiply Instructions
let Constraints = "@earlyclobber $vd", RVVConstraint = WidenV in {
// defm VWMUL_V : VWMUL_MV_V_X<"vwmul", 0b111011>;
// defm VWMULU_V : VWMUL_MV_V_X<"vwmulu", 0b111000>;
// defm VWMULSU_V : VWMUL_MV_V_X<"vwmulsu", 0b111010>;
} // Constraints = "@earlyclobber $vd", RVVConstraint = WidenV

// Vector Single-Width Integer Multiply-Add Instructions
let Constraints = "$vd_wb = $vs2" in {
  defm VMACC_V : VMAC_MV_V_X<"vmacc", 0b101101>;
  defm VNMSAC_V : VMAC_MV_V_X<"vnmsac", 0b101111>;
}
let Constraints = "$vd_wb = $vd" in {
  defm VMADD_V : VMAC_MV_V_X<"vmadd", 0b101001>;
  defm VNMSUB_V : VMAC_MV_V_X<"vnmsub", 0b101011>;
}
// Vector Widening Integer Multiply-Add Instructions
// let Constraints = "@earlyclobber $vd", RVVConstraint = WidenV in {
// defm VWMACCU_V : VWMAC_MV_V_X<"vwmaccu", 0b111100>;
// defm VWMACC_V : VWMAC_MV_V_X<"vwmacc", 0b111101>;
// defm VWMACCSU_V : VWMAC_MV_V_X<"vwmaccsu", 0b111111>;
// defm VWMACCUS_V : VWMAC_MV_X<"vwmaccus", 0b111110>;
// } // Constraints = "@earlyclobber $vd", RVVConstraint = WidenV

// Vector Fixed-Point Arithmetic Instructions
defm VSADDU_V : VSALU_IV_V_X_I<"vsaddu", 0b100000>;
defm VSADD_V : VSALU_IV_V_X_I<"vsadd", 0b100001>;
defm VSSUBU_V : VSALU_IV_V_X<"vssubu", 0b100010>;
defm VSSUB_V : VSALU_IV_V_X<"vssub", 0b100011>;

// Vector Single-Width Averaging Add and Subtract
defm VAADDU_V : VAALU_MV_V_X<"vaaddu", 0b001000>;
defm VAADD_V : VAALU_MV_V_X<"vaadd", 0b001001>;
defm VASUBU_V : VAALU_MV_V_X<"vasubu", 0b001010>;
defm VASUB_V : VAALU_MV_V_X<"vasub", 0b001011>;

// Vector Single-Width Fractional Multiply with Rounding and Saturation
defm VSMUL_V : VSMUL_IV_V_X<"vsmul", 0b100111>;

// Vector Single-Width Scaling Shift Instructions
defm VSSRL_V : VSSHF_IV_V_X_I<"vssrl", 0b101010, uimm5>;
defm VSSRA_V : VSSHF_IV_V_X_I<"vssra", 0b101011, uimm5>;

// Vector Narrowing Fixed-Point Clip Instructions
let Constraints = "@earlyclobber $vd" in {
defm VNCLIPU_W : VNCLP_IV_V_X_I<"vnclipu", 0b101110, uimm5, "w">;
defm VNCLIP_W : VNCLP_IV_V_X_I<"vnclip", 0b101111, uimm5, "w">;
} // Constraints = "@earlyclobber $vd"
} // Predicates = [HasVInstructions]

let Predicates = [HasVInstructionsAnyF] in {
// Vector Single-Width Floating-Point Add/Subtract Instructions
let Uses = [FRM], mayRaiseFPException = true in {
defm VFADD_V : VALU_FV_V_F<"vfadd", 0b000000>;
defm VFSUB_V : VALU_FV_V_F<"vfsub", 0b000010>;
defm VFRSUB_V : VALU_FV_F<"vfrsub", 0b100111>;
}

// Vector Widening Floating-Point Add/Subtract Instructions
let Constraints = "@earlyclobber $vd",
    Uses = [FRM],
    mayRaiseFPException = true in {
// let RVVConstraint = WidenV in {
// defm VFWADD_V : VWALU_FV_V_F<"vfwadd", 0b110000>;
// defm VFWSUB_V : VWALU_FV_V_F<"vfwsub", 0b110010>;
// } // RVVConstraint = WidenV
// Set earlyclobber for following instructions for second operands.
// This has the downside that the earlyclobber constraint is too coarse and
// will impose unnecessary restrictions by not allowing the destination to
// overlap with the first (wide) operand.
// let RVVConstraint = WidenW in {
// defm VFWADD_W : VWALU_FV_V_F<"vfwadd", 0b110100, "w">;
// defm VFWSUB_W : VWALU_FV_V_F<"vfwsub", 0b110110, "w">;
// } // RVVConstraint = WidenW
} // Constraints = "@earlyclobber $vd", Uses = [FRM], mayRaiseFPException = true

// Vector Single-Width Floating-Point Multiply/Divide Instructions
let Uses = [FRM], mayRaiseFPException = true in {
defm VFMUL_V : VMUL_FV_V_F<"vfmul", 0b100100>;
defm VFDIV_V : VDIV_FV_V_F<"vfdiv", 0b100000>;
defm VFRDIV_V : VRDIV_FV_F<"vfrdiv", 0b100001>;
}

// Vector Widening Floating-Point Multiply
// let Constraints = "@earlyclobber $vd", RVVConstraint = WidenV,
//     Uses = [FRM], mayRaiseFPException = true in {
// defm VFWMUL_V : VWMUL_FV_V_F<"vfwmul", 0b111000>;
// } // Constraints = "@earlyclobber $vd", RVVConstraint = WidenV, Uses = [FRM], mayRaiseFPException = true

// Vector Single-Width Floating-Point Fused Multiply-Add Instructions
let Uses = [FRM], mayRaiseFPException = true in {
let Constraints = "$vd_wb = $vs2" in {
  defm VFMACC_V : VMAC_FV_V_F<"vfmacc", 0b101100>;
  defm VFNMACC_V : VMAC_FV_V_F<"vfnmacc", 0b101101>;
  defm VFMSAC_V : VMAC_FV_V_F<"vfmsac", 0b101110>;
  defm VFNMSAC_V : VMAC_FV_V_F<"vfnmsac", 0b101111>;
}
let Constraints = "$vd_wb = $vd" in {
  defm VFMADD_V : VMAC_FV_V_F<"vfmadd", 0b101000>;
  defm VFNMADD_V : VMAC_FV_V_F<"vfnmadd", 0b101001>;
  defm VFMSUB_V : VMAC_FV_V_F<"vfmsub", 0b101010>;
  defm VFNMSUB_V : VMAC_FV_V_F<"vfnmsub", 0b101011>;
}
}

// Vector Widening Floating-Point Fused Multiply-Add Instructions
// let Constraints = "@earlyclobber $vd", RVVConstraint = WidenV,
//     Uses = [FRM], mayRaiseFPException = true in {
// defm VFWMACC_V : VWMAC_FV_V_F<"vfwmacc", 0b111100>;
// defm VFWNMACC_V : VWMAC_FV_V_F<"vfwnmacc", 0b111101>;
// defm VFWMSAC_V : VWMAC_FV_V_F<"vfwmsac", 0b111110>;
// defm VFWNMSAC_V : VWMAC_FV_V_F<"vfwnmsac", 0b111111>;
// } // Constraints = "@earlyclobber $vd", RVVConstraint = WidenV, Uses = [FRM], mayRaiseFPException = true

// Vector Floating-Point Square-Root Instruction
let Uses = [FRM], mayRaiseFPException = true in {
defm VFSQRT_V : VSQR_FV_VS2<"vfsqrt.v", 0b010011, 0b00000>;
defm VFREC7_V : VRCP_FV_VS2<"vfrec7.v", 0b010011, 0b00101>;
}

let mayRaiseFPException = true in
defm VFRSQRT7_V : VRCP_FV_VS2<"vfrsqrt7.v", 0b010011, 0b00100>;

// Vector Floating-Point MIN/MAX Instructions
let mayRaiseFPException = true in {
defm VFMIN_V : VCMP_FV_V_F<"vfmin", 0b000100>;
defm VFMAX_V : VCMP_FV_V_F<"vfmax", 0b000110>;
}

// Vector Floating-Point Sign-Injection Instructions
defm VFSGNJ_V : VSGNJ_FV_V_F<"vfsgnj", 0b001000>;
defm VFSGNJN_V : VSGNJ_FV_V_F<"vfsgnjn", 0b001001>;
defm VFSGNJX_V : VSGNJ_FV_V_F<"vfsgnjx", 0b001010>;

def : InstAlias<"vfneg.v $vd, $vs",
                (VFSGNJN_VV VGPR:$vd, VGPR:$vs, VGPR:$vs), 0>;
def : InstAlias<"vfabs.v $vd, $vs",
                (VFSGNJX_VV VGPR:$vd, VGPR:$vs, VGPR:$vs), 0>;

// Vector Floating-Point Compare Instructions
let RVVConstraint = NoConstraint, mayRaiseFPException = true in {
defm VMFEQ_V : VCMP_FV_V_F<"vmfeq", 0b011000>;
defm VMFNE_V : VCMP_FV_V_F<"vmfne", 0b011100>;
defm VMFLT_V : VCMP_FV_V_F<"vmflt", 0b011011>;
defm VMFLE_V : VCMP_FV_V_F<"vmfle", 0b011001>;
defm VMFGT_V : VCMP_FV_F<"vmfgt", 0b011101>;
defm VMFGE_V : VCMP_FV_F<"vmfge", 0b011111>;
} // RVVConstraint = NoConstraint, mayRaiseFPException = true

// def : InstAlias<"vmsle.vi $vd, $va, !sub($vb, 1)",
//                 (VMSLT_VI VGPR:$vd, VGPR:$va, uimm5:$vb), 0>;
// def : InstAlias<"vmsleu.vi $vd, $va, !sub($vb, 1)",
//                 (VMSLTU_VI VGPR:$vd, VGPR:$va, uimm5:$vb), 0>;
def : InstAlias<"vmfgt.vv $vd, $va, $vb",
                (VMFLT_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;
def : InstAlias<"vmfge.vv $vd, $va, $vb",
                (VMFLE_VV VGPR:$vd, VGPR:$vb, VGPR:$va), 0>;

// Vector Floating-Point Classify Instruction
defm VFCLASS_V : VCLS_FV_VS2<"vfclass.v", 0b010011, 0b10000>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
// Vector Floating-Point Move Instruction
let RVVConstraint = NoConstraint in
let vs2 = 0, CustomConstraints = "$vd = 0,$rs1 = 1" in
def VFMV_V_F : RVInstVX<0b010111, OPFVF, (outs VGPR:$vd),
                       (ins GPRF32:$rs1), "vfmv.v.f", "$vd, $rs1">,
               Sched<[WriteVFMovV_UpperBound, ReadVFMovF_UpperBound]>;

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

// Single-Width Floating-Point/Integer Type-Convert Instructions
let mayRaiseFPException = true in {
let Uses = [FRM] in {
defm VFCVT_XU_F_V : VCVTI_FV_VS2<"vfcvt.xu.f.v", 0b010010, 0b00000>;
defm VFCVT_X_F_V : VCVTI_FV_VS2<"vfcvt.x.f.v", 0b010010, 0b00001>;
}
// Follow the way by RISCVInstrInfoF
defm VFCVT_RTZ_XU_F_V : VCVTI_FV_VS2_FRM<"vfcvt.rtz.xu.f.v", 0b010010, 0b00110>;
defm VFCVT_RTZ_X_F_V : VCVTI_FV_VS2_FRM<"vfcvt.rtz.x.f.v", 0b010010, 0b00111>;
let Uses = [FRM] in {
defm VFCVT_F_XU_V : VCVTF_IV_VS2<"vfcvt.f.xu.v", 0b010010, 0b00010>;
defm VFCVT_F_X_V : VCVTF_IV_VS2<"vfcvt.f.x.v", 0b010010, 0b00011>;
}
} // mayRaiseFPException = true

// Widening Floating-Point/Integer Type-Convert Instructions
let Constraints = "@earlyclobber $vd", RVVConstraint = WidenCvt,
    mayRaiseFPException = true in {
let Uses = [FRM] in {
defm VFWCVT_XU_F_V : VWCVTI_FV_VS2<"vfwcvt.xu.f.v", 0b010010, 0b01000>;
defm VFWCVT_X_F_V : VWCVTI_FV_VS2<"vfwcvt.x.f.v", 0b010010, 0b01001>;
}
defm VFWCVT_RTZ_XU_F_V : VWCVTI_FV_VS2<"vfwcvt.rtz.xu.f.v", 0b010010, 0b01110>;
defm VFWCVT_RTZ_X_F_V : VWCVTI_FV_VS2<"vfwcvt.rtz.x.f.v", 0b010010, 0b01111>;
defm VFWCVT_F_XU_V : VWCVTF_IV_VS2<"vfwcvt.f.xu.v", 0b010010, 0b01010>;
defm VFWCVT_F_X_V : VWCVTF_IV_VS2<"vfwcvt.f.x.v", 0b010010, 0b01011>;
defm VFWCVT_F_F_V : VWCVTF_FV_VS2<"vfwcvt.f.f.v", 0b010010, 0b01100>;
} // Constraints = "@earlyclobber $vd", RVVConstraint = WidenCvt

// Narrowing Floating-Point/Integer Type-Convert Instructions
let Constraints = "@earlyclobber $vd", mayRaiseFPException = true in {
let Uses = [FRM] in {
defm VFNCVT_XU_F_W : VNCVTI_FV_VS2<"vfncvt.xu.f.w", 0b010010, 0b10000>;
defm VFNCVT_X_F_W : VNCVTI_FV_VS2<"vfncvt.x.f.w", 0b010010, 0b10001>;
}
defm VFNCVT_RTZ_XU_F_W : VNCVTI_FV_VS2<"vfncvt.rtz.xu.f.w", 0b010010, 0b10110>;
defm VFNCVT_RTZ_X_F_W : VNCVTI_FV_VS2<"vfncvt.rtz.x.f.w", 0b010010, 0b10111>;
let Uses = [FRM] in {
defm VFNCVT_F_XU_W : VNCVTF_IV_VS2<"vfncvt.f.xu.w", 0b010010, 0b10010>;
defm VFNCVT_F_X_W : VNCVTF_IV_VS2<"vfncvt.f.x.w", 0b010010, 0b10011>;
defm VFNCVT_F_F_W : VNCVTF_FV_VS2<"vfncvt.f.f.w", 0b010010, 0b10100>;
}
defm VFNCVT_ROD_F_F_W : VNCVTF_FV_VS2<"vfncvt.rod.f.f.w", 0b010010, 0b10101>;
} // Constraints = "@earlyclobber $vd", mayRaiseFPException = true
} // Predicates = HasVInstructionsAnyF]


let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

let vs2 = 0, CustomConstraints = "$vd = 0" in
def VID_V : RVInstV<0b010100, 0b10001, OPMVV, (outs VGPR:$vd),
                    (ins), "vid.v", "$vd">,
            Sched<[WriteVMIdxV]>;

// Integer Scalar Move Instructions
let RVVConstraint = NoConstraint in {

def VMV_V_X : RVInstV2<0b010111, 0b00000, OPIVX, (outs VGPR:$vd),
                      (ins GPR:$rs1), "vmv.v.x", "$vd, $rs1">,
              Sched<[WriteVIMovXV, ReadVIMovXV, ReadVIMovXX]>;
} // RVVConstraint = NoConstraint

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

let Predicates = [HasVInstructionsAnyF] in {

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    RVVConstraint = NoConstraint  in {
// Floating-Point Scalar Move Instructions
def VFMV_S_F : RVInstV2<0b010000, 0b00000, OPFVF, (outs VGPR:$vd),
                       (ins GPRF32:$rs1), "vfmv.s.f", "$vd, $rs1">,
               Sched<[WriteVFMovFV, ReadVFMovFV, ReadVFMovFX]>;

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

} // Predicates = [HasVInstructionsAnyF]
// RVInstVV<funct6, opv, (outs VGPR:$vd),
//                 (ins VGPR:$vs2, VGPR:$vs1),
//                 opcodestr, "$vd, $vs2, $vs1">
// def VFEXP1 : RVInstVV<0b010000, OPFVF, (outs GPRF32:$vd),
                      //  (ins GPRF32:$vs2, GPRF32:$vs1), "fadd.s", "$vd, $vs1, $vs2">;

//===----------------------------------------------------------------------===//
// Ventus GPGPU extended instructions for V formats
//===----------------------------------------------------------------------===//
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def VFEXP : RVInstIVI<0b000010, (outs VGPR:$vd),
                    (ins VGPR:$vs2),"vfexp", "$vd, $vs2"> {
  let Inst{25} = 0b1;
  let Inst{19-15} = 0;
  let Inst{14-12} = 0b110;
  let Opcode = OPC_CUSTOM_0.Value;
}

let Constraints = "$vd_wb = $vd" in
def VFTTA : RVInstIVI<0b000011, (outs VGPR:$vd_wb),
                    (ins VGPR:$vd, VGPR:$vs2, VGPR:$vs1),
                    "vftta.vv", "$vd, $vs2, $vs1"> {
  bits<5> vs1;
  let Inst{25} = 0b1;
  let Inst{19-15} = vs1;
  let Inst{14-12} = 0b100;
  let Opcode = OPC_CUSTOM_0.Value;
}

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

//===----------------------------------------------------------------------===//
// Ventus vALU divergent execution patterns
//===----------------------------------------------------------------------===//

// ATTENTION: please don't change the pattern order
// Private memory per-thread load/store
defm : DivergentPriLdPat<load, VLW>;
defm : DivergentPriLdPat<zextloadi16, VLHU>;
defm : DivergentPriLdPat<sextloadi16, VLH>;
defm : DivergentPriLdPat<extloadi16, VLH>;
defm : DivergentPriLdPat<zextloadi8, VLBU>;
defm : DivergentPriLdPat<extloadi8, VLB>;
defm : DivergentPriLdPat<sextloadi8, VLB>;
defm : DivergentPriStPat<store, VSW>;
defm : DivergentPriStPat<truncstorei16, VSH>;
defm : DivergentPriStPat<truncstorei8, VSB>;

// Non-private memory load/store
// TODO: add store/load test file for testing pattern match
defm : DivergentNonPriLdImmPat<load, VLWI12>;
defm : DivergentNonPriLdImmPat<sextloadi16, VLHI12>;
defm : DivergentNonPriLdImmPat<extloadi16, VLHI12>;
defm : DivergentNonPriLdImmPat<sextloadi8, VLBI12>;
defm : DivergentNonPriLdImmPat<extloadi8, VLBI12>;
defm : DivergentNonPriLdImmPat<zextloadi8, VLBUI12>;
defm : DivergentNonPriLdImmPat<zextloadi16, VLHUI12>;
defm : DivergentNonPriStImmPat<store, VSWI12>;
defm : DivergentNonPriStImmPat<truncstorei16, VSHI12>;
defm : DivergentNonPriStImmPat<truncstorei8, VSBI12>;
def : DivergentNonPriLdPat<sextloadi8, VLUXEI8>;
def : DivergentNonPriLdPat<extloadi8, VLUXEI8>;
def : DivergentNonPriLdPat<sextloadi16, VLUXEI16>;
def : DivergentNonPriLdPat<extloadi16, VLUXEI16>;
def : DivergentNonPriLdPat<load, VLUXEI32>;
//def : DivergentNonPriLdPat<zextloadi8, VLUXEI8U>;
//def : DivergentNonPriLdPat<zextloadi16, VLUXEI16U>;
def : DivergentNonPriStPat<truncstorei8, VSUXEI8>;
def : DivergentNonPriStPat<truncstorei16, VSUXEI16>;
def : DivergentNonPriStPat<store, VSUXEI32>;

// FIXME: check this review: https://reviews.llvm.org/D131729#inline-1269307
// def : PatIntSetCC<[VGPR, VGPR], SETLE, VMSLE_VV>;
// def : PatIntSetCC<[VGPR, GPR], SETLE, VMSLE_VX>;
// def : PatIntSetCC<[VGPR, uimm5], SETLE, VMSLE_VI>;
// def : PatIntSetCC<[VGPR, GPR], SETGT, VMSGT_VX>;
// def : PatIntSetCC<[VGPR, uimm5], SETGT, VMSGT_VI>;
// def : PatIntSetCC<[VGPR, VGPR], SETULE, VMSLEU_VV>;
// def : PatIntSetCC<[VGPR, GPR], SETULE, VMSLEU_VX>;
// def : PatIntSetCC<[VGPR, uimm5], SETULE, VMSLEU_VI>;
// def : PatIntSetCC<[VGPR, GPR], SETUGT, VMSGTU_VX>;
// def : PatIntSetCC<[VGPR, uimm5], SETUGT, VMSGTU_VI>;

defm : PatVXIBin<DivergentBinFrag<smin>, [VMIN_VV, VMIN_VX]>;
defm : PatVXIBin<DivergentBinFrag<umin>, [VMINU_VV, VMINU_VX]>;
defm : PatVXIBin<DivergentBinFrag<smax>, [VMAX_VV, VMAX_VX]>;
defm : PatVXIBin<DivergentBinFrag<umax>, [VMAXU_VV, VMAXU_VX]>;
defm : PatVXIBin<DivergentBinFrag<add>,  [VADD_VV, VADD_VX, VADD_VI]>;
defm : PatVXIBin<DivergentBinFrag<sub>,  [VSUB_VV, VSUB_VX]>;
defm : PatVXIBin<DivergentBinFrag<and>,  [VAND_VV, VAND_VX, VAND_VI]>;
defm : PatVXIBin<DivergentBinFrag<or>,   [VOR_VV, VOR_VX, VOR_VI]>;
defm : PatVXIBin<DivergentBinFrag<xor>,  [VXOR_VV, VXOR_VX, VXOR_VI]>;
defm : PatVXIBin<DivergentBinFrag<shl>,  [VSLL_VV, VSLL_VX, VSLL_VI], uimm5>;
defm : PatVXIBin<DivergentBinFrag<srl>,  [VSRL_VV, VSRL_VX, VSRL_VI], uimm5>;
defm : PatVXIBin<DivergentBinFrag<sra>,  [VSRA_VV, VSRA_VX, VSRA_VI], uimm5>;
defm : PatVXIBin<DivergentBinFrag<mul>,  [VMUL_VV, VMUL_VX]>;
defm : PatVXIBin<DivergentBinFrag<mulhs>,[VMULH_VV, VMULH_VX]>;
defm : PatVXIBin<DivergentBinFrag<riscv_mulhsu>,[VMULHSU_VV, VMULHSU_VX]>;
defm : PatVXIBin<DivergentBinFrag<mulhu>,[VMULHU_VV, VMULHU_VX]>;
defm : PatVXIBin<DivergentBinFrag<sdiv>, [VDIV_VV, VDIV_VX]>;
defm : PatVXIBin<DivergentBinFrag<udiv>, [VDIVU_VV, VDIVU_VX]>;
defm : PatVXIBin<DivergentBinFrag<srem>, [VREM_VV, VREM_VX]>;
defm : PatVXIBin<DivergentBinFrag<urem>, [VREMU_VV, VREMU_VX]>;
defm : PatVXIBin<DivergentBinFrag<setlt>, [VMSLT_VV, VMSLT_VX]>;
defm : PatVXIBin<DivergentBinFrag<setult>,
                        [VMSLTU_VV, VMSLTU_VX], uimm5>;
defm : PatVXIBin<DivergentBinFrag<setne>, [VMSNE_VV, VMSNE_VX, VMSNE_VI]>;
defm : PatVXIBin<DivergentBinFrag<seteq>, [VMSEQ_VV, VMSEQ_VX, VMSEQ_VI]>;
// Patterns for vrsub.vx and vrsub.vi
def : Pat<(sub GPR:$rs1, VGPR:$rs2), (VRSUB_VX VGPR:$rs2, GPR:$rs1)>;
def : Pat<(XLenVT (sub simm5:$imm, (XLenVT VGPR:$rs1))),
              (VRSUB_VI VGPR:$rs1, simm5:$imm)>;

// For now, some instructions are aliaed to other instructions
defm : SleOpePatVXIBin<[DivergentBinFrag<xor>, DivergentBinFrag<setlt>],
                                      [VMSLE_VV, VMSLE_VX, VMSGT_VI], simm5>;
defm : SleOpePatVXIBin<[DivergentBinFrag<xor>, DivergentBinFrag<setult>],
                                      [VMSLEU_VV, VMSLEU_VX, VMSGTU_VI], uimm5>;


defm : PatFloatSetCC<[VGPR, VGPR], [SETOEQ, SETEQ], VMFEQ_VV>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETOEQ, SETEQ], VMFEQ_VF>;
defm : PatFloatSetCC<[VGPR, VGPR], [SETUNE, SETNE], VMFNE_VV>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETUNE, SETNE], VMFNE_VF>;
defm : PatFloatSetCC<[VGPR, VGPR], [SETOLT, SETLT], VMFLT_VV>;
defm : PatFloatSetCC<[VGPR, VGPR], [SETOLT, SETLT], VMFLT_VV>;
defm : PatFloatSetCC<[VGPR, VGPR], [SETLT], VMFLT_VV>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETOLT, SETLT], VMFLT_VF>;
defm : PatFloatSetCC<[VGPR, VGPR], [SETOLE, SETLE], VMFLE_VV>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETOLE, SETLE], VMFLE_VV>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETOGT, SETGT], VMFGT_VF>;
defm : PatFloatSetCC<[VGPR, GPRF32], [SETOGE, SETGE], VMFGE_VF>;

def : Pat<(i32 (DivergentBinFrag<riscv_fcvt_x> (f32 VGPR:$rs1), timm:$frm)),
                (VFCVT_RTZ_X_F_V (f32 VGPR:$rs1), $frm)>;
def : Pat<(i32 (DivergentBinFrag<riscv_fcvt_xu> (f32 VGPR:$rs1), timm:$frm)),
                (VFCVT_RTZ_XU_F_V (f32 VGPR:$rs1), $frm)>;
def : PatFXConvert<DivergentUnaryFrag<any_fp_to_sint>,
                          [XLenVT, f32], VFCVT_X_F_V>;
def : PatFXConvert<DivergentUnaryFrag<any_fp_to_uint>,
                          [XLenVT, f32], VFCVT_XU_F_V>;
def : PatFXConvert<DivergentUnaryFrag<any_sint_to_fp>,
                          [f32, XLenVT], VFCVT_F_X_V>;
def : PatFXConvert<DivergentUnaryFrag<any_uint_to_fp>,
                        [f32, XLenVT], VFCVT_F_XU_V>;
defm : PatVFRBin<DivergentBinFrag<fadd>,  [VFADD_VV, VFADD_VF]>;
// def : Pat<(fadd GPRF32:$rs1, GPRF32:$rs2), (FADD_S $rs1, $rs2)>;
defm : PatVFRBin<DivergentBinFrag<fsub>,  [VFSUB_VV, VFSUB_VF]>;
defm : PatVFRBin<DivergentBinFrag<fmul>,  [VFMUL_VV, VFMUL_VF]>;
defm : PatVFRBin<DivergentBinFrag<fdiv>,  [VFDIV_VV, VFDIV_VF, VFRDIV_VF]>;
defm : PatVFRBin<DivergentBinFrag<fminnum>,  [VFMIN_VV, VFMIN_VF]>;
defm : PatVFRBin<DivergentBinFrag<fmaxnum>,  [VFMAX_VV, VFMAX_VF]>;
defm : PatVFRBin<DivergentBinFrag<fcopysign>, [VFSGNJ_VV, VFSGNJ_VF]>;
def : Pat<(fcopysign (f32 VGPR:$rs1), (f32 (fneg VGPR:$rs2))),
                                    (VFSGNJN_VV $rs1, $rs2)>;
def : Pat<(fcopysign (f32 VGPR:$rs1), (fneg GPRF32:$rs2)),
                                (VFSGNJN_VF $rs1, $rs2)>;
def : Pat<(fneg (f32 VGPR:$rs1)), (VFSGNJN_VV $rs1, $rs1)>;
def : Pat<(fabs (f32 VGPR:$rs1)), (VFSGNJX_VV $rs1, $rs1)>;

// Patterns for ternary operations
// TODO: vmacc/vfmacc, vnmsac/vfnmsac
defm : PatVXFTer<[DivergentBinFrag<fadd>, DivergentBinFrag<fmul>], f32,
                                            GPRF32, [VFMADD_VV, VFMADD_VF]>;
defm : PatVXFTer<[DivergentBinFrag<fsub>, DivergentBinFrag<fmul>], f32,
                    GPRF32, [VFMSUB_VV, VFMSUB_VF, VFNMADD_VV, VFNMADD_VF]>;
defm : PatVXFTer<[ReverseDivergentBinFrag<fsub>, DivergentBinFrag<fmul>],
                                   f32, GPRF32, [VFNMSUB_VV, VFNMSUB_VF]>;
defm : PatVXFTer<[DivergentBinFrag<add>, DivergentBinFrag<mul>], XLenVT,
                                     GPR, [VMADD_VV, VMADD_VX]>;
defm : PatVXFTer<[ReverseDivergentBinFrag<sub>, DivergentBinFrag<mul>],
                                  XLenVT, GPR, [VNMSUB_VV, VNMSUB_VX]>;
// TODO: vfrec7.v? what is this
def : Pat<(any_fsqrt (f32 VGPR:$rs1)), (VFSQRT_V (f32 VGPR:$rs1))>;

// fmadd: rs1 * rs2 + rs3
def : Pat<(DivergentTernaryFrag<any_fma>  (f32 VGPR:$vd), (f32 VGPR:$vs1),
      (f32 VGPR:$vs2)), (VFMADD_VV $vd, $vs1, $vs2)>;

def DivergentSelectCCFrag : PatFrag<(ops node:$lhs, node:$rhs, node:$cc,
                                       node:$truev, node:$falsev),
                                    (riscv_selectcc node:$lhs, node:$rhs,
                                                    node:$cc, node:$truev,
                                                    node:$falsev),
                                    [{ return N->isDivergent(); }],
                                    IntCCtoDivergentCC>;
def NonDivergentSelectCCFrag : PatFrag<(ops node:$lhs, node:$rhs, node:$cc,
                                       node:$truev, node:$falsev),
                                    (riscv_selectcc node:$lhs, node:$rhs,
                                                    node:$cc, node:$truev,
                                                    node:$falsev),
                                    [{ return !N->isDivergent(); }],
                                    IntCCtoRISCVCC>;


let Predicates = [HasShortForwardBranchOpt],
    Constraints = "$dst = $falsev", isCommutable = 1, Size = 8 in {
// This instruction moves $truev to $dst when the condition is true. It will
// be expanded to control flow in RISCVExpandPseudoInsts.
def PseudoCCMOVVGPR : Pseudo<(outs VGPR:$dst),
                             (ins VGPR:$lhs, VGPR:$rhs, ixlenimm:$cc,
                              VGPR:$truev, VGPR:$falsev),
                             [(set (XLenVT VGPR:$dst),
                               (DivergentSelectCCFrag:$cc
                                (XLenVT VGPR:$lhs), (XLenVT VGPR:$rhs), cond,
                                (XLenVT VGPR:$truev), (XLenVT VGPR:$falsev)))]>,
                     Sched<[WriteSFB, ReadSFB, ReadSFB, ReadSFB, ReadSFB]>;
}

let usesCustomInserter = 1, Predicates = [NoShortForwardBranchOpt] in
def Select_VGPR_Using_CC_VGPR : Pseudo<(outs VGPR:$dst),
                            (ins VGPR:$lhs, VGPR:$rhs, ixlenimm:$cc,
                             VGPR:$truev, VGPR:$falsev),
                            []>;

// FIXME: Bug in SelectionDAGISel::SelectCodeCommon ? Remove IntCCtoDivergentCC.
def : Pat<(i32 (DivergentSelectCCFrag:$cc (XLenVT VGPR:$lhs), (XLenVT VGPR:$rhs),
            cond, (XLenVT VGPR:$truev), (XLenVT VGPR:$falsev))),
          (Select_VGPR_Using_CC_VGPR VGPR:$lhs, VGPR:$rhs,
            (IntCCtoDivergentCC $cc), VGPR:$truev, VGPR:$falsev)>;
def : Pat<(f32 (DivergentSelectCCFrag:$cc (XLenVT VGPR:$lhs), (XLenVT VGPR:$rhs),
            cond, (f32 VGPR:$truev), (f32 VGPR:$falsev))),
          (Select_VGPR_Using_CC_VGPR VGPR:$lhs, VGPR:$rhs,
            (IntCCtoDivergentCC $cc), VGPR:$truev, VGPR:$falsev)>;

def Select_GPRF32_Using_CC_GPRF32 : Pseudo<(outs GPRF32:$dst),
                            (ins GPRF32:$lhs, GPRF32:$rhs, ixlenimm:$cc,
                             GPRF32:$truev, GPRF32:$falsev),
                            [(set f32:$dst,
                              (NonDivergentSelectCCFrag:$cc
                               (f32 GPRF32:$lhs), (f32 GPRF32:$rhs), cond,
                               (f32 GPRF32:$truev), (f32 GPRF32:$falsev)))]>;

// Match `riscv_brcc` and lower to the appropriate RISC-V branch instruction.
class DivergentBccPat<CondCode Cond, RVInstVB Inst>
    : Pat<(DivergentTetradFrag<riscv_brcc>
           (XLenVT VGPR:$vs1), (XLenVT VGPR:$vs2), Cond, bb:$imm12),
           (Inst VGPR:$vs1, VGPR:$vs2, simm13_lsb0:$imm12)>;

def : DivergentBccPat<SETEQ, VBEQ>;
def : DivergentBccPat<SETNE, VBNE>;
def : DivergentBccPat<SETLT, VBLT>;
def : DivergentBccPat<SETGE, VBGE>;
def : DivergentBccPat<SETULT, VBLTU>;
def : DivergentBccPat<SETUGE, VBGEU>;

// Ventus barrier instructions pattern

let Predicates = [NoSubgroupFlag] in {

  def : Pat<(int_riscv_ventus_barrier timm:$uimm5), (BARRIER $uimm5)>;
  def PseudoBarrier : Pseudo<(outs), (ins i32imm:$uimm1, i32imm:$uimm2),
           [(int_riscv_ventus_barrier_with_scope timm:$uimm1, timm:$uimm2)]>;
} // Predicates = [NoSubgroupBarrierFlag]

let Predicates = [HasSubgroupFlag] in {

def : Pat<(int_riscv_ventus_barrier timm:$uimm5), (SUBGROUP_BARRIER $uimm5)>;
def PseudoSubGroupBarrier : Pseudo<(outs), (ins i32imm:$uimm1, i32imm:$uimm2),
           [(int_riscv_ventus_barrier_with_scope timm:$uimm1, timm:$uimm2)]>;
} // Predicates = [HasSubgroupBarrierFlag]

// TODO: Fix it!
/*
let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    RVVConstraint = NoConstraint in {
// A future extension may relax the vector register alignment restrictions.
foreach n = [1] in {
  defvar vrc = !cast<RegisterClass>(!if(!eq(n, 1), "VR", "VRM"#n));
  def VMV#n#R_V  : RVInstV<0b100111, !add(n, -1), OPIVI, (outs vrc:$vd),
                           (ins vrc:$vs2), "vmv" # n # "r.v", "$vd, $vs2">,
                   VMVRSched<n> {
    let Uses = [];
  }
}
} // hasSideEffects = 0, mayLoad = 0, mayStore = 0
} // Predicates = [HasVInstructions]
*/

// TODO: Will enable this after non divergent execution path are implemented.
// include "VentusInstrInfoVPseudos.td"

//===----------------------------------------------------------------------===//
// Ventus vALU divergent extended execution patterns
//===----------------------------------------------------------------------===//
def : Pat<(XLenVT (DivergentBinFrag<sub> (XLenVT VGPR:$rs1), uimm12:$imm)),
              (XLenVT (VSUBIMM12 VGPR:$rs1, uimm12:$imm))>;
def : Pat<(XLenVT (DivergentBinFrag<add> (XLenVT VGPR:$rs1), uimm12:$imm)),
              (XLenVT (VADDIMM12 VGPR:$rs1, uimm12:$imm))>;

// There already has patterns defined in VentusInstrInfo.td
let Predicates = [HasStdExtZfinx] in {
// def : Pat<(f32 (bitconvert (i32 GPR:$src))), (VMV_V_X GPR:$src)>;
def : Pat<(i32 (bitconvert GPRF32:$src)), (VFMV_V_F GPRF32:$src)>;
} // Predicates = [HasStdExtZfinx]
