; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -mattr=+m,+a,+f,+d,+v,+zfh -target-abi=ilp32 -verify-machineinstrs < %s -o - | FileCheck %s
; Uniform FP loads followed by arithmetic and a uniform select must remain in
; SGPRs. Mixed/vector fallback patterns previously outranked scalar arithmetic
; and produced a forbidden VGPR-to-GPRF32 COPY. No encodings change here.

define spir_kernel void @uniform_add(ptr addrspace(1) %in, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_add:
; CHECK: fadd.s
  %rhs_address = getelementptr float, ptr addrspace(1) %in, i32 1
  %lhs = load float, ptr addrspace(1) %in, align 4
  %rhs = load float, ptr addrspace(1) %rhs_address, align 4
  %result = fadd float %lhs, %rhs
  %cmp = fcmp olt float %result, 0.000000e+00
  %value = select i1 %cmp, float 0.000000e+00, float %result
  store float %value, ptr addrspace(1) %out, align 4
  ret void
}

define spir_kernel void @uniform_sub(ptr addrspace(1) %in, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_sub:
; CHECK: fsub.s
  %rhs_address = getelementptr float, ptr addrspace(1) %in, i32 1
  %lhs = load float, ptr addrspace(1) %in, align 4
  %rhs = load float, ptr addrspace(1) %rhs_address, align 4
  %result = fsub float %lhs, %rhs
  %cmp = fcmp olt float %result, 0.000000e+00
  %value = select i1 %cmp, float 0.000000e+00, float %result
  store float %value, ptr addrspace(1) %out, align 4
  ret void
}

define spir_kernel void @uniform_mul(ptr addrspace(1) %in, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_mul:
; CHECK: fmul.s
  %rhs_address = getelementptr float, ptr addrspace(1) %in, i32 1
  %lhs = load float, ptr addrspace(1) %in, align 4
  %rhs = load float, ptr addrspace(1) %rhs_address, align 4
  %result = fmul float %lhs, %rhs
  %cmp = fcmp olt float %result, 0.000000e+00
  %value = select i1 %cmp, float 0.000000e+00, float %result
  store float %value, ptr addrspace(1) %out, align 4
  ret void
}

define spir_kernel void @uniform_div(ptr addrspace(1) %in, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_div:
; CHECK: fdiv.s
  %rhs_address = getelementptr float, ptr addrspace(1) %in, i32 1
  %lhs = load float, ptr addrspace(1) %in, align 4
  %rhs = load float, ptr addrspace(1) %rhs_address, align 4
  %result = fdiv float %lhs, %rhs
  %cmp = fcmp olt float %result, 0.000000e+00
  %value = select i1 %cmp, float 0.000000e+00, float %result
  store float %value, ptr addrspace(1) %out, align 4
  ret void
}

define spir_kernel void @uniform_add_zero(ptr addrspace(1) %in, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_add_zero:
; CHECK: fadd.s
  %rhs = load float, ptr addrspace(1) %in, align 4
  %result = fadd float 0.000000e+00, %rhs
  %cmp = fcmp olt float %result, 0.000000e+00
  %value = select i1 %cmp, float 0.000000e+00, float %result
  store float %value, ptr addrspace(1) %out, align 4
  ret void
}
