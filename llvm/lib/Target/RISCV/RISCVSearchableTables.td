//===-- RISCVSearchableTables.td ---------------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Resource intrinsics table.
//===----------------------------------------------------------------------===//

class RsrcIntrinsic<RISCVRsrcIntrinsic intr> {
  Intrinsic Intr = !cast<Intrinsic>(intr);
  bits<8> RsrcArg = intr.RsrcArg;
  bit IsImage = intr.IsImage;
}
/*
def RsrcIntrinsics : GenericTable {
  let FilterClass = "RsrcIntrinsic";
  let Fields = ["Intr", "RsrcArg", "IsImage"];

  let PrimaryKey = ["Intr"];
  let PrimaryKeyName = "lookupRsrcIntrinsic";
}

foreach intr = !listconcat(RISCVBufferIntrinsics,
                           RISCVImageDimIntrinsics,
                           RISCVImageDimAtomicIntrinsics) in {
  def : RsrcIntrinsic<!cast<RISCVRsrcIntrinsic>(intr)>;
}
*/
class VentusBufferFormatBase<bits<8> f, bits<8> bpc, bits<8> numc,
                             bits<8> nfmt, bits<8> dfmt> {
  bits<8> Format = f;
  bits<8> BitsPerComp = bpc;
  bits<8> NumComponents = numc;
  bits<8> NumFormat = nfmt;
  bits<8> DataFormat = dfmt;
}

class VentusBufferFormatTable  : GenericTable {
  let CppTypeName = "GcnBufferFormatInfo";
  let Fields = ["Format", "BitsPerComp", "NumComponents", "NumFormat", "DataFormat"];
  let PrimaryKey = ["BitsPerComp", "NumComponents", "NumFormat"];
}

class SourceOfDivergence<Intrinsic intr> {
  Intrinsic Intr = intr;
}

def SourcesOfDivergence : GenericTable {
  let FilterClass = "SourceOfDivergence";
  let Fields = ["Intr"];

  let PrimaryKey = ["Intr"];
  let PrimaryKeyName = "lookupSourceOfDivergence";
}

def : SourceOfDivergence<int_riscv_workitem_id_x>;
def : SourceOfDivergence<int_riscv_workitem_id_y>;
def : SourceOfDivergence<int_riscv_workitem_id_z>;
//def : SourceOfDivergence<int_riscv_interp_mov>;
//def : SourceOfDivergence<int_riscv_interp_p1>;

// The dummy boolean output is divergent from the IR's perspective,
// but the mask results are uniform. These produce a divergent and
// uniform result, so the returned struct is collectively divergent.
// isAlwaysUniform can override the extract of the uniform component.
//def : SourceOfDivergence<int_riscv_if>;
//def : SourceOfDivergence<int_riscv_else>;
//def : SourceOfDivergence<int_riscv_loop>;

//foreach intr = RISCVImageDimAtomicIntrinsics in
//def : SourceOfDivergence<intr>;
