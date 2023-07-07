#include <clc/clc.h>

// #define __CLC_BODY <native_exp.inc>
// #define __FLOAT_ONLY
// #include <clc/math/gentype.inc>

__attribute__((overloadable)) __attribute__((always_inline)) float native_exp(float val) {
  return exp(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float2 native_exp(float2 val) {
  return exp(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float3 native_exp(float3 val) {
  return exp(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float4 native_exp(float4 val) {
  return exp(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float8 native_exp(float8 val) {
  return exp(val);
}

__attribute__((overloadable)) __attribute__((always_inline)) float16 native_exp(float16 val) {
  return exp(val);
}
