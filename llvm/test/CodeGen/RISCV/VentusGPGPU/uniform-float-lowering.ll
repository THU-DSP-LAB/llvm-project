; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s
; Uniform kernel arguments must stay scalar; lane-varying arguments must not.
; Check opcodes rather than incidental register allocation or load ordering.

@scalar = external global float

define ventus_kernel void @uniform_add(float %a, float %b, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_add:
; CHECK-NOT: vfadd
; CHECK: fadd.s
; CHECK-NOT: vfadd
; CHECK: ret
  %v = fadd float %a, %b
  store float %v, ptr addrspace(1) %out
  ret void
}

define ventus_kernel void @uniform_sub(float %a, float %b, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_sub:
; CHECK-NOT: vfsub
; CHECK: fsub.s
; CHECK-NOT: vfsub
; CHECK: ret
  %v = fsub float %a, %b
  store float %v, ptr addrspace(1) %out
  ret void
}

define ventus_kernel void @uniform_mul(float %a, float %b, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_mul:
; CHECK-NOT: vfmul
; CHECK: fmul.s
; CHECK-NOT: vfmul
; CHECK: ret
  %v = fmul float %a, %b
  store float %v, ptr addrspace(1) %out
  ret void
}

define ventus_kernel void @uniform_div(float %a, float %b, ptr addrspace(1) %out) {
; CHECK-LABEL: uniform_div:
; CHECK-NOT: vfdiv
; CHECK: fdiv.s
; CHECK-NOT: vfdiv
; CHECK: ret
  %v = fdiv float %a, %b
  store float %v, ptr addrspace(1) %out
  ret void
}

define float @mixed_add(float %a) {
; CHECK-LABEL: mixed_add:
; CHECK-NOT: vmv.v.x
; CHECK: vfadd.vf
; CHECK-NOT: vfadd.vv
; CHECK: ret
  %v = fadd float %a, 2.000000e+00
  ret float %v
}

define float @mixed_sub(float %a) {
; CHECK-LABEL: mixed_sub:
; CHECK-NOT: vmv.v.x
; CHECK: vfsub.vf
; CHECK: ret
  %b = load float, ptr @scalar
  %v = fsub float %a, %b
  ret float %v
}

define float @mixed_reverse_sub(float %a) {
; CHECK-LABEL: mixed_reverse_sub:
; CHECK-NOT: vmv.v.x
; CHECK: vfrsub.vf
; CHECK: ret
  %v = fsub float 2.000000e+00, %a
  ret float %v
}

define float @mixed_div(float %a) {
; CHECK-LABEL: mixed_div:
; CHECK-NOT: vmv.v.x
; CHECK: vfdiv.vf
; CHECK: ret
  %v = fdiv float %a, 3.000000e+00
  ret float %v
}

define float @mixed_reverse_div(float %a) {
; CHECK-LABEL: mixed_reverse_div:
; CHECK-NOT: vmv.v.x
; CHECK: vfrdiv.vf
; CHECK: ret
  %v = fdiv float 3.000000e+00, %a
  ret float %v
}

define float @divergent_add(float %a, float %b) {
; CHECK-LABEL: divergent_add:
; CHECK-NOT: fadd.s
; CHECK: vfadd.vv
; CHECK: ret
  %v = fadd float %a, %b
  ret float %v
}
