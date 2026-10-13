; RUN: llc -mtriple=riscv32 -mattr=+zve32x,+zvl32b < %s | FileCheck %s

; CHECK-LABEL: plain_i32_value:
; CHECK-NOT: Ventus register domain verifier failed
; CHECK-NOT: Impossible RISCV physreg copy
; CHECK: ret
define i32 @plain_i32_value(i32 %a) {
entry:
  ret i32 %a
}
