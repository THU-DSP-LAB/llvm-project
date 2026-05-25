; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.ventus.shuffle.idx.i32(i32, i32 immarg)
declare i32 @llvm.riscv.ventus.shuffle.up.i32(i32, i32 immarg)
declare i32 @llvm.riscv.ventus.shuffle.down.i32(i32, i32 immarg)
declare i32 @llvm.riscv.ventus.shuffle.bfly.i32(i32, i32 immarg)

define i32 @test_shuffle_idx(i32 %src) {
; CHECK-LABEL: test_shuffle_idx:
; CHECK: shuffle.idx
  %out = call i32 @llvm.riscv.ventus.shuffle.idx.i32(i32 %src, i32 3)
  ret i32 %out
}

define i32 @test_shuffle_up(i32 %src) {
; CHECK-LABEL: test_shuffle_up:
; CHECK: shuffle.up
  %out = call i32 @llvm.riscv.ventus.shuffle.up.i32(i32 %src, i32 1)
  ret i32 %out
}

define i32 @test_shuffle_down(i32 %src) {
; CHECK-LABEL: test_shuffle_down:
; CHECK: shuffle.down
  %out = call i32 @llvm.riscv.ventus.shuffle.down.i32(i32 %src, i32 2)
  ret i32 %out
}

define i32 @test_shuffle_bfly(i32 %src) {
; CHECK-LABEL: test_shuffle_bfly:
; CHECK: shuffle.bfly
  %out = call i32 @llvm.riscv.ventus.shuffle.bfly.i32(i32 %src, i32 4)
  ret i32 %out
}
