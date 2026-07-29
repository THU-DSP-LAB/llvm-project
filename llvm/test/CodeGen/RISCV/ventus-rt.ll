; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.ventus.rt.traverse.i32(i32)
declare void @llvm.riscv.ventus.rt.release.i32(i32)

define i32 @test_rt_traverse(i32 %slot) {
; CHECK-LABEL: test_rt_traverse:
; CHECK: vt.rt.traverse v{{[0-9]+}}, v{{[0-9]+}}
  %status = call i32 @llvm.riscv.ventus.rt.traverse.i32(i32 %slot)
  ret i32 %status
}

define void @test_rt_release(i32 %slot) {
; CHECK-LABEL: test_rt_release:
; CHECK: vt.rt.release v{{[0-9]+}}
  call void @llvm.riscv.ventus.rt.release.i32(i32 %slot)
  ret void
}

define void @test_rt_order(ptr addrspace(3) %scratch, i32 %slot, i32 %value) {
; CHECK-LABEL: test_rt_order:
; CHECK: vsw{{[0-9]*}}.v
; CHECK-NEXT: vt.rt.traverse
; CHECK: vsw{{[0-9]*}}.v
; CHECK-NEXT: vt.rt.release
  store volatile i32 %value, ptr addrspace(3) %scratch, align 4
  %status = call i32 @llvm.riscv.ventus.rt.traverse.i32(i32 %slot)
  store volatile i32 %status, ptr addrspace(3) %scratch, align 4
  call void @llvm.riscv.ventus.rt.release.i32(i32 %slot)
  ret void
}
