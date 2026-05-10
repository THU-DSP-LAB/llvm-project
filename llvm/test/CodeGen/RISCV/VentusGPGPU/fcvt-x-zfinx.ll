; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s

; CHECK-LABEL: name: convert_char_rtef
; CHECK: VFCVT_RTZ_X_F_V
define dso_local signext i8 @convert_char_rtef(float noundef %x) local_unnamed_addr {
entry:
  %conv = call i8 @llvm.fptosi.sat.i8.f32(float %x)
  ret i8 %conv
}

; CHECK-LABEL: name: convert_uchar_rtef
; CHECK: VFCVT_RTZ_XU_F_V
define dso_local zeroext i8 @convert_uchar_rtef(float noundef %x) local_unnamed_addr {
entry:
  %conv = call i8 @llvm.fptoui.sat.i8.f32(float %x)
  ret i8 %conv
}

declare i8 @llvm.fptosi.sat.i8.f32(float)
declare i8 @llvm.fptoui.sat.i8.f32(float)
