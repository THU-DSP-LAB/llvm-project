; RUN: opt -S -mtriple=riscv32 -passes=ventus-always-inline %s | FileCheck --check-prefix=ALL %s
; RUN: opt -S -mtriple=riscv32 -ventus-stress-function-calls -passes=ventus-always-inline %s | FileCheck --check-prefix=ALL %s

@local0 = addrspace(3) global i32 undef, align 4
@local1 = addrspace(3) global [512 x i32] undef, align 4
@nested.local.address = addrspace(1) global ptr addrspace(3) @local0, align 4

@alias.local0 = alias i32, ptr addrspace(3) @local0
@local.cycle = addrspace(3) global i32 ptrtoint (ptr addrspace(3) @local.cycle to i32), align 4


; ALL-LABEL: define i32 @load_local_simple() #0 {
define i32 @load_local_simple() {
  %load = load i32, ptr addrspace(3) @local0, align 4
  ret i32 %load
}

; ALL-LABEL: define i32 @load_local_const_gep() #0 {
define i32 @load_local_const_gep() {
  %load = load i32, ptr addrspace(3) getelementptr inbounds ([512 x i32], ptr addrspace(3) @local1, i64 0, i64 4), align 4
  ret i32 %load
}

; ALL-LABEL: define i32 @load_local_var_gep(i32 %idx) #0 {
define i32 @load_local_var_gep(i32 %idx) {
  %gep = getelementptr inbounds [512 x i32], ptr addrspace(3) @local1, i32 0, i32 %idx
  %load = load i32, ptr addrspace(3) %gep, align 4
  ret i32 %load
}

; ALL-LABEL: define ptr addrspace(3) @load_nested_address(i32 %idx) #0 {
define ptr addrspace(3) @load_nested_address(i32 %idx) {
  %load = load ptr addrspace(3), ptr addrspace(1) @nested.local.address, align 4
  ret ptr addrspace(3) %load
}

; ALL-LABEL: define i32 @load_local_alias() #0 {
define i32 @load_local_alias() {
  %load = load i32, ptr addrspace(3) @alias.local0, align 4
  ret i32 %load
}

; ALL-LABEL: define i32 @load_local_cycle() #0 {
define i32 @load_local_cycle() {
  %load = load i32, ptr addrspace(3) @local.cycle, align 4
  ret i32 %load
}

; ALL-LABEL: define i1 @icmp_local_address() #0 {
define i1 @icmp_local_address() {
  ret i1 icmp eq (ptr addrspace(3) @local0, ptr addrspace(3) null)
}

; ALL-LABEL: define i32 @transitive_call() #0 {
define i32 @transitive_call() {
  %call = call i32 @load_local_simple()
  ret i32 %call
}

; ALL-LABEL: define i32 @recursive_call_local(i32 %arg0) #0 {
define i32 @recursive_call_local(i32 %arg0) {
  %load = load i32, ptr addrspace(3) @local0, align 4
  %add = add i32 %arg0, %load
  %call = call i32 @recursive_call_local(i32 %add)
  ret i32 %call
}

; ALL-LABEL: define i32 @load_local_simple_noinline() #0 {
define i32 @load_local_simple_noinline() noinline {
  %load = load i32, ptr addrspace(3) @local0, align 4
  ret i32 %load
}

; ALL-LABEL: define i32 @recursive_call_local_noinline(i32 %arg0) #0 {
define i32 @recursive_call_local_noinline(i32 %arg0) noinline {
  %load = load i32, ptr addrspace(3) @local0, align 4
  %add = add i32 %arg0, %load
  %call = call i32 @recursive_call_local(i32 %add)
  ret i32 %call
}

; ALL-LABEL: define ventus_kernel void @kernel_with_local_access(
define ventus_kernel void @kernel_with_local_access(ptr addrspace(1) %out) {
  %load = load i32, ptr addrspace(3) @local0, align 4
  store i32 %load, ptr addrspace(1) %out, align 4
  ret void
}

; ALL: attributes #0 = { alwaysinline }
