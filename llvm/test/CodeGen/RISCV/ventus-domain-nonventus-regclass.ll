; RUN: llc -mtriple=riscv32 -mattr=+f -target-abi=ilp32f \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s

; CHECK-LABEL: name: plain_float_value
; CHECK: %0:fpr32 = COPY
; CHECK-NOT: vgpr
; CHECK-NOT: gprf32
define float @plain_float_value(float %a) {
entry:
  ret float %a
}
