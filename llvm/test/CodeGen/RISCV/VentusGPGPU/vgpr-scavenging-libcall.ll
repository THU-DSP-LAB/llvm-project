; RUN: llc -O0 -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32"

declare void @escape_private(ptr addrspace(5))

; This stresses PEI frame-index scavenging: scalarizing the wide conversion
; creates repeated libcalls while many VGPR values are live in stack slots.
define dso_local <16 x double> @convert_i64x16_to_f64x16(<16 x i64> %x) #0 {
; CHECK-LABEL: convert_i64x16_to_f64x16:
; CHECK:       call __floatdidf@plt
; CHECK:       vsw.v
; CHECK:       vlw.v
entry:
  %r = sitofp <16 x i64> %x to <16 x double>
  ret <16 x double> %r
}

; The dedicated VGPR scavenging slots occupy the first two private-stack words.
; The aligned object remains naturally aligned after those reserved words.
define dso_local i64 @aligned_private_i64(i64 %x) #0 {
; CHECK-LABEL: aligned_private_i64:
; CHECK:       addi tp, tp, 16
; CHECK:       vsw.v v1, -12(v32)
; CHECK:       vsw.v v0, -16(v32)
; CHECK:       vlw.v v0, -16(v32)
; CHECK:       vlw.v v1, -12(v32)
entry:
  %slot = alloca i64, align 8, addrspace(5)
  store i64 %x, ptr addrspace(5) %slot, align 8
  %v = load i64, ptr addrspace(5) %slot, align 8
  ret i64 %v
}

; Large private frames need address legalization for normal frame objects. The
; VGPR scavenging slot itself must still be reachable directly; otherwise saving
; the scavenged VGPR creates another VGPR scratch and PEI cannot finish.
define dso_local <16 x double> @convert_i64x16_to_f64x16_large_private_frame(<16 x i64> %x) #0 {
; CHECK-LABEL: convert_i64x16_to_f64x16_large_private_frame:
; CHECK:       addi tp, tp, 1284
; CHECK:       vsw.v {{v[0-9]+}}, -4(v32){{.*}}Folded Spill
; CHECK:       vlw.v {{v[0-9]+}}, -4(v32){{.*}}Folded Reload
; CHECK:       call __floatdidf@plt
entry:
  %buf = alloca [257 x i32], align 4, addrspace(5)
  call void @escape_private(ptr addrspace(5) %buf)
  %r = sitofp <16 x i64> %x to <16 x double>
  ret <16 x double> %r
}

attributes #0 = { "target-cpu"="ventus-gpgpu" }
