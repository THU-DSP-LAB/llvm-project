#include <clc/clc.h>
#include "../clcmacro.h"

extern float __nextafterf(float x, float y)

// _CLC_DEFINE_BINARY_BUILTIN(float, nextafter, __builtin_nextafterf, float, float)
__attribute__((always_inline)) __attribute__((overloadable)) float nextafter(float x, float y) { return __nextafterf(x, y); }
__attribute__((overloadable)) __attribute__((always_inline)) float2 nextafter(float2 x, float2 y) { return (float2)(nextafter(x.x, y.x), nextafter(x.y, y.y)); }
__attribute__((overloadable)) __attribute__((always_inline)) float3 nextafter(float3 x, float3 y) { return (float3)(nextafter(x.x, y.x), nextafter(x.y, y.y), nextafter(x.z, y.z)); }
__attribute__((overloadable)) __attribute__((always_inline)) float4 nextafter(float4 x, float4 y) { return (float4)(nextafter(x.lo, y.lo), nextafter(x.hi, y.hi)); }
__attribute__((overloadable)) __attribute__((always_inline)) float8 nextafter(float8 x, float8 y) { return (float8)(nextafter(x.lo, y.lo), nextafter(x.hi, y.hi)); }
__attribute__((overloadable)) __attribute__((always_inline)) float16 nextafter(float16 x, float16 y) { return (float16)(nextafter(x.lo, y.lo), nextafter(x.hi, y.hi)); }

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

_CLC_DEFINE_BINARY_BUILTIN(double, nextafter, __builtin_nextafter, double, double)

#endif
