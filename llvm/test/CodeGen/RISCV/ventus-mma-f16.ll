; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare { i32, i32 } @llvm.riscv.ventus.mma.m16n8k16.row.col.f16.f16.f16.f16(i32, i32, i32, i32, i32, i32, i32, i32)

define i32 @test(i32 %a0, i32 %a1, i32 %a2, i32 %a3, i32 %b0, i32 %b1, i32 %c0, i32 %c1) {
; CHECK-LABEL: test:
; CHECK: mma.m16n8k16.row.col.f16.f16.f16.f16
  %d = call { i32, i32 } @llvm.riscv.ventus.mma.m16n8k16.row.col.f16.f16.f16.f16(
      i32 %a0, i32 %a1, i32 %a2, i32 %a3,
      i32 %b0, i32 %b1,
      i32 %c0, i32 %c1)
  %d0 = extractvalue { i32, i32 } %d, 0
  ret i32 %d0
}
