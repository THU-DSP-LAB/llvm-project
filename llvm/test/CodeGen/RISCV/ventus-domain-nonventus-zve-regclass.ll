; RUN: llc -mtriple=riscv32 -mattr=+zve32x,+zvl32b \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ZVE32X
; RUN: llc -mtriple=riscv32 -mattr=+f,+zve32f,+zvl32b -target-abi=ilp32f \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ZVE32F

; ZVE32X-LABEL: name: plain_i32_value
; ZVE32X: %0:gpr = COPY
; ZVE32X-NOT: vgpr

; ZVE32F-LABEL: name: plain_i32_value
; ZVE32F: %0:gpr = COPY
; ZVE32F-NOT: vgpr

; ZVE32F-LABEL: name: plain_f32_value
; ZVE32F: %0:fpr32 = COPY
; ZVE32F-NOT: vgpr
; ZVE32F-NOT: gprf32
define i32 @plain_i32_value(i32 %a) {
entry:
  ret i32 %a
}

define float @plain_f32_value(float %a) {
entry:
  ret float %a
}
