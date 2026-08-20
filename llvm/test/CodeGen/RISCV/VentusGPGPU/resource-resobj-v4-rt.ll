; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

; CHECK:      .section .ventus.resobj,"",@progbits
; CHECK-NEXT: .p2align 3
; CHECK-NEXT: .word 1448235855
; CHECK-NEXT: .word 4
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 1
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 36
; CHECK-NEXT: .word 140
; CHECK-NEXT: .word 140
; CHECK-NEXT: .word rt_kernel
; CHECK-NEXT: .word 1
; CHECK-NEXT: .quad {{[0-9]+}}
; CHECK-NEXT: .quad {{[0-9]+}}
; CHECK-NEXT: .quad 0
; CHECK-NEXT: .quad 0
; CHECK-NEXT: .quad 0
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 3
; CHECK-NEXT: .word 5
; CHECK-NEXT: .word 5
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 384
; CHECK-NEXT: .word 384
; CHECK-NEXT: .word 128
; CHECK-NEXT: .word 96
; CHECK-NEXT: .word 0
; CHECK-NEXT: .word 0

define dso_local ventus_kernel void @rt_kernel() !ventus.rt.resource !0 {
entry:
  ret void
}

!0 = !{i32 3, i32 5, i32 5, i32 0, i32 384, i32 384, i32 128, i32 96, i32 0, i32 0}
