#include <clc/clc.h>

// #define __CLC_NATIVE_INTRINSIC sin

// #define __CLC_BODY <native_unary_intrinsic.inc>
// #define __FLOAT_ONLY
// #include <clc/math/gentype.inc>

__attribute__((overloadable)) __attribute__((always_inline)) float native_sin(float val) {
  return sin(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float2 native_sin(float2 val) {
  return sin(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float3 native_sin(float3 val) {
  return sin(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float4 native_sin(float4 val) {
  return sin(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float8 native_sin(float8 val) {
  return sin(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float16 native_sin(float16 val) {
  return sin(val);
}
