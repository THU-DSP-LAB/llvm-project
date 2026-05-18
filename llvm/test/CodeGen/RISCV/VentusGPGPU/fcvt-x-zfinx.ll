; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s

; CHECK-LABEL: name: convert_char_rtef
; CHECK: VFCVT_RTZ_X_F_V
define dso_local signext i8 @convert_char_rtef(float noundef %x) local_unnamed_addr {
entry:
  %conv = call i8 @llvm.fptosi.sat.i8.f32(float %x)
  ret i8 %conv
}

; CHECK-LABEL: name: convert_char_rint
; CHECK: CSRRS 2, $x0
; CHECK-NEXT: CSRRWI $x0, 2, 0
; CHECK-NEXT: nofpexcept VFCVT_X_F_V
; CHECK-NEXT: nofpexcept VFCVT_F_X_V
; CHECK-NEXT: CSRRW $x0, 2
; CHECK: VFCVT_RTZ_X_F_V
; CHECK-NOT: FCVT_W_S
define dso_local signext i8 @convert_char_rint(float noundef %x) local_unnamed_addr #0 {
entry:
  %rounded = call float @llvm.rint.f32(float %x) #2
  %conv = fptosi float %rounded to i8
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
declare float @llvm.rint.f32(float)

attributes #0 = { alwaysinline convergent norecurse nounwind vscale_range(1,2048) "target-cpu"="ventus-gpgpu" "target-features"="+32bit,+a,+m,+relax,+zdinx,+zfinx,+zhinx,+zve32f,+zve32x,+zvl32b,-64bit,-save-restore" }
attributes #2 = { convergent nobuiltin "no-builtins" }
