; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s

@shared_tmp = internal addrspace(3) global [4 x i32] undef, align 4

define dso_local void @f1() {
; VENTUS-LABEL: f1:
; VENTUS:       csrr t0, CSR_LDS
; VENTUS-NEXT:  li t1, 1
; VENTUS-NEXT:  sw t1, 0(t0)
; VENTUS-NEXT:  ret
entry:
  store i32 1, ptr addrspace(3) @shared_tmp, align 4
  ret void
}

define dso_local void @f2() {
; VENTUS-LABEL: f2:
; VENTUS:       csrr t0, CSR_LDS
; VENTUS-NEXT:  li t1, 2
; VENTUS-NEXT:  sw t1, 0(t0)
; VENTUS-NEXT:  ret
entry:
  store i32 2, ptr addrspace(3) @shared_tmp, align 4
  ret void
}

; VENTUS: .type	shared_tmp,@object
; VENTUS: .comm	shared_tmp,16,4
