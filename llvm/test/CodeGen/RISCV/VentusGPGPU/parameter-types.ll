; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: write) vscale_range(1,2048)
; Here we only foucus on kernel argument loading instruction
define dso_local ventus_kernel void @test_kernel(i8 noundef %c, i8 noundef %uc, i16 noundef %s, i16 noundef %us, i32 noundef %i, i32 noundef %ui, float noundef %f, ptr addrspace(1) nocapture noundef writeonly align 4 %result) {
entry:
	; CHECK: flw	t0, 24(a0)
	; CHECK: lw	t1, 20(a0)
	; CHECK: lw	t2, 16(a0)
	; CHECK: lhu	s0, 12(a0)
	; CHECK: lh	s1, 8(a0)
	; CHECK: lb	a1, 0(a0)
	; CHECK: lbu	a2, 4(a0)
	; CHECK: lw	a0, 28(a0)
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
