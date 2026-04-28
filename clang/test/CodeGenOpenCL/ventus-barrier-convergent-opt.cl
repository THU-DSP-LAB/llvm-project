// RUN: %clang_cc1 -cl-std=CL2.0 -triple riscv32-unknown-unknown -finclude-default-header -O1 -emit-llvm -o - %s \
// RUN: | FileCheck %s

// CHECK-LABEL: define {{.*}}@test(
// CHECK-COUNT-2: call void @llvm.riscv.ventus.barrier
// CHECK-NOT: call void @llvm.riscv.ventus.barrier
// CHECK: ret void
// CHECK-DAG: declare void @llvm.riscv.ventus.barrier(i32 immarg) #[[BARRIER_ATTR:[0-9]+]]
// CHECK-DAG: attributes #[[BARRIER_ATTR]] = { {{.*}}convergent{{.*}} }

kernel void test(global int *out, local int *scratch) {
  int tx = get_local_id(0);

  if (tx > 0)
    scratch[tx] = out[tx];

  barrier(CLK_LOCAL_MEM_FENCE);

  if (tx > 0)
    out[tx] = scratch[tx];

  barrier(CLK_LOCAL_MEM_FENCE);
}
