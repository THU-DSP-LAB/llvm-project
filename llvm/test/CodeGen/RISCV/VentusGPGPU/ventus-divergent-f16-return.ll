; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -mattr=+zfh < %s | FileCheck %s

; CHECK-LABEL: divergent_f16:
; CHECK: vlh.v v0, 0(v0)
; CHECK-NOT: fmv.h.x
define half @divergent_f16(ptr addrspace(5) %p) {
entry:
  %v = load half, ptr addrspace(5) %p, align 2
  ret half %v
}
