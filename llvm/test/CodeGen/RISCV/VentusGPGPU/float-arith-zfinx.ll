; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs -O1 < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s


define dso_local spir_kernel void @fadd(float noundef %c, float noundef %d, ptr addrspace(1) nocapture noundef writeonly align 4 %result)  {
entry:
	; VENTUS: fadd.s a{{[1-9]}}, a2, a1
  %add1 = fadd float %c, %d
  store float %add1, ptr addrspace(1) %result, align 4
  ret void
}

define dso_local spir_kernel void @fsub(float noundef %c, float noundef %d, ptr addrspace(1) nocapture noundef writeonly align 4 %result)  {
entry:
	; VENTUS: fsub.s a{{[1-9]}}, a2, a1
  %sub = fsub float %c, %d
  store float %sub, ptr addrspace(1) %result, align 4
  ret void
}

define dso_local spir_kernel void @fmul(float noundef %c, float noundef %d, ptr addrspace(1) nocapture noundef writeonly align 4 %result)  {
entry:
	; VENTUS: fmul.s a{{[1-9]}}, a2, a1
  %mul = fmul float %c, %d
  store float %mul, ptr addrspace(1) %result, align 4
  ret void
}

define dso_local spir_kernel void @fdiv(float noundef %c, float noundef %d, ptr addrspace(1) nocapture noundef writeonly align 4 %result)  {
entry:
	; VENTUS: fdiv.s a{{[1-9]}}, a2, a1
  %div = fdiv float %c, %d
  store float %div, ptr addrspace(1) %result, align 4
  ret void
}
