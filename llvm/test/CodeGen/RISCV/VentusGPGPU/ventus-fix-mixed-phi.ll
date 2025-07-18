; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -march=riscv32 -mcpu=ventus-gpgpu -mattr=+f,+d -verify-machineinstrs < %s | FileCheck %s

;===----------------------------------------------------------------------===//
; Test case for VentusFixMixedPHI pass
;
; The VentusFixMixedPHI pass addresses a specific issue in Ventus GPGPU code
; generation where PHI nodes have VGPR (Vector General Purpose Register) 
; results but receive inputs from GPR/GPRF32/FPR (scalar registers).
;
; This creates illegal machine code because the PHI node expects all inputs
; to be from the same register class as the result. The pass fixes this by:
;
; 1. Detecting PHI nodes with VGPR results and non-VGPR inputs
; 2. Inserting VMV.V.X instructions in predecessor blocks to convert 
;    scalar inputs to vector registers
; 3. Handling immediate values by:
;    - Using direct ADDI for small immediates (-2048 to 2047)
;    - Using LUI+ADDI sequence for large immediates
;    - Reusing existing constant loads when possible
; 4. Updating PHI operands to use the new VGPR registers
;
; The tests below verify various scenarios:
; - Basic PHI with small immediate constants
; - PHI with large immediate constants requiring LUI+ADDI
; - Multi-predecessor PHI nodes
; - PHI with variable (non-immediate) inputs  
; - PHI with zero and negative immediate values
;===----------------------------------------------------------------------===//

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown"

; Test 1: Basic PHI with VGPR result and GPR inputs
; This test verifies that small immediate constants (42, 100) are properly
; converted using ADDI+VMV.V.X sequence in each predecessor block.
define void @test_basic_phi(i32 %cond, ptr addrspace(1) %out) {
; CHECK-LABEL: test_basic_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         vmv.v.x v3, zero
; CHECK:         li      t0, 42
; CHECK:         vmv.v.x v2, t0
; CHECK:         vbne    v0, v3, .LBB0_2
; CHECK:       .LBB0_1: # %if.else
; CHECK:         li      t0, 100
; CHECK:         vmv.v.x v2, t0
; CHECK:       .LBB0_2: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v2, 0(v1)
; CHECK:         ret
entry:
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %val = phi i32 [ 42, %if.then ], [ 100, %if.else ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
}

; Test 2: PHI with large immediate constants (requires LUI+ADDI)
; This test verifies that large immediate constants that don't fit in 12-bit
; signed immediate are properly handled using LUI+ADDI sequence.
define void @test_large_immediate_phi(i32 %cond, ptr addrspace(1) %out) {
; CHECK-LABEL: test_large_immediate_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         vmv.v.x v3, zero
; CHECK:         lui     t0, 305176
; CHECK:         addi    t0, t0, -896
; CHECK:         vmv.v.x v2, t0
; CHECK:         vbne    v0, v3, .LBB1_2
; CHECK:       .LBB1_1: # %if.else
; CHECK:         lui     t0, 610
; CHECK:         addi    t0, t0, 1440
; CHECK:         vmv.v.x v2, t0
; CHECK:       .LBB1_2: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v2, 0(v1)
; CHECK:         ret
entry:
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %val = phi i32 [ 1250000000, %if.then ], [ 2500000, %if.else ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
}

; Test 3: Multi-predecessor PHI with mixed register types
; This test verifies handling of PHI nodes with more than 2 predecessors,
; ensuring each predecessor block gets the appropriate conversion.
define void @test_multi_predecessor_phi(i32 %cond, ptr addrspace(1) %out) {
; CHECK-LABEL: test_multi_predecessor_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         li      t0, 1000
; CHECK:         li      t1, 9
; CHECK:         vmv.v.x v3, t1
; CHECK:         vmv.v.x v2, t0
; CHECK:         vblt    v3, v0, .LBB2_3
; CHECK:       .LBB2_1: # %if.small
; CHECK:         vmv.v.x v3, zero
; CHECK:         vmv.v.x v2, zero
; CHECK:         vbeq    v0, v3, .LBB2_3
; CHECK:       .LBB2_2: # %if.positive
; CHECK:         li      t0, 5
; CHECK:         vmv.v.x v2, t0
; CHECK:       .LBB2_3: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v2, 0(v1)
; CHECK:         ret
entry:
  %cmp1 = icmp sge i32 %cond, 10
  br i1 %cmp1, label %if.big, label %if.small

if.big:
  br label %if.end

if.small:
  %cmp2 = icmp eq i32 %cond, 0
  br i1 %cmp2, label %if.end, label %if.positive

if.positive:
  br label %if.end

if.end:
  %val = phi i32 [ 1000, %if.big ], [ 0, %if.small ], [ 5, %if.positive ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
}

; Test 4: PHI with variable inputs (non-immediate)
; This test verifies that non-immediate GPR values are properly converted
; to VGPR using VMV.V.X instruction.
define void @test_variable_phi(i32 %cond, i32 %a, i32 %b, ptr addrspace(1) %out) {
; CHECK-LABEL: test_variable_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         vmv.v.x v4, zero
; CHECK:         vbne    v0, v4, .LBB3_2
; CHECK:       .LBB3_1: # %if.else
; CHECK:         vadd.vx v1, v2, zero
; CHECK:       .LBB3_2: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v1, 0(v3)
; CHECK:         ret
entry:
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %val = phi i32 [ %a, %if.then ], [ %b, %if.else ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
}

; Test 5: PHI with zero immediate (special case)
; This test verifies that zero immediate is handled correctly, which can
; use VMV.V.X with the zero register directly.
define void @test_zero_phi(i32 %cond, ptr addrspace(1) %out) {
; CHECK-LABEL: test_zero_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         vmv.v.x v3, zero
; CHECK:         vmv.v.x v2, zero
; CHECK:         vbne    v0, v3, .LBB4_2
; CHECK:       .LBB4_1: # %if.else
; CHECK:         li      t0, 1
; CHECK:         vmv.v.x v2, t0
; CHECK:       .LBB4_2: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v2, 0(v1)
; CHECK:         ret
entry:
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %val = phi i32 [ 0, %if.then ], [ 1, %if.else ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
}

; Test 6: PHI with negative immediate
; This test verifies that negative immediate values are properly handled
; using ADDI with negative immediate values.
define void @test_negative_phi(i32 %cond, ptr addrspace(1) %out) {
; CHECK-LABEL: test_negative_phi:
; CHECK:       # %bb.0: # %entry
; CHECK:         vmv.v.x v3, zero
; CHECK:         li      t0, -42
; CHECK:         vmv.v.x v2, t0
; CHECK:         vbne    v0, v3, .LBB5_2
; CHECK:       .LBB5_1: # %if.else
; CHECK:         li      t0, -100
; CHECK:         vmv.v.x v2, t0
; CHECK:       .LBB5_2: # %if.end
; CHECK:         join    zero, zero, 0
; CHECK:         vsw12.v v2, 0(v1)
; CHECK:         ret
entry:
  %tobool = icmp ne i32 %cond, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %val = phi i32 [ -42, %if.then ], [ -100, %if.else ]
  store i32 %val, ptr addrspace(1) %out, align 4
  ret void
} 
