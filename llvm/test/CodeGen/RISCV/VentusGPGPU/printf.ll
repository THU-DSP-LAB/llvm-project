; RUN: opt -mtriple=riscv32 -ventus-printf-runtime-binding -mcpu=ventus-gpgpu -S < %s

@.str = private unnamed_addr addrspace(2) constant [6 x i8] c"%s:%d\00", align 1

define ventus_kernel void @test_kernel(i32 %n) {
entry:
  %str = alloca [9 x i8], align 1, addrspace(5)
  %arraydecay = getelementptr inbounds [9 x i8], [9 x i8] addrspace(5)* %str, i32 0, i32 0
  %call1 = call i32 (i8 addrspace(2)*, ...) @printf(i8 addrspace(2)* getelementptr inbounds ([6 x i8], [6 x i8] addrspace(2)* @.str, i32 0, i32 0), i8 addrspace(5)* %arraydecay, i32 %n)
  ret void
}

declare i32 @printf(i8 addrspace(2)*, ...)
