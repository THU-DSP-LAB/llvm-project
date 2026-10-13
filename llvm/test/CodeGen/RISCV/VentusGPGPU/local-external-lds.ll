; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs \
; RUN:   -filetype=obj < %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs \
; RUN:   -asm-verbose < %s | FileCheck %s --check-prefix=ASM

@ext_lds = external addrspace(3) global i32, align 64

define ptr addrspace(3) @get_ext_lds() {
entry:
  ret ptr addrspace(3) @ext_lds
}

; CHECK:      Section ({{.*}}) .rela.text {
; CHECK-NEXT:   0x0 R_RISCV_VENTUS_LDS_HI20 ext_lds 0x0
; CHECK-NEXT:   0xC R_RISCV_VENTUS_LDS_LO12_I ext_lds 0x0

; ASM:      .word ext_lds
; ASM-NEXT: .word 1
; ASM-NEXT: .quad 4
; ASM-NEXT: .quad 64
