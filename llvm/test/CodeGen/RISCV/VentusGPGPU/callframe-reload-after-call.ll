; RUN: llc -O0 -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32"

declare dso_local double @get_el(<16 x double>, i64) #0

define dso_local <2 x double> @wrap2(<16 x double> %x, <2 x i64> %mask) {
; CHECK-LABEL: wrap2:
; CHECK:       call get_el
; CHECK-NEXT:  addi tp, tp, -16
; CHECK-NEXT:  regext zero, zero, 1
; CHECK-NEXT:  vmv.v.x v32, tp
; CHECK:       call get_el
; CHECK-NEXT:  addi tp, tp, -16
; CHECK-NEXT:  regext zero, zero, 1
; CHECK-NEXT:  vmv.v.x v32, tp
entry:
  %m0 = extractelement <2 x i64> %mask, i64 0
  %e0 = call double @get_el(<16 x double> %x, i64 %m0)
  %r0 = insertelement <2 x double> undef, double %e0, i64 0
  %m1 = extractelement <2 x i64> %mask, i64 1
  %e1 = call double @get_el(<16 x double> %x, i64 %m1)
  %r1 = insertelement <2 x double> %r0, double %e1, i64 1
  ret <2 x double> %r1
}

attributes #0 = { noinline }
