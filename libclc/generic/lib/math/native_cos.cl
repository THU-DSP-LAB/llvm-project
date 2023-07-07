#include <clc/clc.h>

// #define __CLC_BODY <native_unary_intrinsic.inc>
// #define __FLOAT_ONLY
// #include <clc/math/gentype.inc>

__attribute__((overloadable)) __attribute__((always_inline)) float native_cos(float val) {
  return cos(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float2 native_cos(float2 val) {
  return cos(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float3 native_cos(float3 val) {
  return cos(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float4 native_cos(float4 val) {
  return cos(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float8 native_cos(float8 val) {
  return cos(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float16 native_cos(float16 val) {
  return cos(val);
}

