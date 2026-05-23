; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s

; The leading double16 argument consumes all V0-V31 argument registers on RV32.
; The following i64 argument is split to the stack and must keep distinct ABI
; offsets for its two i32 parts.

; CHECK-LABEL: name:            stack_i64_after_double16
; CHECK: fixedStack:
; CHECK-NEXT:  - { id: 0, type: default, offset: -4, size: 4
; CHECK:       - { id: 1, type: default, offset: 0, size: 4
; CHECK: %{{[0-9]+}}:vgpr = VLW %fixed-stack.1
; CHECK-NEXT: %{{[0-9]+}}:vgpr = VLW %fixed-stack.0
define dso_local i64 @stack_i64_after_double16(<16 x double> %x, i64 %idx) {
entry:
  ret i64 %idx
}
