; RUN: split-file %s %t
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/external.ll 2>&1 | FileCheck %s --check-prefix=EXTERNAL
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/indirect.ll 2>&1 | FileCheck %s --check-prefix=INDIRECT
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/variadic.ll 2>&1 | FileCheck %s --check-prefix=VARIADIC
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/return.ll 2>&1 | FileCheck %s --check-prefix=RETURN
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/store-ptr.ll 2>&1 | FileCheck %s --check-prefix=STOREPTR
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/store-self.ll 2>&1 | FileCheck %s --check-prefix=STORESELF
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/ptrtoint.ll 2>&1 | FileCheck %s --check-prefix=PTRTOINT
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/mixed-phi.ll 2>&1 | FileCheck %s --check-prefix=MIXEDPHI
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/mixed-select.ll 2>&1 | FileCheck %s --check-prefix=MIXEDSELECT
; RUN: not llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-generic-as-specialization < %t/byval.ll 2>&1 | FileCheck %s --check-prefix=BYVAL

; EXTERNAL: private-derived generic pointer passed to external call
; INDIRECT: private-derived generic pointer passed to indirect call
; VARIADIC: private-derived generic pointer passed to variadic call
; RETURN: private-derived generic pointer returned
; STOREPTR: private-derived generic pointer stored to memory
; STORESELF: private-derived generic pointer stored to memory
; PTRTOINT: private-derived generic pointer converted through integer
; MIXEDPHI: mixed-AS phi for private-derived generic pointer
; MIXEDSELECT: mixed-AS select for private-derived generic pointer
; BYVAL: ABI-sensitive pointer attribute on private-derived call argument

;--- external.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_external_escape() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @external(ptr %flat)
  ret void
}
declare void @external(ptr)

;--- indirect.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_indirect(ptr %fn) {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void %fn(ptr %flat)
  ret void
}

;--- variadic.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_variadic() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void (ptr, ...) @variadic(ptr %flat)
  ret void
}
define dso_local void @variadic(ptr %p, ...) {
entry:
  ret void
}

;--- return.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ptr @return_private() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  ret ptr %flat
}

;--- store-ptr.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_store_ptr(ptr addrspace(1) %out) {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  store ptr %flat, ptr addrspace(1) %out, align 4
  ret void
}

;--- store-self.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_store_self() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  store ptr %flat, ptr %flat, align 4
  ret void
}

;--- ptrtoint.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local i32 @private_ptrtoint() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  %i = ptrtoint ptr %flat to i32
  ret i32 %i
}

;--- mixed-phi.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_mixed_phi(i1 %cond, ptr %flat.other) {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  br i1 %cond, label %a, label %b
a:
  br label %join
b:
  br label %join
join:
  %p = phi ptr [ %flat, %a ], [ %flat.other, %b ]
  store i32 1, ptr %p, align 4
  ret void
}

;--- mixed-select.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_mixed_select(i1 %cond, ptr %flat.other) {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  %p = select i1 %cond, ptr %flat, ptr %flat.other
  store i32 1, ptr %p, align 4
  ret void
}

;--- byval.ll
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32-unknown-unknown"
define dso_local ventus_kernel void @kernel_byval() {
entry:
  %obj = alloca i32, align 4, addrspace(5)
  %flat = addrspacecast ptr addrspace(5) %obj to ptr
  call void @byval_helper(ptr byval(i32) %flat)
  ret void
}
define dso_local void @byval_helper(ptr byval(i32) %p) {
entry:
  ret void
}
