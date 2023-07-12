
/* @(#)s_nextafter.c 5.1 93/09/24 */
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
//===---------------------------nextafter.cl-------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "types.h"


float __nextafterf(float x, float y) {
  int hx, hy, ix, iy;

  do {
    ieee_float_shape_type gf_u;
    gf_u.value = (x);
    (hx) = gf_u.word;
  } while (0);
  do {
    ieee_float_shape_type gf_u;
    gf_u.value = (y);
    (hy) = gf_u.word;
  } while (0);
  ix = hx & 0x7fffffff;
  iy = hy & 0x7fffffff;

  if ((ix > 0x7f800000) || (iy > 0x7f800000))
    return x + y;
  if (x == y)
    return y;
  if (ix == 0) {
    float u;
    do {
      ieee_float_shape_type sf_u;
      sf_u.word = ((hy & 0x80000000) | 1);
      (x) = sf_u.value;
    } while (0);
    u = ({
      __typeof(x) __x = (x);
      __asm("" : "+m"(__x));
      __x;
    });
    u = u * u;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    return x;
  }
  if (hx >= 0) {
    if (hx > hy) {
      hx -= 1;
    } else {
      hx += 1;
    }
  } else {
    if (hy >= 0 || hx > hy) {
      hx -= 1;
    } else {
      hx += 1;
    }
  }
  hy = hx & 0x7f800000;
  if (hy >= 0x7f800000) {
    float u = x + x;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    //  ((*__errno_location ()) = (ERANGE));
  }
  if (hy < 0x00800000) {
    float u = x * x;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    //  ((*__errno_location ()) = (ERANGE));
  }
  do {
    ieee_float_shape_type sf_u;
    sf_u.word = (hx);
    (x) = sf_u.value;
  } while (0);
  return x;
}

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

double
__nextafter (double x, double y)
{
  int32_t hx, hy, ix, iy;
  uint32_t lx, ly;

  do
    {
      ieee_double_shape_type ew_u;
      ew_u.value = (x);
      (hx) = ew_u.parts.msw;
      (lx) = ew_u.parts.lsw;
    }
  while (0);
  do
    {
      ieee_double_shape_type ew_u;
      ew_u.value = (y);
      (hy) = ew_u.parts.msw;
      (ly) = ew_u.parts.lsw;
    }
  while (0);
  ix = hx & 0x7fffffff;
  iy = hy & 0x7fffffff;

  if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0)
      || ((iy >= 0x7ff00000) && ((iy - 0x7ff00000) | ly) != 0))
    return x + y;
  if (x == y)
    return y;
  if ((ix | lx) == 0)
    {
      double u;
      do
	{
	  ieee_double_shape_type iw_u;
	  iw_u.parts.msw = (hy & 0x80000000);
	  iw_u.parts.lsw = (1);
	  (x) = iw_u.value;
	}
      while (0);
      u = ({
	__typeof (x) __x = (x);
	__asm ("" : "+m"(__x));
	__x;
      });
      u = u * u;
      ({
	__typeof (u) __x = (u);
	__asm __volatile__ ("" : : "m"(__x));
      });
      return x;
    }
  if (hx >= 0)
    {
      if (hx > hy || ((hx == hy) && (lx > ly)))
	{
	  if (lx == 0)
	    hx -= 1;
	  lx -= 1;
	}
      else
	{
	  lx += 1;
	  if (lx == 0)
	    hx += 1;
	}
    }
  else
    {
      if (hy >= 0 || hx > hy || ((hx == hy) && (lx > ly)))
	{
	  if (lx == 0)
	    hx -= 1;
	  lx -= 1;
	}
      else
	{
	  lx += 1;
	  if (lx == 0)
	    hx += 1;
	}
    }
  hy = hx & 0x7ff00000;
  if (hy >= 0x7ff00000)
    {
      double u = x + x;
      ({
	__typeof (u) __x = (u);
	__asm __volatile__ ("" : : "m"(__x));
      });
      // __set_errno (34);
    }
  if (hy < 0x00100000)
    {
      double u = x * x;
      ({
	__typeof (u) __x = (u);
	__asm __volatile__ ("" : : "m"(__x));
      });
      // __set_errno (34);
    }
  do
    {
      ieee_double_shape_type iw_u;
      iw_u.parts.msw = (hx);
      iw_u.parts.lsw = (lx);
      (x) = iw_u.value;
    }
  while (0);
  return x;
}

#endif
