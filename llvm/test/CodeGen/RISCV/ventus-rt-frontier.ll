; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.ventus.rt.traverse.v5.i32(i32, i32)
declare i32 @llvm.riscv.ventus.rt.completion.frontier.i32(i32)

define i32 @test_rt_frontier_order(ptr addrspace(1) %pds,
                                   i32 %pds_address,
                                   i32 %private_address) {
; CHECK-LABEL: test_rt_frontier_order:
; CHECK: vt.rt.traverse [[RESULT:v[0-9]+]],
; CHECK: vor.vi [[RESULT]], [[RESULT]], 0
; CHECK-NEXT: vlw{{[0-9]*}}.v
  %compact = call i32 @llvm.riscv.ventus.rt.traverse.v5.i32(
      i32 %pds_address, i32 %private_address)
  %frontier = call i32
      @llvm.riscv.ventus.rt.completion.frontier.i32(i32 %compact)
  %published = load volatile i32, ptr addrspace(1) %pds, align 4
  %result = add i32 %frontier, %published
  ret i32 %result
}
