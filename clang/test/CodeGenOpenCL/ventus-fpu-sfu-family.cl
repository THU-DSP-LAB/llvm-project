// RUN: %clang_cc1 -no-opaque-pointers -triple riscv32-unknown-unknown -target-cpu ventus-gpgpu -cl-std=CL2.0 -S -emit-llvm -o - %s | FileCheck %s

typedef unsigned int uint;

float test_vcvt_fp32_fp16(uint a0) {
  // CHECK-LABEL: @test_vcvt_fp32_fp16
  // CHECK: call float @llvm.riscv.ventus.vcvt.fp32.fp16(i32 %{{.*}})
  return __builtin_riscv_ventus_vcvt_fp32_fp16(a0);
}

uint test_vcvt_fp16_fp32(float a0) {
  // CHECK-LABEL: @test_vcvt_fp16_fp32
  // CHECK: call i32 @llvm.riscv.ventus.vcvt.fp16.fp32(float %{{.*}})
  return __builtin_riscv_ventus_vcvt_fp16_fp32(a0);
}

float test_vcvt_fp32_bf16(uint a0) {
  // CHECK-LABEL: @test_vcvt_fp32_bf16
  // CHECK: call float @llvm.riscv.ventus.vcvt.fp32.bf16(i32 %{{.*}})
  return __builtin_riscv_ventus_vcvt_fp32_bf16(a0);
}

uint test_vcvt_bf16_fp32(float a0) {
  // CHECK-LABEL: @test_vcvt_bf16_fp32
  // CHECK: call i32 @llvm.riscv.ventus.vcvt.bf16.fp32(float %{{.*}})
  return __builtin_riscv_ventus_vcvt_bf16_fp32(a0);
}

uint test_vadd_f16x2(uint a0, uint a1) {
  // CHECK-LABEL: @test_vadd_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vadd.f16x2(i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vadd_f16x2(a0, a1);
}

uint test_vmul_f16x2(uint a0, uint a1) {
  // CHECK-LABEL: @test_vmul_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vmul.f16x2(i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vmul_f16x2(a0, a1);
}

uint test_vfma_f16x2(uint a0, uint a1, uint a2) {
  // CHECK-LABEL: @test_vfma_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vfma.f16x2(i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vfma_f16x2(a0, a1, a2);
}

uint test_vadd_bf16x2(uint a0, uint a1) {
  // CHECK-LABEL: @test_vadd_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vadd.bf16x2(i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vadd_bf16x2(a0, a1);
}

uint test_vmul_bf16x2(uint a0, uint a1) {
  // CHECK-LABEL: @test_vmul_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vmul.bf16x2(i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vmul_bf16x2(a0, a1);
}

uint test_vfma_bf16x2(uint a0, uint a1, uint a2) {
  // CHECK-LABEL: @test_vfma_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vfma.bf16x2(i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}})
  return __builtin_riscv_ventus_vfma_bf16x2(a0, a1, a2);
}

float test_vex2_approx_f32(float x) {
  // CHECK-LABEL: @test_vex2_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vex2.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vex2_approx_f32(x);
}

float test_vlg2_approx_f32(float x) {
  // CHECK-LABEL: @test_vlg2_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vlg2.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vlg2_approx_f32(x);
}

float test_vrcp_approx_f32(float x) {
  // CHECK-LABEL: @test_vrcp_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vrcp.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vrcp_approx_f32(x);
}

float test_vsqrt_approx_f32(float x) {
  // CHECK-LABEL: @test_vsqrt_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vsqrt.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vsqrt_approx_f32(x);
}

float test_vrsqrt_approx_f32(float x) {
  // CHECK-LABEL: @test_vrsqrt_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vrsqrt.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vrsqrt_approx_f32(x);
}

float test_vsin_approx_f32(float x) {
  // CHECK-LABEL: @test_vsin_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vsin.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vsin_approx_f32(x);
}

float test_vcos_approx_f32(float x) {
  // CHECK-LABEL: @test_vcos_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vcos.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vcos_approx_f32(x);
}

float test_vtanh_approx_f32(float x) {
  // CHECK-LABEL: @test_vtanh_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vtanh.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vtanh_approx_f32(x);
}

float test_vgelu_approx_f32(float x) {
  // CHECK-LABEL: @test_vgelu_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vgelu.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vgelu_approx_f32(x);
}

float test_vsilu_approx_f32(float x) {
  // CHECK-LABEL: @test_vsilu_approx_f32
  // CHECK: call float @llvm.riscv.ventus.vsilu.approx.f32(float %{{.*}})
  return __builtin_riscv_ventus_vsilu_approx_f32(x);
}

uint test_vex2_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vex2_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vex2.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vex2_approx_f16x2(x);
}

uint test_vrcp_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vrcp_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vrcp.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vrcp_approx_f16x2(x);
}

uint test_vsqrt_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vsqrt_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vsqrt.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vsqrt_approx_f16x2(x);
}

uint test_vrsqrt_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vrsqrt_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vrsqrt.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vrsqrt_approx_f16x2(x);
}

uint test_vtanh_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vtanh_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vtanh.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vtanh_approx_f16x2(x);
}

uint test_vgelu_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vgelu_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vgelu.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vgelu_approx_f16x2(x);
}

uint test_vsilu_approx_f16x2(uint x) {
  // CHECK-LABEL: @test_vsilu_approx_f16x2
  // CHECK: call i32 @llvm.riscv.ventus.vsilu.approx.f16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vsilu_approx_f16x2(x);
}

uint test_vex2_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vex2_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vex2.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vex2_approx_bf16x2(x);
}

uint test_vrcp_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vrcp_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vrcp.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vrcp_approx_bf16x2(x);
}

uint test_vsqrt_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vsqrt_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vsqrt.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vsqrt_approx_bf16x2(x);
}

uint test_vrsqrt_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vrsqrt_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vrsqrt.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vrsqrt_approx_bf16x2(x);
}

uint test_vtanh_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vtanh_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vtanh.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vtanh_approx_bf16x2(x);
}

uint test_vgelu_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vgelu_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vgelu.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vgelu_approx_bf16x2(x);
}

uint test_vsilu_approx_bf16x2(uint x) {
  // CHECK-LABEL: @test_vsilu_approx_bf16x2
  // CHECK: call i32 @llvm.riscv.ventus.vsilu.approx.bf16x2(i32 %{{.*}})
  return __builtin_riscv_ventus_vsilu_approx_bf16x2(x);
}
