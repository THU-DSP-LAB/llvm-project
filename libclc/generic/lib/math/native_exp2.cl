#include <clc/clc.h>

// #define __CLC_NATIVE_INTRINSIC exp2

// #define __CLC_BODY <native_unary_intrinsic.inc>
// #define __FLOAT_ONLY
// #include <clc/math/gentype.inc>

__attribute__((overloadable)) __attribute__((always_inline)) float native_exp2(float val) {
  return exp2(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float2 native_exp2(float2 val) {
  return exp2(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float3 native_exp2(float3 val) {
  return exp2(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float4 native_exp2(float4 val) {
  return exp2(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float8 native_exp2(float8 val) {
  return exp2(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float16 native_exp2(float16 val) {
  return exp2(val);
}
