// RUN: %clang -target riscv32 -mcpu=ventus-gpgpu -S -emit-llvm %s -o - \
// RUN:   | FileCheck -check-prefixes=VENTUS %s

// VENTUS-LABEL: define{{.*}} void @foo(<2 x float> noundef %a, <4 x i32> noundef %bar, <4 x float> noundef %b, i32 noundef %c, <8 x float> noundef %cc, <16 x float> noundef %dd, ptr nocapture noundef readonly %0, ptr nocapture noundef readonly %1)
void foo(float2 a, int4 bar, float4 b, int c, float8 cc, float16 dd, __global float *gout, __local float *lout) {
  float2 tmp = pown(a, c);
  *gout = tmp.s0 + b.s2;
  *lout = tmp.s1 + b.s3;
}

// VENTUS-LABEL: define{{.*}} <8 x float> @bar()
float8 bar() {
  return (float8)(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
}
