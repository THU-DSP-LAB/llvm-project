; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

%struct.MyStruct = type { i32, i8, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: write) vscale_range(1,2048)
; Here we foucus on kernel struct argument
define dso_local ventus_kernel void @test_kernel1(i8 noundef %c, %struct.MyStruct %st.coerce, i8 noundef %uc, ptr addrspace(1) nocapture noundef writeonly align 4 %result) {
entry:
  ; CHECK: lb	  t0, 0(a0)
	; CHECK: lbu	t1, 24(a0)
	; CHECK: lw	  t2, 28(a0)
	; CHECK: lw	  s0, 8(a0)
  %st.coerce.fca.0.extract = extractvalue %struct.MyStruct %st.coerce, 0
  %conv = sitofp i8 %c to float
  store float %conv, ptr addrspace(1) %result, align 4
  %conv1 = sitofp i32 %st.coerce.fca.0.extract to float
  %arrayidx2 = getelementptr inbounds float, ptr addrspace(1) %result, i32 1
  store float %conv1, ptr addrspace(1) %arrayidx2, align 4
  %conv3 = uitofp i8 %uc to float
  %arrayidx4 = getelementptr inbounds float, ptr addrspace(1) %result, i32 2
  store float %conv3, ptr addrspace(1) %arrayidx4, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: write) vscale_range(1,2048)
; Here we foucus on scalar argument
define dso_local ventus_kernel void @test_kernel2(i8 noundef %c, i8 noundef %uc, i16 noundef %s, i16 noundef %us, i32 noundef %i, i32 noundef %ui, float noundef %f, ptr addrspace(1) nocapture noundef writeonly align 4 %result) {
entry:
  ; CHECK: flw	t0, 24(a0)
  ; CHECK: lw	  t1, 20(a0)
  ; CHECK: lw	  t2, 16(a0)
  ; CHECK: lhu	s0, 12(a0)
  ; CHECK: lh	  s1, 8(a0)
  ; CHECK: lb	  a1, 0(a0)
  ; CHECK: lbu	a2, 4(a0)
  ; CHECK: lw	  a0, 28(a0)
  %conv = sitofp i8 %c to float
  store float %conv, ptr addrspace(1) %result, align 4
  %conv1 = uitofp i8 %uc to float
  %arrayidx2 = getelementptr inbounds float, ptr addrspace(1) %result, i32 1
  store float %conv1, ptr addrspace(1) %arrayidx2, align 4
  %conv3 = sitofp i16 %s to float
  %arrayidx4 = getelementptr inbounds float, ptr addrspace(1) %result, i32 2
  store float %conv3, ptr addrspace(1) %arrayidx4, align 4
  %conv5 = uitofp i16 %us to float
  %arrayidx6 = getelementptr inbounds float, ptr addrspace(1) %result, i32 3
  store float %conv5, ptr addrspace(1) %arrayidx6, align 4
  %conv7 = sitofp i32 %i to float
  %arrayidx8 = getelementptr inbounds float, ptr addrspace(1) %result, i32 4
  store float %conv7, ptr addrspace(1) %arrayidx8, align 4
  %conv9 = uitofp i32 %ui to float
  %arrayidx10 = getelementptr inbounds float, ptr addrspace(1) %result, i32 5
  store float %conv9, ptr addrspace(1) %arrayidx10, align 4
  %arrayidx11 = getelementptr inbounds float, ptr addrspace(1) %result, i32 6
  store float %f, ptr addrspace(1) %arrayidx11, align 4
  ret void
}

; Function Attrs: convergent mustprogress nofree norecurse nounwind willreturn memory(argmem: write) vscale_range(1,2048)
; Here we foucus on vector argument
define dso_local ventus_kernel void @test_kernel3(<2 x i8> noundef %c, <2 x i8> noundef %uc, <2 x i16> noundef %s, <2 x i16> noundef %us, <2 x i32> noundef %i, <2 x i32> noundef %ui, <2 x float> noundef %f, ptr addrspace(1) nocapture noundef writeonly align 8 %result) {
entry:
  ;CHECK: flw	t0, 36(a0)
  ;CHECK: flw	t0, 32(a0)
  ;CHECK: lw	t0, 28(a0)
  ;CHECK: lw	t0, 24(a0)
  ;CHECK: lw	t0, 20(a0)
  ;CHECK: lw	t0, 16(a0)
  ;CHECK: lhu	t0, 14(a0)
  ;CHECK: lhu	t0, 12(a0)
  ;CHECK: lhu	t0, 10(a0)
  ;CHECK: lhu	t0, 8(a0)
  ;CHECK: lbu	t0, 5(a0)
  ;CHECK: lbu	t0, 4(a0)
  ;CHECK: lbu	t0, 1(a0)
  ;CHECK: lbu	t1, 0(a0)
  ;CHECK: lw	t2, 40(a0)
  %call = call <2 x float> @_Z14convert_float2Dv2_c(<2 x i8> noundef %c) 
  store <2 x float> %call, ptr addrspace(1) %result, align 8
  %call1 = call <2 x float> @_Z14convert_float2Dv2_h(<2 x i8> noundef %uc) 
  %arrayidx2 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 1
  store <2 x float> %call1, ptr addrspace(1) %arrayidx2, align 8
  %call3 = call <2 x float> @_Z14convert_float2Dv2_s(<2 x i16> noundef %s) 
  %arrayidx4 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 2
  store <2 x float> %call3, ptr addrspace(1) %arrayidx4, align 8
  %call5 = call <2 x float> @_Z14convert_float2Dv2_t(<2 x i16> noundef %us) 
  %arrayidx6 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 3
  store <2 x float> %call5, ptr addrspace(1) %arrayidx6, align 8
  %call7 = call <2 x float> @_Z14convert_float2Dv2_i(<2 x i32> noundef %i) 
  %arrayidx8 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 4
  store <2 x float> %call7, ptr addrspace(1) %arrayidx8, align 8
  %call9 = call <2 x float> @_Z14convert_float2Dv2_j(<2 x i32> noundef %ui) 
  %arrayidx10 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 5
  store <2 x float> %call9, ptr addrspace(1) %arrayidx10, align 8
  %call11 = call <2 x float> @_Z14convert_float2Dv2_f(<2 x float> noundef %f) 
  %arrayidx12 = getelementptr inbounds <2 x float>, ptr addrspace(1) %result, i32 6
  store <2 x float> %call11, ptr addrspace(1) %arrayidx12, align 8
  ret void
}

declare dso_local <2 x float> @_Z14convert_float2Dv2_c(<2 x i8> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_h(<2 x i8> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_s(<2 x i16> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_t(<2 x i16> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_i(<2 x i32> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_j(<2 x i32> noundef) 
declare dso_local <2 x float> @_Z14convert_float2Dv2_f(<2 x float> noundef)

; Function Attrs: convergent mustprogress nofree norecurse nounwind willreturn memory(argmem: write) vscale_range(1,2048)
; Here we foucus on vector argument
define dso_local ventus_kernel void @test_kernel4(<4 x i8> noundef %c, <4 x i8> noundef %uc, <4 x i16> noundef %s, <4 x i16> noundef %us, <4 x i32> noundef %i, <4 x i32> noundef %ui, <4 x float> noundef %f, ptr addrspace(1) nocapture noundef writeonly align 16 %result) {
entry:
  ;CHECK: flw	t0, 76(a0)
  ;CHECK: flw	t0, 72(a0)
  ;CHECK: flw	t0, 68(a0)
  ;CHECK: flw	t0, 64(a0)
  ;CHECK: lw	t0, 60(a0)
  ;CHECK: lw	t0, 56(a0)
  ;CHECK: lw	t0, 52(a0)
  ;CHECK: lw	t0, 48(a0)
  ;CHECK: lw	t0, 44(a0)
  ;CHECK: lw	t0, 40(a0)
  ;CHECK: lw	t0, 36(a0)
  ;CHECK: lw	t0, 32(a0)
  ;CHECK: lhu	t0, 22(a0)
  ;CHECK: lhu	t0, 20(a0)
  ;CHECK: lhu	t0, 18(a0)
  ;CHECK: lhu	t0, 16(a0)
  ;CHECK: lhu	t0, 14(a0)
  ;CHECK: lhu	t0, 12(a0)
  ;CHECK: lhu	t0, 10(a0)
  ;CHECK: lhu	t0, 8(a0)
  ;CHECK: lw	t0, 4(a0)
  ;CHECK: lbu	t1, 4(a0)
  ;CHECK: lw	t0, 80(a0)
  ;CHECK: lw	t0, 0(a0)
  ;CHECK: lbu	t1, 0(a0)
  %call = call <4 x float> @_Z14convert_float4Dv4_c(<4 x i8> noundef %c)   
  store <4 x float> %call, ptr addrspace(1) %result, align 16  
  %call1 = call <4 x float> @_Z14convert_float4Dv4_h(<4 x i8> noundef %uc)   
  %arrayidx2 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 1
  store <4 x float> %call1, ptr addrspace(1) %arrayidx2, align 16
  %call3 = call <4 x float> @_Z14convert_float4Dv4_s(<4 x i16> noundef %s)   
  %arrayidx4 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 2
  store <4 x float> %call3, ptr addrspace(1) %arrayidx4, align 16  
  %call5 = call <4 x float> @_Z14convert_float4Dv4_t(<4 x i16> noundef %us)   
  %arrayidx6 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 3
  store <4 x float> %call5, ptr addrspace(1) %arrayidx6, align 16 
  %call7 = call <4 x float> @_Z14convert_float4Dv4_i(<4 x i32> noundef %i)   
  %arrayidx8 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 4
  store <4 x float> %call7, ptr addrspace(1) %arrayidx8, align 16  
  %call9 = call <4 x float> @_Z14convert_float4Dv4_j(<4 x i32> noundef %ui)   
  %arrayidx10 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 5
  store <4 x float> %call9, ptr addrspace(1) %arrayidx10, align 16 
  %call11 = call <4 x float> @_Z14convert_float4Dv4_f(<4 x float> noundef %f)   
  %arrayidx12 = getelementptr inbounds <4 x float>, ptr addrspace(1) %result, i32 6
  store <4 x float> %call11, ptr addrspace(1) %arrayidx12, align 16
  ret void
}

declare dso_local <4 x float> @_Z14convert_float4Dv4_c(<4 x i8> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_h(<4 x i8> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_s(<4 x i16> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_t(<4 x i16> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_i(<4 x i32> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_j(<4 x i32> noundef)    
declare dso_local <4 x float> @_Z14convert_float4Dv4_f(<4 x float> noundef)    
