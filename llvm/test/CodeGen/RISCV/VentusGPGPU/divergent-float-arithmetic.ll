; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -mattr=+m,+a,+f,+d,+v,+zfh -target-abi=ilp32 -verify-machineinstrs < %s -o - | FileCheck %s
; Per-lane loads use VV when both inputs diverge; a uniform input may use
; VF/FV or a broadcast followed by VV. The E02 two-texture retained IR exposed mixed fallback
; patterns winning before strict operand predicates. Encodings do not change.

define spir_kernel void @lane_add(ptr addrspace(5) %in, ptr addrspace(5) %out) {
; CHECK-LABEL: lane_add:
; CHECK: vfadd.vv
  %second = getelementptr float, ptr addrspace(5) %in, i32 1
  %x = load float, ptr addrspace(5) %in, align 4
  %y = load float, ptr addrspace(5) %second, align 4
  %z = fadd float %x, %y
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @lane_sub(ptr addrspace(5) %in, ptr addrspace(5) %out) {
; CHECK-LABEL: lane_sub:
; CHECK: vfsub.vv
  %second = getelementptr float, ptr addrspace(5) %in, i32 1
  %x = load float, ptr addrspace(5) %in, align 4
  %y = load float, ptr addrspace(5) %second, align 4
  %z = fsub float %x, %y
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @lane_mul(ptr addrspace(5) %in, ptr addrspace(5) %out) {
; CHECK-LABEL: lane_mul:
; CHECK: vfmul.vv
  %second = getelementptr float, ptr addrspace(5) %in, i32 1
  %x = load float, ptr addrspace(5) %in, align 4
  %y = load float, ptr addrspace(5) %second, align 4
  %z = fmul float %x, %y
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @lane_div(ptr addrspace(5) %in, ptr addrspace(5) %out) {
; CHECK-LABEL: lane_div:
; CHECK: vfdiv.vv
  %second = getelementptr float, ptr addrspace(5) %in, i32 1
  %x = load float, ptr addrspace(5) %in, align 4
  %y = load float, ptr addrspace(5) %second, align 4
  %z = fdiv float %x, %y
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @mixed_add(ptr addrspace(5) %in, ptr addrspace(5) %out, float %scale) {
; CHECK-LABEL: mixed_add:
; CHECK: vfadd.{{v[vf]}}
  %x = load float, ptr addrspace(5) %in, align 4
  %z = fadd float %x, %scale
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @mixed_mul(ptr addrspace(5) %in, ptr addrspace(5) %out, float %scale) {
; CHECK-LABEL: mixed_mul:
; CHECK: vfmul.{{v[vf]}}
  %x = load float, ptr addrspace(5) %in, align 4
  %z = fmul float %scale, %x
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @mixed_sub(ptr addrspace(5) %in, ptr addrspace(5) %out, float %scale) {
; CHECK-LABEL: mixed_sub:
; CHECK: vf{{rsub.vf|sub.vv}}
  %x = load float, ptr addrspace(5) %in, align 4
  %z = fsub float %scale, %x
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}

define spir_kernel void @mixed_div(ptr addrspace(5) %in, ptr addrspace(5) %out, float %scale) {
; CHECK-LABEL: mixed_div:
; CHECK: vf{{rdiv.vf|div.vv}}
  %x = load float, ptr addrspace(5) %in, align 4
  %z = fdiv float %scale, %x
  store float %z, ptr addrspace(5) %out, align 4
  ret void
}
