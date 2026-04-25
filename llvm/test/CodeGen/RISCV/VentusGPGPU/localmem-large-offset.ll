; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck --check-prefix=VENTUS %s

@buf = internal addrspace(3) global [600 x i32] zeroinitializer, align 4

define ventus_kernel void @test_localmem_big_offset(ptr addrspace(1) nocapture noundef writeonly %out) {
; VENTUS-LABEL: test_localmem_big_offset:
; VENTUS:       call _Z12get_local_idj
; VENTUS:       vadd12.vi v1, v0, 520
; VENTUS-NEXT:  vsll.vi v1, v1, 2
; VENTUS-NEXT:  addi [[BASE:[a-z0-9]+]], s0, -2048
; VENTUS-NEXT:  addi [[BASE]], [[BASE]], -352
; VENTUS-NEXT:  vadd.vx v1, v1, [[BASE]]
; VENTUS-NEXT:  vsw12.v v0, 0(v1)
entry:
  %lid = call i32 @_Z12get_local_idj(i32 noundef 0)
  %idx = add i32 %lid, 520
  %ptr = getelementptr inbounds [600 x i32], ptr addrspace(3) @buf, i32 0, i32 %idx
  store i32 %lid, ptr addrspace(3) %ptr, align 4
  %val = load i32, ptr addrspace(3) %ptr, align 4
  %outptr = getelementptr inbounds i32, ptr addrspace(1) %out, i32 %lid
  store i32 %val, ptr addrspace(1) %outptr, align 4
  ret void
}

declare dso_local i32 @_Z12get_local_idj(i32 noundef)
