; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32"

; crt0 supplies the static prefix. Only the ordinary frame grows TP.
define ventus_kernel void @prefix_resource() #0 {
; CHECK-LABEL: prefix_resource:
; CHECK: addi tp, tp, 12
; CHECK-NOT: addi tp, tp, 308
; CHECK: vsw.v
  %slot = alloca i32, align 4, addrspace(5)
  store volatile i32 7, ptr addrspace(5) %slot, align 4
  ret void
}

; CHECK: .section .ventus.resource.prefix_resource,
; CHECK: .quad 12
attributes #0 = { "target-cpu"="ventus-gpgpu" "ventus-rt-private-prefix"="308" }
