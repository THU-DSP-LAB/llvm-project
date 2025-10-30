; RUN: llc -march=riscv32 -mcpu=ventus-gpgpu -mattr=+f,+d -stop-after=ventus-promote-alloca < %s | FileCheck %s

;===----------------------------------------------------------------------===//
; Test case for VentusPromoteAlloca pass
;
; VentusPromoteAlloca eliminates stack allocations (alloca) in private address
; space (addrspace 5), converting them to SSA values (registers/vectors). This
; is a mem2reg optimization specifically for GPU private memory.
;
; Pass workflow:
; 1. Identify allocas in private address space (addrspace 5)
; 2. Verify all uses are simple load/store or convertible GEP
; 3. Convert loads to extractelement, stores to insertelement
; 4. Use SSAUpdater to track values across basic blocks
; 5. Eliminate alloca and all memory operations
;
; Optimization benefits:
; - Replace slow memory operations with fast register operations
; - Reduce private memory (stack) usage
; - Create more opportunities for subsequent optimizations
;===----------------------------------------------------------------------===//

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown"

; Test 1: Basic vector alloca promotion
; Verify that simple vector alloca and its load/store are eliminated and converted to direct SSA value usage
define ventus_kernel void @test_basic_vector(ptr addrspace(1) noundef align 16 %out) {
; CHECK-LABEL: define ventus_kernel void @test_basic_vector
; CHECK-NOT: alloca
; CHECK: store <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, ptr addrspace(1) %out
entry:
  %vec1 = alloca <4 x float>, align 16, addrspace(5)
  %vec2 = alloca <4 x float>, align 16, addrspace(5)
  store <4 x float> <float 1.0, float 2.0, float 3.0, float 4.0>, ptr addrspace(5) %vec2, align 16
  %tmp = load <4 x float>, ptr addrspace(5) %vec2, align 16
  store <4 x float> %tmp, ptr addrspace(5) %vec1, align 16
  %result = load <4 x float>, ptr addrspace(5) %vec1, align 16
  store <4 x float> %result, ptr addrspace(1) %out, align 16
  ret void
}

; Test 2: Array to vector conversion
; Verify that arrays can be converted to vectors and promoted
define ventus_kernel void @test_array_to_vector(ptr addrspace(1) noundef %out) {
; CHECK-LABEL: define ventus_kernel void @test_array_to_vector
; CHECK-NOT: alloca
; CHECK-NOT: getelementptr
; CHECK: %sum = add i32 10, 20
; CHECK: store i32 %sum, ptr addrspace(1) %out
entry:
  %arr = alloca [4 x i32], align 16, addrspace(5)
  %ptr0 = getelementptr inbounds [4 x i32], ptr addrspace(5) %arr, i32 0, i32 0
  store i32 10, ptr addrspace(5) %ptr0, align 4
  %ptr1 = getelementptr inbounds [4 x i32], ptr addrspace(5) %arr, i32 0, i32 1
  store i32 20, ptr addrspace(5) %ptr1, align 4
  %val0 = load i32, ptr addrspace(5) %ptr0, align 4
  %val1 = load i32, ptr addrspace(5) %ptr1, align 4
  %sum = add i32 %val0, %val1
  store i32 %sum, ptr addrspace(1) %out, align 4
  ret void
}

; Test 3: Cross-block promotion using SSAUpdater
; Verify that the pass correctly handles allocas used across multiple basic blocks
define ventus_kernel void @test_cross_block(i32 %cond, ptr addrspace(1) noundef %out) {
; CHECK-LABEL: define ventus_kernel void @test_cross_block
; CHECK-NOT: alloca
; CHECK: %vec.0 = phi <2 x i32> [ <i32 1, i32 2>, %if.then ], [ <i32 3, i32 4>, %if.else ]
; CHECK: store <2 x i32> %vec.0, ptr addrspace(1) %out
entry:
  %vec = alloca <2 x i32>, align 8, addrspace(5)
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  store <2 x i32> <i32 1, i32 2>, ptr addrspace(5) %vec, align 8
  br label %if.end

if.else:
  store <2 x i32> <i32 3, i32 4>, ptr addrspace(5) %vec, align 8
  br label %if.end

if.end:
  %result = load <2 x i32>, ptr addrspace(5) %vec, align 8
  store <2 x i32> %result, ptr addrspace(1) %out, align 8
  ret void
}

; Test 4: Element access using extractelement
; Verify that GEP-based element access is converted to extractelement operations
define ventus_kernel void @test_element_access(ptr addrspace(1) noundef %out) {
; CHECK-LABEL: define ventus_kernel void @test_element_access
; CHECK-NOT: alloca
; CHECK-NOT: getelementptr
; CHECK: extractelement <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 2
; CHECK: store float {{.*}}, ptr addrspace(1) %out
entry:
  %vec = alloca <4 x float>, align 16, addrspace(5)
  store <4 x float> <float 1.0, float 2.0, float 3.0, float 4.0>, ptr addrspace(5) %vec, align 16
  %ptr2 = getelementptr inbounds <4 x float>, ptr addrspace(5) %vec, i32 0, i32 2
  %elem2 = load float, ptr addrspace(5) %ptr2, align 4
  store float %elem2, ptr addrspace(1) %out, align 4
  ret void
}

; Test 5: Partial vector update using insertelement
; Verify that partial updates of vector elements work correctly
define ventus_kernel void @test_partial_update(float %newval, ptr addrspace(1) noundef %out) {
; CHECK-LABEL: define ventus_kernel void @test_partial_update
; CHECK-NOT: alloca
; CHECK: insertelement <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, float %newval, i32 1
; CHECK: store <4 x float> {{.*}}, ptr addrspace(1) %out
entry:
  %vec = alloca <4 x float>, align 16, addrspace(5)
  store <4 x float> <float 1.0, float 2.0, float 3.0, float 4.0>, ptr addrspace(5) %vec, align 16
  %ptr1 = getelementptr inbounds <4 x float>, ptr addrspace(5) %vec, i32 0, i32 1
  store float %newval, ptr addrspace(5) %ptr1, align 4
  %result = load <4 x float>, ptr addrspace(5) %vec, align 16
  store <4 x float> %result, ptr addrspace(1) %out, align 16
  ret void
}

