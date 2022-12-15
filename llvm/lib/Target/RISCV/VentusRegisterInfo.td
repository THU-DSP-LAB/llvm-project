//===-- VentusRegisterInfo.td - Ventus Register defs -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Declarations that describe the RISC-V register files for Ventus GPGPU:
//  General purpose sALU registers: x0-x63
//  General purpose vALU registers: v0-v255
//===----------------------------------------------------------------------===//

class Indexes<int N> {
  list<int> all = [0,   1,  2,  3,  4,  5,  6 , 7,
                   8,   9, 10, 11, 12, 13, 14, 15];

  // Returns list of indexes [0..N)
  list<int> slice = !filter(i, all, !lt(i, N));
}

let Namespace = "RISCV" in {
// Single instruction can not encoding 2^8 registers, there will be a pass
// which converts register number greater than 31 to 2 instructions:
//   regext vd_base0, vs0_base0, vs1_base1
//   vALUop vd, vs0, vs1
// where the first instruction tells HW uses v[0-31] for vd, vs1,
// uses v[32-63] for vs0.
class RISCVReg<bits<8> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{7-0} = Enc;
  let AltNames = alt;
}

class RISCVReg16<bits<8> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{7-0} = Enc;
  let AltNames = alt;
}

def lo16 : SubRegIndex<16, 0>;
def hi16 : SubRegIndex<16, 16>;
def lo32 : SubRegIndex<32, 0>;
def hi32 : SubRegIndex<32, 32>;

class RISCVReg32<RISCVReg16 subreg> : Register<""> {
  let HWEncoding{7-0} = subreg.HWEncoding{7-0};
  let SubRegs = [subreg];
  let SubRegIndices = [lo16];
  let AsmName = subreg.AsmName;
  let AltNames = subreg.AltNames;
}

// Because RISCVReg64 register have AsmName and AltNames that alias with their
// 16/32-bit sub-register, RISCVAsmParser will need to coerce a register number
// from a RISCVReg16/RISCVReg32 to the equivalent RISCVReg64 when appropriate.
class RISCVReg64<RISCVReg32 subreg> : Register<""> {
  let HWEncoding{7-0} = subreg.HWEncoding{7-0};
  let SubRegs = [subreg];
  let SubRegIndices = [lo32];
  let AsmName = subreg.AsmName;
  let AltNames = subreg.AltNames;
}

class RISCVRegWithSubRegs<bits<8> Enc, string n, list<Register> subregs,
                          list<string> alt = []>
      : RegisterWithSubRegs<n, subregs> {
  let HWEncoding{7-0} = Enc;
  let AltNames = alt;
}

def ABIRegAltName : RegAltNameIndex;

foreach Index = 0...15 in {
  def sub#Index : SubRegIndex<32, !shl(Index, 5)>;
}

foreach Index = 1...15 in {
  def sub#Index#_lo16 : ComposedSubRegIndex<!cast<SubRegIndex>(sub#Index), lo16>;
  def sub#Index#_hi16 : ComposedSubRegIndex<!cast<SubRegIndex>(sub#Index), hi16>;
}

foreach Size = {2...6,8,16} in {
  foreach Index = Indexes<!sub(17, Size)>.slice in {
    def !interleave(!foreach(cur, Indexes<Size>.slice, "sub"#!add(cur, Index)),
                    "_") :
      SubRegIndex<!mul(Size, 32), !shl(Index, 5)> {
      let CoveringSubRegIndices =
        !foreach(cur, Indexes<Size>.slice,
                 !cast<SubRegIndex>(sub#!add(cur, Index)));
    }
  }
}

} // Namespace = "RISCV"

//===----------------------------------------------------------------------===//
//  Helpers
//===----------------------------------------------------------------------===//

class getSubRegs<int size> {
  list<SubRegIndex> ret2 = [sub0, sub1];
  list<SubRegIndex> ret3 = [sub0, sub1, sub2];
  list<SubRegIndex> ret4 = [sub0, sub1, sub2, sub3];
  list<SubRegIndex> ret8 = [sub0, sub1, sub2, sub3, sub4, sub5, sub6, sub7];
  list<SubRegIndex> ret16 = [sub0, sub1, sub2, sub3,
                             sub4, sub5, sub6, sub7,
                             sub8, sub9, sub10, sub11,
                             sub12, sub13, sub14, sub15];

  list<SubRegIndex> ret = !if(!eq(size, 2), ret2,
                              !if(!eq(size, 3), ret3,
                                  !if(!eq(size, 4), ret4,
                                      !if(!eq(size, 8), ret8,
                                          ret16))));
}


// Generates list of sequential register tuple names.
// E.g. RegSeq<3,2,2,"v">.ret -> [ "v[0:1]", "v[2:3]" ]
class RegSeqNames<int last_reg, int stride, int size, string prefix,
                  int start = 0> {
  int next = !add(start, stride);
  int end_reg = !add(start, size, -1);
  list<string> ret =
    !if(!le(end_reg, last_reg),
        !listconcat([prefix # "[" # start # ":" # end_reg # "]"],
                    RegSeqNames<last_reg, stride, size, prefix, next>.ret),
                    []);
}

// Generates list of dags for register tuples.
class RegSeqDags<RegisterClass RC, int last_reg, int stride, int size,
                int start = 0> {
  dag trunc_rc = (trunc RC,
                  !if(!and(!eq(stride, 1), !eq(start, 0)),
                      !sub(!add(last_reg, 2), size),
                      !add(last_reg, 1)));
  list<dag> ret =
    !if(!lt(start, size),
        !listconcat([(add (decimate (shl trunc_rc, start), stride))],
                    RegSeqDags<RC, last_reg, stride, size, !add(start, 1)>.ret),
        []);
}

class VentusRegisterTuples<list<SubRegIndex> Indices, RegisterClass RC,
                           int last_reg, int stride, int size, string prefix> :
  RegisterTuples<Indices,
                 RegSeqDags<RC, last_reg, stride, size>.ret,
                 RegSeqNames<last_reg, stride, size, prefix>.ret>;

// Integer registers
// CostPerUse is set higher for registers that may not be compressible as they
// are not part of GPRC, the most restrictive register class used by the
// compressed instruction set. This will influence the greedy register
// allocator to reduce the use of registers that can't be encoded in 16 bit
// instructions.

let RegAltNameIndices = [ABIRegAltName] in {
  let isConstant = true in
  def X0  : RISCVReg<0, "x0", ["zero"]>, DwarfRegNum<[0]>;
  let CostPerUse = [0, 1] in {
  def X1  : RISCVReg<1, "x1", ["ra"]>, DwarfRegNum<[1]>;
  def X2  : RISCVReg<2, "x2", ["sp"]>, DwarfRegNum<[2]>;
  def X3  : RISCVReg<3, "x3", ["gp"]>, DwarfRegNum<[3]>;
  def X4  : RISCVReg<4, "x4", ["tp"]>, DwarfRegNum<[4]>;
  def X5  : RISCVReg<5, "x5", ["t0"]>, DwarfRegNum<[5]>;
  def X6  : RISCVReg<6, "x6", ["t1"]>, DwarfRegNum<[6]>;
  def X7  : RISCVReg<7, "x7", ["t2"]>, DwarfRegNum<[7]>;
  }
  def X8  : RISCVReg<8, "x8", ["s0", "fp"]>, DwarfRegNum<[8]>;
  def X9  : RISCVReg<9, "x9", ["s1"]>, DwarfRegNum<[9]>;
  def X10 : RISCVReg<10,"x10", ["a0"]>, DwarfRegNum<[10]>;
  def X11 : RISCVReg<11,"x11", ["a1"]>, DwarfRegNum<[11]>;
  def X12 : RISCVReg<12,"x12", ["a2"]>, DwarfRegNum<[12]>;
  def X13 : RISCVReg<13,"x13", ["a3"]>, DwarfRegNum<[13]>;
  def X14 : RISCVReg<14,"x14", ["a4"]>, DwarfRegNum<[14]>;
  def X15 : RISCVReg<15,"x15", ["a5"]>, DwarfRegNum<[15]>;
  let CostPerUse = [0, 1] in {
  def X16 : RISCVReg<16,"x16", ["a6"]>, DwarfRegNum<[16]>;
  def X17 : RISCVReg<17,"x17", ["a7"]>, DwarfRegNum<[17]>;
  def X18 : RISCVReg<18,"x18", ["s2"]>, DwarfRegNum<[18]>;
  def X19 : RISCVReg<19,"x19", ["s3"]>, DwarfRegNum<[19]>;
  def X20 : RISCVReg<20,"x20", ["s4"]>, DwarfRegNum<[20]>;
  def X21 : RISCVReg<21,"x21", ["s5"]>, DwarfRegNum<[21]>;
  def X22 : RISCVReg<22,"x22", ["s6"]>, DwarfRegNum<[22]>;
  def X23 : RISCVReg<23,"x23", ["s7"]>, DwarfRegNum<[23]>;
  def X24 : RISCVReg<24,"x24", ["s8"]>, DwarfRegNum<[24]>;
  def X25 : RISCVReg<25,"x25", ["s9"]>, DwarfRegNum<[25]>;
  def X26 : RISCVReg<26,"x26", ["s10"]>, DwarfRegNum<[26]>;
  def X27 : RISCVReg<27,"x27", ["s11"]>, DwarfRegNum<[27]>;
  def X28 : RISCVReg<28,"x28", ["t3"]>, DwarfRegNum<[28]>;
  def X29 : RISCVReg<29,"x29", ["t4"]>, DwarfRegNum<[29]>;
  def X30 : RISCVReg<30,"x30", ["t5"]>, DwarfRegNum<[30]>;
  def X31 : RISCVReg<31,"x31", ["t6"]>, DwarfRegNum<[31]>;
  def X32 : RISCVReg<32,"x32", ["t7"]>, DwarfRegNum<[32]>;
  def X33 : RISCVReg<33,"x33", ["t8"]>, DwarfRegNum<[33]>;
  def X34 : RISCVReg<34,"x34", ["t9"]>, DwarfRegNum<[34]>;
  def X35 : RISCVReg<35,"x35", ["t10"]>, DwarfRegNum<[35]>;
  def X36 : RISCVReg<36,"x36", ["t12"]>, DwarfRegNum<[36]>;
  def X37 : RISCVReg<37,"x37", ["t13"]>, DwarfRegNum<[37]>;
  def X38 : RISCVReg<38,"x38", ["t14"]>, DwarfRegNum<[38]>;
  def X39 : RISCVReg<39,"x39", ["t15"]>, DwarfRegNum<[39]>;
  def X40 : RISCVReg<40,"x40", ["t16"]>, DwarfRegNum<[40]>;
  def X41 : RISCVReg<41,"x41", ["t17"]>, DwarfRegNum<[41]>;
  def X42 : RISCVReg<42,"x42", ["t18"]>, DwarfRegNum<[42]>;
  def X43 : RISCVReg<43,"x43", ["t19"]>, DwarfRegNum<[43]>;
  def X44 : RISCVReg<44,"x44", ["t20"]>, DwarfRegNum<[44]>;
  def X45 : RISCVReg<45,"x45", ["t21"]>, DwarfRegNum<[45]>;
  def X46 : RISCVReg<46,"x46", ["t22"]>, DwarfRegNum<[46]>;
  def X47 : RISCVReg<47,"x47", ["t23"]>, DwarfRegNum<[47]>;
  def X48 : RISCVReg<48,"x48", ["t24"]>, DwarfRegNum<[48]>;
  def X49 : RISCVReg<49,"x49", ["t25"]>, DwarfRegNum<[49]>;
  def X50 : RISCVReg<50,"x50", ["t26"]>, DwarfRegNum<[50]>;
  def X51 : RISCVReg<51,"x51", ["t27"]>, DwarfRegNum<[51]>;
  def X52 : RISCVReg<52,"x52", ["t28"]>, DwarfRegNum<[52]>;
  def X53 : RISCVReg<53,"x53", ["t29"]>, DwarfRegNum<[53]>;
  def X54 : RISCVReg<54,"x54", ["t30"]>, DwarfRegNum<[54]>;
  def X55 : RISCVReg<55,"x55", ["t31"]>, DwarfRegNum<[55]>;
  def X56 : RISCVReg<56,"x56", ["t32"]>, DwarfRegNum<[56]>;
  def X57 : RISCVReg<57,"x57", ["t33"]>, DwarfRegNum<[57]>;
  def X58 : RISCVReg<58,"x58", ["t34"]>, DwarfRegNum<[58]>;
  def X59 : RISCVReg<59,"x59", ["t35"]>, DwarfRegNum<[59]>;
  def X60 : RISCVReg<60,"x60", ["t36"]>, DwarfRegNum<[60]>;
  def X61 : RISCVReg<61,"x61", ["t37"]>, DwarfRegNum<[61]>;
  def X62 : RISCVReg<62,"x62", ["t38"]>, DwarfRegNum<[62]>;
  def X63 : RISCVReg<63,"x63", ["t39"]>, DwarfRegNum<[63]>;
  }
}

def XLenVT : ValueTypeByHwMode<[RV32, RV64],
                               [i32,  i64]>;
def XLenRI : RegInfoByHwMode<
      [RV32,              RV64],
      [RegInfo<32,32,32>, RegInfo<64,64,64>]>;

// The order of registers represents the preferred allocation sequence.
// Registers are listed in the order caller-save, callee-save, specials.
def GPR : RegisterClass<"RISCV", [XLenVT], 32, (add
    (sequence "X%u", 10, 17),
    (sequence "X%u", 5, 7),
    (sequence "X%u", 28, 31),
    (sequence "X%u", 8, 9),
    (sequence "X%u", 18, 27),
    (sequence "X%u", 0, 4)
  )> {
  let RegInfos = XLenRI;
}

def GPRX0 : RegisterClass<"RISCV", [XLenVT], 32, (add X0)> {
  let RegInfos = XLenRI;
}

def GPRNoX0 : RegisterClass<"RISCV", [XLenVT], 32, (sub GPR, X0)> {
  let RegInfos = XLenRI;
}

def GPRNoX0X2 : RegisterClass<"RISCV", [XLenVT], 32, (sub GPR, X0, X2)> {
  let RegInfos = XLenRI;
}

// Don't use X1 or X5 for JALR since that is a hint to pop the return address
// stack on some microarchitectures. Also remove the reserved registers X0, X2,
// X3, and X4 as it reduces the number of register classes that get synthesized
// by tablegen.
def GPRJALR : RegisterClass<"RISCV", [XLenVT], 32, (sub GPR, (sequence "X%u", 0, 5))> {
  let RegInfos = XLenRI;
}

def GPRC : RegisterClass<"RISCV", [XLenVT], 32, (add
    (sequence "X%u", 10, 15),
    (sequence "X%u", 8, 9)
  )> {
  let RegInfos = XLenRI;
}

// For indirect tail calls, we can't use callee-saved registers, as they are
// restored to the saved value before the tail call, which would clobber a call
// address. We shouldn't use x5 since that is a hint for to pop the return
// address stack on some microarchitectures.
def GPRTC : RegisterClass<"RISCV", [XLenVT], 32, (add
    (sequence "X%u", 6, 7),
    (sequence "X%u", 10, 17),
    (sequence "X%u", 28, 31)
  )> {
  let RegInfos = XLenRI;
}

def SP : RegisterClass<"RISCV", [XLenVT], 32, (add X2)> {
  let RegInfos = XLenRI;
}

// Floating point registers
let RegAltNameIndices = [ABIRegAltName] in {
  def F0_H  : RISCVReg16<0, "f0", ["ft0"]>, DwarfRegNum<[32]>;
  def F1_H  : RISCVReg16<1, "f1", ["ft1"]>, DwarfRegNum<[33]>;
  def F2_H  : RISCVReg16<2, "f2", ["ft2"]>, DwarfRegNum<[34]>;
  def F3_H  : RISCVReg16<3, "f3", ["ft3"]>, DwarfRegNum<[35]>;
  def F4_H  : RISCVReg16<4, "f4", ["ft4"]>, DwarfRegNum<[36]>;
  def F5_H  : RISCVReg16<5, "f5", ["ft5"]>, DwarfRegNum<[37]>;
  def F6_H  : RISCVReg16<6, "f6", ["ft6"]>, DwarfRegNum<[38]>;
  def F7_H  : RISCVReg16<7, "f7", ["ft7"]>, DwarfRegNum<[39]>;
  def F8_H  : RISCVReg16<8, "f8", ["fs0"]>, DwarfRegNum<[40]>;
  def F9_H  : RISCVReg16<9, "f9", ["fs1"]>, DwarfRegNum<[41]>;
  def F10_H : RISCVReg16<10,"f10", ["fa0"]>, DwarfRegNum<[42]>;
  def F11_H : RISCVReg16<11,"f11", ["fa1"]>, DwarfRegNum<[43]>;
  def F12_H : RISCVReg16<12,"f12", ["fa2"]>, DwarfRegNum<[44]>;
  def F13_H : RISCVReg16<13,"f13", ["fa3"]>, DwarfRegNum<[45]>;
  def F14_H : RISCVReg16<14,"f14", ["fa4"]>, DwarfRegNum<[46]>;
  def F15_H : RISCVReg16<15,"f15", ["fa5"]>, DwarfRegNum<[47]>;
  def F16_H : RISCVReg16<16,"f16", ["fa6"]>, DwarfRegNum<[48]>;
  def F17_H : RISCVReg16<17,"f17", ["fa7"]>, DwarfRegNum<[49]>;
  def F18_H : RISCVReg16<18,"f18", ["fs2"]>, DwarfRegNum<[50]>;
  def F19_H : RISCVReg16<19,"f19", ["fs3"]>, DwarfRegNum<[51]>;
  def F20_H : RISCVReg16<20,"f20", ["fs4"]>, DwarfRegNum<[52]>;
  def F21_H : RISCVReg16<21,"f21", ["fs5"]>, DwarfRegNum<[53]>;
  def F22_H : RISCVReg16<22,"f22", ["fs6"]>, DwarfRegNum<[54]>;
  def F23_H : RISCVReg16<23,"f23", ["fs7"]>, DwarfRegNum<[55]>;
  def F24_H : RISCVReg16<24,"f24", ["fs8"]>, DwarfRegNum<[56]>;
  def F25_H : RISCVReg16<25,"f25", ["fs9"]>, DwarfRegNum<[57]>;
  def F26_H : RISCVReg16<26,"f26", ["fs10"]>, DwarfRegNum<[58]>;
  def F27_H : RISCVReg16<27,"f27", ["fs11"]>, DwarfRegNum<[59]>;
  def F28_H : RISCVReg16<28,"f28", ["ft8"]>, DwarfRegNum<[60]>;
  def F29_H : RISCVReg16<29,"f29", ["ft9"]>, DwarfRegNum<[61]>;
  def F30_H : RISCVReg16<30,"f30", ["ft10"]>, DwarfRegNum<[62]>;
  def F31_H : RISCVReg16<31,"f31", ["ft11"]>, DwarfRegNum<[63]>;

  foreach Index = 0-31 in {
    def F#Index#_F : RISCVReg32<!cast<RISCVReg16>("F"#Index#"_H")>,
      DwarfRegNum<[!add(Index, 32)]>;
  }

  foreach Index = 0-31 in {
    def F#Index#_D : RISCVReg64<!cast<RISCVReg32>("F"#Index#"_F")>,
      DwarfRegNum<[!add(Index, 32)]>;
  }
}

// The order of registers represents the preferred allocation sequence,
// meaning caller-save regs are listed before callee-save.
def FPR16 : RegisterClass<"RISCV", [f16], 16, (add
    (sequence "F%u_H", 0, 7),
    (sequence "F%u_H", 10, 17),
    (sequence "F%u_H", 28, 31),
    (sequence "F%u_H", 8, 9),
    (sequence "F%u_H", 18, 27)
)>;

def FPR32 : RegisterClass<"RISCV", [f32], 32, (add
    (sequence "F%u_F", 0, 7),
    (sequence "F%u_F", 10, 17),
    (sequence "F%u_F", 28, 31),
    (sequence "F%u_F", 8, 9),
    (sequence "F%u_F", 18, 27)
)>;

def FPR32C : RegisterClass<"RISCV", [f32], 32, (add
  (sequence "F%u_F", 10, 15),
  (sequence "F%u_F", 8, 9)
)>;

// The order of registers represents the preferred allocation sequence,
// meaning caller-save regs are listed before callee-save.
def FPR64 : RegisterClass<"RISCV", [f64], 64, (add
    (sequence "F%u_D", 0, 7),
    (sequence "F%u_D", 10, 17),
    (sequence "F%u_D", 28, 31),
    (sequence "F%u_D", 8, 9),
    (sequence "F%u_D", 18, 27)
)>;

def FPR64C : RegisterClass<"RISCV", [f64], 64, (add
  (sequence "F%u_D", 10, 15),
  (sequence "F%u_D", 8, 9)
)>;


// Vector registers
let RegAltNameIndices = [ABIRegAltName] in {
  def VTYPE  : RISCVReg<0, "vtype", ["vtype"]>;
  def VL     : RISCVReg<0, "vl", ["vl"]>;
  def VXSAT  : RISCVReg<0, "vxsat", ["vxsat"]>;
  def VXRM   : RISCVReg<0, "vxrm", ["vxrm"]>;
  let isConstant = true in
  def VLENB  : RISCVReg<0, "vlenb", ["vlenb"]>,
               DwarfRegNum<[!add(4096, SysRegVLENB.Encoding)]>;
}

def VCSR : RegisterClass<"RISCV", [XLenVT], 32,
                          (add VTYPE, VL, VLENB)> {
  let RegInfos = XLenRI;
}

let RegInfos = XLenRI in {
def GPRF16  : RegisterClass<"RISCV", [f16], 16, (add GPR)>;
def GPRF32  : RegisterClass<"RISCV", [f32], 32, (add GPR)>;
def GPRF64  : RegisterClass<"RISCV", [f64], 64, (add GPR)>;
} // RegInfos = XLenRI

let RegAltNameIndices = [ABIRegAltName] in {
  foreach Index = [0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22,
                   24, 26, 28, 30] in {
    defvar Reg = !cast<Register>("X"#Index);
    def X#Index#_PD : RISCVRegWithSubRegs<Index, Reg.AsmName,
                                          [!cast<Register>("X"#Index),
                                           !cast<Register>("X"#!add(Index, 1))],
                                           Reg.AltNames> {
      let SubRegIndices = [lo32, hi32];
    }
  }
}

let RegInfos = RegInfoByHwMode<[RV64], [RegInfo<64, 64, 64>]> in
def GPRPF64 : RegisterClass<"RISCV", [f64], 64, (add
    X10_PD, X12_PD, X14_PD, X16_PD,
    X6_PD,
    X28_PD, X30_PD,
    X8_PD,
    X18_PD, X20_PD, X22_PD, X24_PD, X26_PD,
    X0_PD, X2_PD, X4_PD
)>;

// Special registers
def FFLAGS : RISCVReg<0, "fflags">;
def FRM    : RISCVReg<0, "frm">;

// TODO: Ref to SIRegisterInfo.td to finish this.
multiclass VentusRegLoHi16 <string n, bits<8> regIdx, bit ArtificialHigh = 1> {
  // There is no special encoding for 16 bit subregs, these are not real
  // registers but rather operands for instructions preserving other 16 bits
  // of the result or reading just 16 bits of a 32 bit VGPR.
  // It is encoded as a corresponding 32 bit register.
  // Non-VGPR register classes use it as we need to have matching subregisters
  // to move instructions and data between ALUs.
  def _LO16 : RISCVReg<regIdx, n#".l"> {
    //let HWEncoding{8} = HWEncodingHigh;
  }
  def _HI16 : RISCVReg<regIdx, !if(ArtificialHigh, "", n#".h")> {
    let isArtificial = ArtificialHigh;
    //let HWEncoding{8} = HWEncodingHigh;
  }
  def "" : RegisterWithSubRegs<n, [!cast<Register>(NAME#"_LO16"),
                                   !cast<Register>(NAME#"_HI16")]> {
    let Namespace = "RISCV";
    let SubRegIndices = [lo16, hi16];
    let CoveredBySubRegs = !not(ArtificialHigh);
    let HWEncoding{7-0} = regIdx;
    //let HWEncoding{8} = HWEncodingHigh;
  }
}

// FIXME: DwarfRegNum!!
// VGPR registers name
foreach Index = 0...255 in {
  defm V#Index :
    VentusRegLoHi16 <"v"#Index, Index, 0>,
    DwarfRegNum<[!add(Index, 2560), !add(Index, 1536)]>;
}

class RegisterTypes<list<ValueType> reg_types> {
  list<ValueType> types = reg_types;
}

def Reg16Types : RegisterTypes<[i16, f16]>;
def Reg32Types : RegisterTypes<[i32, f32, v2i16, v2f16]>;

// VGPR 32-bit registers class
def VGPR_32 : RegisterClass<"RISCV",
                            !listconcat(Reg32Types.types, Reg16Types.types),
                            32,
                            (add (sequence "V%u", 0, 255))>;

// VGPR 64-bit registers class
def VGPR_64 : VentusRegisterTuples<getSubRegs<2>.ret, VGPR_32, 255, 1, 2, "v">;

// VGPR 96-bit registers class
def VGPR_96 : VentusRegisterTuples<getSubRegs<3>.ret, VGPR_32, 255, 1, 3, "v">;

// VGPR 128-bit registers class
def VGPR_128 : VentusRegisterTuples<getSubRegs<4>.ret, VGPR_32, 255, 1, 4, "v">;

// VGPR 256-bit registers class
def VGPR_256 : VentusRegisterTuples<getSubRegs<8>.ret, VGPR_32, 255, 1, 8, "v">;

// VGPR 512-bit registers class
def VGPR_512 : VentusRegisterTuples<getSubRegs<16>.ret, VGPR_32, 255, 1, 16, "v">;

// Any type register. Used for .insn directives when we don't know what the
// register types could be.
// NOTE: The alignment and size are bogus values. The Size needs to be non-zero
// or tablegen will use "untyped" to determine the size which will assert.
let isAllocatable = 0 in
def AnyReg : RegisterClass<"RISCV", [untyped], 32,
                           (add (sequence "X%u", 0, 63),
                                (sequence "F%u_D", 0, 31),
                                (sequence "V%u", 0, 255))> {
  let Size = 32;
}
