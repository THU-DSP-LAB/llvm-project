; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare float @llvm.riscv.ventus.vcvt.fp32.fp16(i32)
declare i32 @llvm.riscv.ventus.vcvt.fp16.fp32(float)
declare float @llvm.riscv.ventus.vcvt.fp32.bf16(i32)
declare i32 @llvm.riscv.ventus.vcvt.bf16.fp32(float)
declare i32 @llvm.riscv.ventus.vadd.f16x2(i32, i32)
declare i32 @llvm.riscv.ventus.vmul.f16x2(i32, i32)
declare i32 @llvm.riscv.ventus.vfma.f16x2(i32, i32, i32)
declare i32 @llvm.riscv.ventus.vadd.bf16x2(i32, i32)
declare i32 @llvm.riscv.ventus.vmul.bf16x2(i32, i32)
declare i32 @llvm.riscv.ventus.vfma.bf16x2(i32, i32, i32)
declare float @llvm.riscv.ventus.vex2.approx.f32(float)
declare float @llvm.riscv.ventus.vlg2.approx.f32(float)
declare float @llvm.riscv.ventus.vrcp.approx.f32(float)
declare float @llvm.riscv.ventus.vsqrt.approx.f32(float)
declare float @llvm.riscv.ventus.vrsqrt.approx.f32(float)
declare float @llvm.riscv.ventus.vsin.approx.f32(float)
declare float @llvm.riscv.ventus.vcos.approx.f32(float)
declare float @llvm.riscv.ventus.vtanh.approx.f32(float)
declare float @llvm.riscv.ventus.vgelu.approx.f32(float)
declare float @llvm.riscv.ventus.vsilu.approx.f32(float)
declare i32 @llvm.riscv.ventus.vex2.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vrcp.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vsqrt.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vrsqrt.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vtanh.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vgelu.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vsilu.approx.f16x2(i32)
declare i32 @llvm.riscv.ventus.vex2.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vrcp.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vsqrt.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vrsqrt.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vtanh.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vgelu.approx.bf16x2(i32)
declare i32 @llvm.riscv.ventus.vsilu.approx.bf16x2(i32)

define float @test_vcvt_fp32_fp16(i32 %a0) {
; CHECK-LABEL: test_vcvt_fp32_fp16:
; CHECK: vcvt.fp32.fp16
  %r = call float @llvm.riscv.ventus.vcvt.fp32.fp16(i32 %a0)
  ret float %r
}

define i32 @test_vcvt_fp16_fp32(float %a0) {
; CHECK-LABEL: test_vcvt_fp16_fp32:
; CHECK: vcvt.fp16.fp32
  %r = call i32 @llvm.riscv.ventus.vcvt.fp16.fp32(float %a0)
  ret i32 %r
}

define float @test_vcvt_fp32_bf16(i32 %a0) {
; CHECK-LABEL: test_vcvt_fp32_bf16:
; CHECK: vcvt.fp32.bf16
  %r = call float @llvm.riscv.ventus.vcvt.fp32.bf16(i32 %a0)
  ret float %r
}

define i32 @test_vcvt_bf16_fp32(float %a0) {
; CHECK-LABEL: test_vcvt_bf16_fp32:
; CHECK: vcvt.bf16.fp32
  %r = call i32 @llvm.riscv.ventus.vcvt.bf16.fp32(float %a0)
  ret i32 %r
}

define i32 @test_vadd_f16x2(i32 %a0, i32 %a1) {
; CHECK-LABEL: test_vadd_f16x2:
; CHECK: vadd.f16x2
  %r = call i32 @llvm.riscv.ventus.vadd.f16x2(i32 %a0, i32 %a1)
  ret i32 %r
}

define i32 @test_vmul_f16x2(i32 %a0, i32 %a1) {
; CHECK-LABEL: test_vmul_f16x2:
; CHECK: vmul.f16x2
  %r = call i32 @llvm.riscv.ventus.vmul.f16x2(i32 %a0, i32 %a1)
  ret i32 %r
}

define i32 @test_vfma_f16x2(i32 %a0, i32 %a1, i32 %a2) {
; CHECK-LABEL: test_vfma_f16x2:
; CHECK: vfma.f16x2
  %r = call i32 @llvm.riscv.ventus.vfma.f16x2(i32 %a0, i32 %a1, i32 %a2)
  ret i32 %r
}

define i32 @test_vadd_bf16x2(i32 %a0, i32 %a1) {
; CHECK-LABEL: test_vadd_bf16x2:
; CHECK: vadd.bf16x2
  %r = call i32 @llvm.riscv.ventus.vadd.bf16x2(i32 %a0, i32 %a1)
  ret i32 %r
}

define i32 @test_vmul_bf16x2(i32 %a0, i32 %a1) {
; CHECK-LABEL: test_vmul_bf16x2:
; CHECK: vmul.bf16x2
  %r = call i32 @llvm.riscv.ventus.vmul.bf16x2(i32 %a0, i32 %a1)
  ret i32 %r
}

define i32 @test_vfma_bf16x2(i32 %a0, i32 %a1, i32 %a2) {
; CHECK-LABEL: test_vfma_bf16x2:
; CHECK: vfma.bf16x2
  %r = call i32 @llvm.riscv.ventus.vfma.bf16x2(i32 %a0, i32 %a1, i32 %a2)
  ret i32 %r
}

define float @test_vex2_approx_f32(float %x) {
; CHECK-LABEL: test_vex2_approx_f32:
; CHECK: vex2.approx.f32
  %r = call float @llvm.riscv.ventus.vex2.approx.f32(float %x)
  ret float %r
}

define float @test_vlg2_approx_f32(float %x) {
; CHECK-LABEL: test_vlg2_approx_f32:
; CHECK: vlg2.approx.f32
  %r = call float @llvm.riscv.ventus.vlg2.approx.f32(float %x)
  ret float %r
}

define float @test_vrcp_approx_f32(float %x) {
; CHECK-LABEL: test_vrcp_approx_f32:
; CHECK: vrcp.approx.f32
  %r = call float @llvm.riscv.ventus.vrcp.approx.f32(float %x)
  ret float %r
}

define float @test_vsqrt_approx_f32(float %x) {
; CHECK-LABEL: test_vsqrt_approx_f32:
; CHECK: vsqrt.approx.f32
  %r = call float @llvm.riscv.ventus.vsqrt.approx.f32(float %x)
  ret float %r
}

define float @test_vrsqrt_approx_f32(float %x) {
; CHECK-LABEL: test_vrsqrt_approx_f32:
; CHECK: vrsqrt.approx.f32
  %r = call float @llvm.riscv.ventus.vrsqrt.approx.f32(float %x)
  ret float %r
}

define float @test_vsin_approx_f32(float %x) {
; CHECK-LABEL: test_vsin_approx_f32:
; CHECK: vsin.approx.f32
  %r = call float @llvm.riscv.ventus.vsin.approx.f32(float %x)
  ret float %r
}

define float @test_vcos_approx_f32(float %x) {
; CHECK-LABEL: test_vcos_approx_f32:
; CHECK: vcos.approx.f32
  %r = call float @llvm.riscv.ventus.vcos.approx.f32(float %x)
  ret float %r
}

define float @test_vtanh_approx_f32(float %x) {
; CHECK-LABEL: test_vtanh_approx_f32:
; CHECK: vtanh.approx.f32
  %r = call float @llvm.riscv.ventus.vtanh.approx.f32(float %x)
  ret float %r
}

define float @test_vgelu_approx_f32(float %x) {
; CHECK-LABEL: test_vgelu_approx_f32:
; CHECK: vgelu.approx.f32
  %r = call float @llvm.riscv.ventus.vgelu.approx.f32(float %x)
  ret float %r
}

define float @test_vsilu_approx_f32(float %x) {
; CHECK-LABEL: test_vsilu_approx_f32:
; CHECK: vsilu.approx.f32
  %r = call float @llvm.riscv.ventus.vsilu.approx.f32(float %x)
  ret float %r
}

define i32 @test_vex2_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vex2_approx_f16x2:
; CHECK: vex2.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vex2.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vrcp_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vrcp_approx_f16x2:
; CHECK: vrcp.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vrcp.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vsqrt_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vsqrt_approx_f16x2:
; CHECK: vsqrt.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vsqrt.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vrsqrt_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vrsqrt_approx_f16x2:
; CHECK: vrsqrt.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vrsqrt.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vtanh_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vtanh_approx_f16x2:
; CHECK: vtanh.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vtanh.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vgelu_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vgelu_approx_f16x2:
; CHECK: vgelu.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vgelu.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vsilu_approx_f16x2(i32 %x) {
; CHECK-LABEL: test_vsilu_approx_f16x2:
; CHECK: vsilu.approx.f16x2
  %r = call i32 @llvm.riscv.ventus.vsilu.approx.f16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vex2_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vex2_approx_bf16x2:
; CHECK: vex2.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vex2.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vrcp_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vrcp_approx_bf16x2:
; CHECK: vrcp.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vrcp.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vsqrt_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vsqrt_approx_bf16x2:
; CHECK: vsqrt.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vsqrt.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vrsqrt_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vrsqrt_approx_bf16x2:
; CHECK: vrsqrt.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vrsqrt.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vtanh_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vtanh_approx_bf16x2:
; CHECK: vtanh.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vtanh.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vgelu_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vgelu_approx_bf16x2:
; CHECK: vgelu.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vgelu.approx.bf16x2(i32 %x)
  ret i32 %r
}

define i32 @test_vsilu_approx_bf16x2(i32 %x) {
; CHECK-LABEL: test_vsilu_approx_bf16x2:
; CHECK: vsilu.approx.bf16x2
  %r = call i32 @llvm.riscv.ventus.vsilu.approx.bf16x2(i32 %x)
  ret i32 %r
}
