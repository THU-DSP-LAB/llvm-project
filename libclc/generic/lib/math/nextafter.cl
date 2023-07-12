#include <clc/clc.h>
#include "../clcmacro.h"

extern float __nextafterf(float x, float y);

_CLC_DEFINE_BINARY_BUILTIN(float, nextafter, __nextafterf, float, float)

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern double __nextafter(double x, double y);
_CLC_DEFINE_BINARY_BUILTIN(double, nextafter, __nextafter, double, double)

#endif
