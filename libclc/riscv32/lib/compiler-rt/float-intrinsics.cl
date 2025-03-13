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

fp_t _fmax(fp_t x, fp_t y) {
    return (__builtin_isnan(x)) ? y : ((__builtin_isnan(y)) ? x : ((x < y) ? y : x));
}

fp_t _fmin(fp_t x, fp_t y) {
    return (__builtin_isnan(x)) ? y : ((__builtin_isnan(y)) ? x : ((x < y) ? x : y));
}

double rint (double x)
{

  static const double TWO52[2] = {
    4.50359962737049600000e+15,
    -4.50359962737049600000e+15,
  };
  int64_t i0, sx;
  int32_t j0;
  do
    {
      ieee_double_shape_type gh_u;
      gh_u.value = (x);
      (i0) = gh_u.word;
    }
  while (0);
  sx = (i0 >> 63) & 1;
  j0 = ((i0 >> 52) & 0x7ff) - 0x3ff;
  if (j0 < 52)
    {
      if (j0 < 0)
	{
	  double w = TWO52[sx] + x;
	  double t = w - TWO52[sx];
	  do
	    {
	      ieee_double_shape_type gh_u;
	      gh_u.value = (t);
	      (i0) = gh_u.word;
	    }
	  while (0);
	  do
	    {
	      ieee_double_shape_type iw_u;
	      iw_u.word = ((i0 & 0x7fffffffffffffffUL) | (sx << 63));
	      (t) = iw_u.value;
	    }
	  while (0);

	  return t;
	}
    }
  else
    {
      if (j0 == 0x400)
	return x + x;
      else
	return x;
    }
  double w = TWO52[sx] + x;
  return w - TWO52[sx];
}

double
round (double x)
{




  int64_t i0, j0;

  do { ieee_double_shape_type gh_u; gh_u.value = (x); (i0) = gh_u.word; } while (0);
  j0 = ((i0 >> 52) & 0x7ff) - 0x3ff;
  if (__builtin_expect ((j0 < 52), 1))
    {
      if (j0 < 0)
 {
   i0 &= 0x8000000000000000UL;
   if (j0 == -1)
     i0 |= 0x3ff0000000000000UL;
 }
      else
 {
   uint64_t i = 0x000fffffffffffffUL >> j0;
   if ((i0 & i) == 0)

     return x;

   i0 += 0x0008000000000000UL >> j0;
   i0 &= ~i;
 }
    }
  else
    {
      if (j0 == 0x400)

 return x + x;
      else
 return x;
    }

  do { ieee_double_shape_type iw_u; iw_u.word = (i0); (x) = iw_u.value; } while (0);
  return x;

}

double ceil (double x)
{

  int64_t i0, i;
  int32_t j0;
  do
    {
      ieee_double_shape_type gh_u;
      gh_u.value = (x);
      (i0) = gh_u.word;
    }
  while (0);
  j0 = ((i0 >> 52) & 0x7ff) - 0x3ff;
  if (j0 <= 51)
    {
      if (j0 < 0)
	{

	  if (i0 < 0)
	    i0 = 0x8000000000000000L;
	  else if (i0 != 0)
	    i0 = 0x3ff0000000000000L;
	}
      else
	{
	  i = 0x000fffffffffffffL >> j0;
	  if ((i0 & i) == 0)
	    return x;
	  if (i0 > 0)
	    i0 += 0x0010000000000000UL >> j0;
	  i0 &= ~i;
	}
    }
  else
    {
      if (j0 == 0x400)
	return x + x;
      else
	return x;
    }
  do
    {
      ieee_double_shape_type iw_u;
      iw_u.word = (i0);
      (x) = iw_u.value;
    }
  while (0);
  return x;
}

double trunc (double x)
{

  int64_t i0, j0;
  int64_t sx;

  do
    {
      ieee_double_shape_type gh_u;
      gh_u.value = (x);
      (i0) = gh_u.word;
    }
  while (0);
  sx = i0 & 0x8000000000000000UL;
  j0 = ((i0 >> 52) & 0x7ff) - 0x3ff;
  if (j0 < 52)
    {
      if (j0 < 0)

	do
	  {
	    ieee_double_shape_type iw_u;
	    iw_u.word = (sx);
	    (x) = iw_u.value;
	  }
	while (0);
      else
	do
	  {
	    ieee_double_shape_type iw_u;
	    iw_u.word = (sx | (i0 & ~(0x000fffffffffffffUL >> j0)));
	    (x) = iw_u.value;
	  }
	while (0);
    }
  else
    {
      if (j0 == 0x400)

	return x + x;
    }

  return x;
}

static const double tiny = 1.0e-300;
double sqrt(double x) {
  double z;
  int32_t sign = (int)0x80000000;
  int32_t ix0, s0, q, m, t, i;
  uint32_t r, t1, s1, ix1, q1;

  EXTRACT_WORDS(ix0, ix1, x);

  /* take care of Inf and NaN */
  if ((ix0 & 0x7ff00000) == 0x7ff00000) {
    return x * x + x; /* sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=sNaN */
  }
  /* take care of zero */
  if (ix0 <= 0) {
    if (((ix0 & ~sign) | ix1) == 0)
      return x; /* sqrt(+-0) = +-0 */
    if (ix0 < 0)
      return (x - x) / (x - x); /* sqrt(-ve) = sNaN */
  }
  /* normalize x */
  m = ix0 >> 20;
  if (m == 0) { /* subnormal x */
    while (ix0 == 0) {
      m -= 21;
      ix0 |= (ix1 >> 11);
      ix1 <<= 21;
    }
    for (i = 0; (ix0 & 0x00100000) == 0; i++)
      ix0 <<= 1;
    m -= i - 1;
    ix0 |= ix1 >> (32 - i);
    ix1 <<= i;
  }
  m -= 1023; /* unbias exponent */
  ix0 = (ix0 & 0x000fffff) | 0x00100000;
  if (m & 1) { /* odd m, double x to make it even */
    ix0 += ix0 + ((ix1 & sign) >> 31);
    ix1 += ix1;
  }
  m >>= 1; /* m = [m/2] */

  /* generate sqrt(x) bit by bit */
  ix0 += ix0 + ((ix1 & sign) >> 31);
  ix1 += ix1;
  q = q1 = s0 = s1 = 0; /* [q,q1] = sqrt(x) */
  r = 0x00200000;       /* r = moving bit from right to left */

  while (r != 0) {
    t = s0 + r;
    if (t <= ix0) {
      s0 = t + r;
      ix0 -= t;
      q += r;
    }
    ix0 += ix0 + ((ix1 & sign) >> 31);
    ix1 += ix1;
    r >>= 1;
  }

  r = sign;
  while (r != 0) {
    t1 = s1 + r;
    t = s0;
    if (t < ix0 || (t == ix0 && t1 <= ix1)) {
      s1 = t1 + r;
      if ((t1 & sign) == sign && (s1 & sign) == 0)
        s0++;
      ix0 -= t;
      if (ix1 < t1)
        ix0--;
      ix1 -= t1;
      q1 += r;
    }
    ix0 += ix0 + ((ix1 & sign) >> 31);
    ix1 += ix1;
    r >>= 1;
  }

  /* use floating add to find out rounding direction */
  if ((ix0 | ix1) != 0) {
    z = 1.0 - tiny; /* raise inexact flag */
    if (z >= 1.0) {
      z = 1.0 + tiny;
      if (q1 == (uint32_t)0xffffffff) {
        q1 = 0;
        q++;
      } else if (z > 1.0) {
        if (q1 == (uint32_t)0xfffffffe)
          q++;
        q1 += 2;
      } else
        q1 += q1 & 1;
    }
  }
  ix0 = (q >> 1) + 0x3fe00000;
  ix1 = q1 >> 1;
  if (q & 1)
    ix1 |= sign;
  ix0 += m << 20;
  INSERT_WORDS(z, ix0, ix1);
  return z;
}

#endif
