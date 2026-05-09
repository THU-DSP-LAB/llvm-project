; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -O0 -stop-after=ventus-generic-as-specialization < %s | FileCheck %s
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -O1 -stop-after=ventus-generic-as-specialization < %s | FileCheck %s
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -O1 < %s | FileCheck %s --check-prefix=ASM

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"

%struct.S = type { i32, i8 }

define dso_local ventus_kernel void @kernel_helper_clone(ptr addrspace(1) %out) {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_helper_clone(
; CHECK: %obj = alloca %struct.S, align 4, addrspace(5)
; CHECK: call void @helper.ventus.as5_p0(ptr addrspace(5) %obj, ptr addrspace(1) %out)
; CHECK-NOT: call void @helper(ptr {{.*}}%obj
entry:
  %obj = alloca %struct.S, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @helper(ptr %flat, ptr addrspace(1) %out)
  ret void
}

define dso_local void @helper(ptr %p, ptr addrspace(1) %out) {
; CHECK-LABEL: define dso_local void @helper(
; CHECK: store i32 7, ptr %p, align 4
entry:
  store i32 7, ptr %p, align 4
  %v = load i32, ptr %p, align 4
  store i32 %v, ptr addrspace(1) %out, align 4
  ret void
}

define dso_local ventus_kernel void @kernel_local_memset_store() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_local_memset_store(
; CHECK: alloca
; CHECK-SAME: addrspace(5)
; CHECK: call void @llvm.memset.p5.i32(ptr addrspace(5)
; CHECK-NOT: call void @llvm.memset.p0.i32(
entry:
  %arr = alloca [8 x i8], align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %arr to ptr
  call void @llvm.memset.p0.i32(ptr align 4 %flat, i8 0, i32 8, i1 false)
  %elt = getelementptr inbounds [8 x i8], ptr %flat, i32 0, i32 3
  store i8 9, ptr %elt, align 1
  ret void
}

define dso_local ventus_kernel void @kernel_two_private_args() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_two_private_args(
; CHECK: call void @two_private_helper.ventus.as5_p0.ventus.as5_p1(ptr addrspace(5) %a, ptr addrspace(5) %b)
; CHECK-NOT: call void @two_private_helper.ventus.as5_p0.ventus.as5_p1(ptr addrspace(5) %a, ptr addrspace(5) %b)
; CHECK: ret void
entry:
  %a = alloca i32, align 4, addrspace(5)
  %b = alloca i32, align 4, addrspace(5)
  %a.flat = addrspacecast ptr addrspace(5) %a to ptr
  %b.flat = addrspacecast ptr addrspace(5) %b to ptr
  call void @two_private_helper(ptr %a.flat, ptr %b.flat)
  ret void
}

define dso_local void @two_private_helper(ptr %a, ptr %b) {
; CHECK-LABEL: define dso_local void @two_private_helper(
entry:
  store i32 1, ptr %a, align 4
  store i32 2, ptr %b, align 4
  ret void
}

define dso_local ventus_kernel void @kernel_mixed_private_arg() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_mixed_private_arg(
; CHECK: call void @mixed_private_helper.ventus.as5_p0(ptr addrspace(5) %obj, ptr addrspace(5) %scratch)
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %scratch = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @mixed_private_helper(ptr %flat, ptr addrspace(5) %scratch)
  ret void
}

define dso_local void @mixed_private_helper(ptr %p,
                                            ptr addrspace(5) %scratch) {
; CHECK-LABEL: define dso_local void @mixed_private_helper(
entry:
  store i32 3, ptr %p, align 4
  store i32 4, ptr addrspace(5) %scratch, align 4
  ret void
}

define dso_local ventus_kernel void @kernel_available_externally_clone() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_available_externally_clone(
; CHECK: call void @available_helper.ventus.as5_p0(ptr addrspace(5) %obj)
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @available_helper(ptr %flat)
  ret void
}

define available_externally dso_local void @available_helper(ptr %p) {
; CHECK-LABEL: define available_externally dso_local void @available_helper(
entry:
  store i32 5, ptr %p, align 4
  ret void
}

define dso_local ventus_kernel void @kernel_cast_back_resolved(ptr addrspace(1) %out) {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_cast_back_resolved(
; CHECK: %a = alloca i32, align 4, addrspace(5)
; CHECK-NOT: addrspacecast ptr addrspace(5) %a to ptr
; CHECK: store volatile i32 1, ptr addrspace(5) %a, align 4
; CHECK: [[V:%.*]] = load volatile i32, ptr addrspace(5) %a, align 4
; CHECK: store i32 [[V]], ptr addrspace(1) %out, align 4
; CHECK-NOT: addrspacecast ptr addrspace(5) %a to ptr
; CHECK: ret void
entry:
  %a = alloca i32, align 4, addrspace(5)
  %af = addrspacecast ptr addrspace(5) %a to ptr
  %p5 = addrspacecast ptr %af to ptr addrspace(5)
  store volatile i32 1, ptr addrspace(5) %p5, align 4
  %v = load volatile i32, ptr addrspace(5) %p5, align 4
  store i32 %v, ptr addrspace(1) %out, align 4
  ret void
}

define dso_local ventus_kernel void @kernel_loop_phi_gep(i32 %n) {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_loop_phi_gep(
; CHECK: %a = alloca [8 x i32], align 4, addrspace(5)
; CHECK: %p.as5 = phi ptr addrspace(5) [ %a, %entry ], [ %next.as5, %loop ]
; CHECK: store volatile i32 %i, ptr addrspace(5) %p.as5, align 4
; CHECK: %next.as5 = getelementptr i32, ptr addrspace(5) %p.as5, i32 1
; CHECK-NOT: phi ptr [ %flat
entry:
  %a = alloca [8 x i32], align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %a to ptr
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %inc, %loop ]
  %p = phi ptr [ %flat, %entry ], [ %next, %loop ]
  store volatile i32 %i, ptr %p, align 4
  %next = getelementptr i32, ptr %p, i32 1
  %inc = add i32 %i, 1
  %cond = icmp ult i32 %inc, %n
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

define dso_local ventus_kernel void @kernel_pointer_compare(ptr addrspace(1) %out) {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_pointer_compare(
; CHECK: %a = alloca [4 x i32], align 4, addrspace(5)
; CHECK: %end.as5 = getelementptr i32, ptr addrspace(5) %a, i32 4
; CHECK: %done = icmp eq ptr addrspace(5) %a, %end.as5
; CHECK: store i1 %done, ptr addrspace(1) %out, align 1
entry:
  %a = alloca [4 x i32], align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %a to ptr
  %end = getelementptr i32, ptr %flat, i32 4
  %done = icmp eq ptr %flat, %end
  store i1 %done, ptr addrspace(1) %out, align 1
  ret void
}

define dso_local ventus_kernel void @kernel_lifetime_only() {
; CHECK-LABEL: define dso_local ventus_kernel void @kernel_lifetime_only(
; CHECK-NOT: addrspacecast
; CHECK-NOT: llvm.lifetime.start
; CHECK: ret void
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @llvm.lifetime.start.p0(i64 4, ptr %flat)
  ret void
}

define dso_local i32 @vararg_va_list(ptr addrspace(2) %fmt, ...) {
; CHECK-LABEL: define dso_local i32 @vararg_va_list(
; CHECK: %va = alloca ptr, align 4, addrspace(5)
; CHECK: %va.start = addrspacecast ptr addrspace(5) %va to ptr
; CHECK: call void @llvm.va_start(ptr %va.start)
; CHECK: %va.end = addrspacecast ptr addrspace(5) %va to ptr
; CHECK: call void @llvm.va_end(ptr %va.end)
entry:
  %va = alloca ptr, align 4, addrspace(5)
  %va.start = addrspacecast ptr addrspace(5) %va to ptr
  call void @llvm.va_start(ptr %va.start)
  %va.end = addrspacecast ptr addrspace(5) %va to ptr
  call void @llvm.va_end(ptr %va.end)
  ret i32 0
}

; CHECK-LABEL: define dso_local void @helper.ventus.as5_p0(
; CHECK-SAME: ptr addrspace(5) %p
; CHECK: store i32 7, ptr addrspace(5) %p, align 4
; CHECK: load i32, ptr addrspace(5) %p, align 4

; CHECK-LABEL: define dso_local void @two_private_helper.ventus.as5_p0.ventus.as5_p1(
; CHECK-SAME: ptr addrspace(5) %a
; CHECK-SAME: ptr addrspace(5) %b
; CHECK: store i32 1, ptr addrspace(5) %a, align 4
; CHECK: store i32 2, ptr addrspace(5) %b, align 4

; CHECK-LABEL: define dso_local void @mixed_private_helper.ventus.as5_p0(
; CHECK-SAME: ptr addrspace(5) %p
; CHECK-SAME: ptr addrspace(5) %scratch
; CHECK: store i32 3, ptr addrspace(5) %p, align 4
; CHECK: store i32 4, ptr addrspace(5) %scratch, align 4

; CHECK-LABEL: define internal void @available_helper.ventus.as5_p0(
; CHECK-SAME: ptr addrspace(5) %p
; CHECK: store i32 5, ptr addrspace(5) %p, align 4

declare void @llvm.memset.p0.i32(ptr nocapture writeonly, i8, i32, i1 immarg)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture)
declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)

; ASM-LABEL: two_private_helper:
; ASM: vsw12.v
; ASM: vsw12.v
; ASM-LABEL: two_private_helper.ventus.as5_p0.ventus.as5_p1:
; ASM: vsw.v
; ASM: vsw.v
; ASM-NOT: vsw12.v
