#include <clc/clc.h>

// #define __CLC_NATIVE_INTRINSIC log10

// #define __CLC_BODY <native_unary_intrinsic.inc>
// #define __FLOAT_ONLY
// #include <clc/math/gentype.inc>

__attribute__((overloadable)) __attribute__((always_inline)) float native_log10(float val) {
  return log10(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float2 native_log10(float2 val) {
  return log10(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float3 native_log10(float3 val) {
  return log10(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float4 native_log10(float4 val) {
  return log10(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float8 native_log10(float8 val) {
  return log10(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float16 native_log10(float16 val) {
  return log10(val);
}
