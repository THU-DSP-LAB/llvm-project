/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4


union __double_repr { double __f; __uint64_t __i; };

#define __FLOAT_BITS(f) (((union __float_repr){ (float)(f) }).__i)
#define __DOUBLE_BITS(f) (((union __double_repr){ (double)(f) }).__i)

#define _fpclassify(x) ( \
	sizeof(x) == sizeof(double) ? __fpclassify(x) : \
	__fpclassifyl(x) )

#define _isinf(x) ( \
	sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & (__uint64_t)-1>>1) == (__uint64_t)0x7ff<<52 : \
	__fpclassifyl(x) == FP_INFINITE)

#define _isnan(x) ( \
	sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & (__uint64_t)-1>>1) > (__uint64_t)0x7ff<<52 : \
	__fpclassifyl(x) == FP_NAN)

#define _isnormal(x) ( \
	sizeof(x) == sizeof(double) ? ((__DOUBLE_BITS(x)+((__uint64_t)1<<52)) & (__uint64_t)-1>>1) >= (__uint64_t)1<<53 : \
	__fpclassifyl(x) == FP_NORMAL)

#define _isfinite(x) ( \
	(__DOUBLE_BITS(x) & (__uint64_t)-1>>1) < (__uint64_t)0x7ff<<52)

// If there are other needs, we can define other
bool isfinite(double x) {
  return _isfinite(x);
}

#endif
