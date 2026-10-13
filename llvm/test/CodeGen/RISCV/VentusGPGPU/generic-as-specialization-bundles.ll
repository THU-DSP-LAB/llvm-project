; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -O0 -stop-after=ventus-generic-as-specialization < %s | FileCheck %s
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -O1 -stop-after=ventus-generic-as-specialization < %s | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"

define dso_local ventus_kernel void @kernel_helper_clone_bundle() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_helper_clone_bundle(
; CHECK: %obj = alloca i32, align 4, addrspace(5)
; CHECK: call void @helper.ventus.as5_p0(ptr addrspace(5) %obj) [ "deopt"(i32 7) ]
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @helper(ptr %flat) [ "deopt"(i32 7) ]
  ret void
}

define dso_local void @helper(ptr %p) {
entry:
  store i32 1, ptr %p, align 4
  ret void
}

; CHECK-LABEL: define dso_local void @helper.ventus.as5_p0(
; CHECK-SAME: ptr addrspace(5) %p
; CHECK: store i32 1, ptr addrspace(5) %p, align 4
