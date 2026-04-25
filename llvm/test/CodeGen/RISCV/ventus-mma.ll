; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare { i32, i32, i32, i32 } @llvm.riscv.ventus.mma.m16n8k16.row.col.f32.f16.f16.f32(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)

define i32 @test(i32 %a0, i32 %a1, i32 %a2, i32 %a3, i32 %b0, i32 %b1, float %c0, float %c1, float %c2, float %c3) {
; CHECK-LABEL: test:
; CHECK: mma.m16n8k16.row.col.f32.f16.f16.f32
  %c0i = bitcast float %c0 to i32
  %c1i = bitcast float %c1 to i32
  %c2i = bitcast float %c2 to i32
  %c3i = bitcast float %c3 to i32
  %d = call { i32, i32, i32, i32 } @llvm.riscv.ventus.mma.m16n8k16.row.col.f32.f16.f16.f32(
      i32 %a0, i32 %a1, i32 %a2, i32 %a3,
      i32 %b0, i32 %b1,
      i32 %c0i, i32 %c1i, i32 %c2i, i32 %c3i)
  %d0 = extractvalue { i32, i32, i32, i32 } %d, 0
  ret i32 %d0
}
