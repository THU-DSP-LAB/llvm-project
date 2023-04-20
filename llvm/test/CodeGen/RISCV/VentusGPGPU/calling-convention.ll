; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs -O1 < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s

define dso_local spir_kernel void @_kernel(ptr addrspace(1) nocapture noundef align 4 %A, ptr addrspace(1) nocapture noundef readonly align 4 %B){
entry:
  %call = tail call i32 @_Z13get_global_idj(i32 noundef 0)
  %arrayidx = getelementptr inbounds float, ptr addrspace(1) %B, i32 %call
  ; VENTUS: lw	s0, 0(a0)
  %0 = load float, ptr addrspace(1) %arrayidx, align 4
  %arrayidx1 = getelementptr inbounds float, ptr addrspace(1) %A, i32 %call
  ; VENTUS: lw	s1, 4(a0)
  %1 = load float, ptr addrspace(1) %arrayidx1, align 4
  %add = fadd float %0, %1
  store float %add, ptr addrspace(1) %arrayidx1, align 4
  ret void
}

; THis non-kernel function takes 3 arguments
; The expected behavior is that all arguments are passsed by registers
; Function Attrs: convergent mustprogress nofree nounwind willreturn memory(none)
declare dso_local i32 @_Z13get_global_idj(i32 noundef) local_unnamed_addr

define dso_local i32 @non_kernel_1(ptr nocapture noundef readonly %a, ptr nocapture noundef readonly %b, ptr nocapture noundef readonly %c){
entry:
  ; VENTUS: vlw12.v v0, 0(v0)
  %0 = load i32, ptr %a, align 4
  ; VENTUS: vlw12.v v1, 0(v1)
  %1 = load i32, ptr %b, align 4
  %add = add nsw i32 %1, %0
  ; VENTUS: vlw12.v v2, 0(v2)
  %2 = load i32, ptr %c, align 4
  %add1 = add nsw i32 %add, %2
  ret i32 %add1
}

; THis non-kernel function takes 34 arguments, the range is beyond 32
; so the left two arguments need to be passed by tp stack
; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(read, inaccessiblemem: none)
define dso_local i32 @non_kernel(ptr nocapture noundef readonly %a1, ptr nocapture noundef readonly %a2, ptr nocapture noundef readonly %a3, ptr nocapture noundef readonly %a4, ptr nocapture noundef readonly %a5, ptr nocapture noundef readonly %a6, ptr nocapture noundef readonly %a7, ptr nocapture noundef readonly %a8, ptr nocapture noundef readonly %a9, ptr nocapture noundef readonly %a10, ptr nocapture noundef readonly %a11, ptr nocapture noundef readonly %a12, ptr nocapture noundef readonly %a13, ptr nocapture noundef readonly %a14, ptr nocapture noundef readonly %a15, ptr nocapture noundef readonly %a16, ptr nocapture noundef readonly %a17, ptr nocapture noundef readonly %a18, ptr nocapture noundef readonly %a19, ptr nocapture noundef readonly %a20, ptr nocapture noundef readonly %a21, ptr nocapture noundef readonly %a22, ptr nocapture noundef readonly %a23, ptr nocapture noundef readonly %a24, ptr nocapture noundef readonly %a25, ptr nocapture noundef readonly %a26, ptr nocapture noundef readonly %a27, ptr nocapture noundef readonly %a28, ptr nocapture noundef readonly %a29, ptr nocapture noundef readonly %a30, ptr nocapture noundef readonly %a31, ptr nocapture noundef readonly %a32, ptr addrspace(5) nocapture noundef readonly %0, ptr addrspace(5) nocapture noundef readonly %1) {
entry:
  ; VENTUS: vlw.v v{{[1-9]+}}, -16(tp)
  %a33 = load ptr, ptr addrspace(5) %0, align 4
  ; VENTUS: vlw.v v{{[1-9]+}}, -12(tp)
  %a34 = load ptr, ptr addrspace(5) %1, align 4
  ; VENTUS: vlw12.v	v0, 0(v0)
  %2 = load i32, ptr %a1, align 4
  ; VENTUS: vlw12.v	v1, 0(v1)
  %3 = load i32, ptr %a2, align 4
  %add = add nsw i32 %3, %2
  ; VENTUS: vlw12.v	v2, 0(v2)
  %4 = load i32, ptr %a3, align 4
  %add1 = add nsw i32 %add, %4
  ; VENTUS: vlw12.v	v3, 0(v3)
  %5 = load i32, ptr %a4, align 4
  %add2 = add nsw i32 %add1, %5
  %6 = load i32, ptr %a5, align 4
  ; VENTUS: vlw12.v	v{{[1-9]+}}, 0(v4)
  %add3 = add nsw i32 %add2, %6
  %7 = load i32, ptr %a6, align 4
  %add4 = add nsw i32 %add3, %7
  ; VENTUS: vlw12.v	v{{[1-9]+}}, 0(v5)
  %8 = load i32, ptr %a7, align 4
  %add5 = add nsw i32 %add4, %8
  %9 = load i32, ptr %a8, align 4
  %add6 = add nsw i32 %add5, %9
  %10 = load i32, ptr %a9, align 4
  %add7 = add nsw i32 %add6, %10
  %11 = load i32, ptr %a10, align 4
  %add8 = add nsw i32 %add7, %11
  %12 = load i32, ptr %a11, align 4
  %add9 = add nsw i32 %add8, %12
  %13 = load i32, ptr %a12, align 4
  %add10 = add nsw i32 %add9, %13
  %14 = load i32, ptr %a13, align 4
  %add11 = add nsw i32 %add10, %14
  %15 = load i32, ptr %a14, align 4
  %add12 = add nsw i32 %add11, %15
  %16 = load i32, ptr %a15, align 4
  %add13 = add nsw i32 %add12, %16
  %17 = load i32, ptr %a16, align 4
  %add14 = add nsw i32 %add13, %17
  %18 = load i32, ptr %a17, align 4
  %add15 = add nsw i32 %add14, %18
  %19 = load i32, ptr %a18, align 4
  %add16 = add nsw i32 %add15, %19
  %20 = load i32, ptr %a19, align 4
  %add17 = add nsw i32 %add16, %20
  %21 = load i32, ptr %a20, align 4
  %add18 = add nsw i32 %add17, %21
  %22 = load i32, ptr %a21, align 4
  %add19 = add nsw i32 %add18, %22
  %23 = load i32, ptr %a22, align 4
  %add20 = add nsw i32 %add19, %23
  %24 = load i32, ptr %a23, align 4
  %add21 = add nsw i32 %add20, %24
  %25 = load i32, ptr %a24, align 4
  %add22 = add nsw i32 %add21, %25
  %26 = load i32, ptr %a25, align 4
  %add23 = add nsw i32 %add22, %26
  %27 = load i32, ptr %a26, align 4
  %add24 = add nsw i32 %add23, %27
  %28 = load i32, ptr %a27, align 4
  %add25 = add nsw i32 %add24, %28
  %29 = load i32, ptr %a28, align 4
  %add26 = add nsw i32 %add25, %29
  %30 = load i32, ptr %a29, align 4
  %add27 = add nsw i32 %add26, %30
  %31 = load i32, ptr %a30, align 4
  %add28 = add nsw i32 %add27, %31
  %32 = load i32, ptr %a31, align 4
  %add29 = add nsw i32 %add28, %32
  %33 = load i32, ptr %a32, align 4
  %add30 = add nsw i32 %add29, %33
  ; VENTUS: vlw12.v	v{{[1-9]+}}, 0(v30)
  %34 = load i32, ptr %a33, align 4
  %add31 = add nsw i32 %add30, %34
  ; VENTUS: vlw12.v	v{{[1-9]+}}, 0(v31)
  %35 = load i32, ptr %a34, align 4
  %add32 = add nsw i32 %add31, %35
  ret i32 %add32
}
