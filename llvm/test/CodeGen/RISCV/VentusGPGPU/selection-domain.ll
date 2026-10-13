; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu \
; RUN:   -stop-before=ventus-broadcast-copy-propagation < %s | FileCheck %s

define i32 @sat_s32(float %x) {
; CHECK-LABEL: name: sat_s32
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
; CHECK: VFCVT_RTZ_X_F_V
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
entry:
  %conv = call i32 @llvm.fptosi.sat.i32.f32(float %x)
  ret i32 %conv
}

define i32 @sat_u32(float %x) {
; CHECK-LABEL: name: sat_u32
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
; CHECK: VFCVT_RTZ_XU_F_V
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
entry:
  %conv = call i32 @llvm.fptoui.sat.i32.f32(float %x)
  ret i32 %conv
}

define float @add_scalar_rhs(float %x) {
; CHECK-LABEL: name: add_scalar_rhs
; CHECK: VFADD_VF
; CHECK-NOT: VFADD_VV
entry:
  %y = fadd float %x, 1.000000e+00
  ret float %y
}

define float @sub_scalar_lhs(float %x) {
; CHECK-LABEL: name: sub_scalar_lhs
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
; CHECK: VFRSUB_VF
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
entry:
  %y = fsub float 1.000000e+00, %x
  ret float %y
}

define float @div_scalar_lhs(float %x) {
; CHECK-LABEL: name: div_scalar_lhs
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
; CHECK: VFRDIV_VF
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
entry:
  %y = fdiv float 1.000000e+00, %x
  ret float %y
}

define i32 @fcmp_ole_scalar_rhs(float %x) {
; CHECK-LABEL: name: fcmp_ole_scalar_rhs
; CHECK: VMFLE_VF
; CHECK-NOT: VMFLE_VV
entry:
  %cmp = fcmp ole float %x, 1.000000e+00
  %z = zext i1 %cmp to i32
  ret i32 %z
}

define i32 @fcmp_olt_scalar_lhs(float %x) {
; CHECK-LABEL: name: fcmp_olt_scalar_lhs
; CHECK: VMFGT_VF
; CHECK-NOT: VMFLT_VV
entry:
  %cmp = fcmp olt float 1.000000e+00, %x
  %z = zext i1 %cmp to i32
  ret i32 %z
}

define i32 @fcmp_both_divergent(float %x, float %y) {
; CHECK-LABEL: name: fcmp_both_divergent
; CHECK: VMFLT_VV
entry:
  %cmp = fcmp olt float %x, %y
  %z = zext i1 %cmp to i32
  ret i32 %z
}

define float @copysign_uniform_lhs(float %x) {
; CHECK-LABEL: name: copysign_uniform_lhs
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
; CHECK: VFMV_V_F
; CHECK: VFSGNJ_VV
; CHECK-NOT: :{{gpr(f16|f32|f64)?}} = COPY {{%[0-9]+}}:vgpr
entry:
  %y = call float @llvm.copysign.f32(float 1.000000e+00, float %x)
  ret float %y
}

declare i32 @llvm.fptosi.sat.i32.f32(float)
declare i32 @llvm.fptoui.sat.i32.f32(float)
declare float @llvm.copysign.f32(float, float)
