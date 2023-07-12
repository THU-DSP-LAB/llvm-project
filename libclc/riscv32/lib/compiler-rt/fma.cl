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
#include "libm.h"
extern int isfinite(double);
union ld80 {
	double x;
	struct {
		uint64_t m;
		uint16_t e;
		uint16_t s;
		uint16_t pad;
	} bits;
};

/* exact add, assumes exponent_x >= exponent_y */
static void add(double *hi, double *lo, double x, double y)
{
	double r;

	r = x + y;
	*hi = r;
	r -= x;
	*lo = y - r;
}

/* exact mul, assumes no over/underflow */
static void mul(double *hi, double *lo, double x, double y)
{
	static const double c = 1.0 + 0x1p32L;
	double cx, xh, xl, cy, yh, yl;

	cx = c*x;
	xh = (x - cx) + cx;
	xl = x - xh;
	cy = c*y;
	yh = (y - cy) + cy;
	yl = y - yh;
	*hi = x*y;
	*lo = (xh*yh - *hi) + xh*yl + xl*yh + xl*yl;
}

/*
assume (double)(hi+lo) == hi
return an adjusted hi so that rounding it to double (or less) precision is correct
*/
static double adjust(double hi, double lo)
{
	union ld80 uhi, ulo;

	if (lo == 0)
		return hi;
	uhi.x = hi;
	if (uhi.bits.m & 0x3ff)
		return hi;
	ulo.x = lo;
	if (uhi.bits.s == ulo.bits.s)
		uhi.bits.m++;
	else {
		uhi.bits.m--;
		/* handle underflow and take care of ld80 implicit msb */
		if (uhi.bits.m == (uint64_t)-1/2) {
			uhi.bits.m *= 2;
			uhi.bits.e--;
		}
	}
	return uhi.x;
}

/* adjusted add so the result is correct when rounded to double (or less) precision */
static double dadd(double x, double y)
{
	add(&x, &y, x, y);
	return adjust(x, y);
}

/* adjusted mul so the result is correct when rounded to double (or less) precision */
static double dmul(double x, double y)
{
	mul(&x, &y, x, y);
	return adjust(x, y);
}

static int getexp(double x)
{
	union ld80 u;
	u.x = x;
	return u.bits.e;
}

double fma(double x, double y, double z)
{
	double hi, lo1, lo2, xy;
	int round, ez, exy;

	/* handle +-inf,nan */
	if (!isfinite(x) || !isfinite(y))
		return x*y + z;
	if (!isfinite(z))
		return z;
	/* handle +-0 */
	if (x == 0.0 || y == 0.0)
		return x*y + z;
	round = fegetround();
	if (z == 0.0) {
		if (round == FE_TONEAREST)
			return dmul(x, y);
		return x*y;
	}

	/* exact mul and add require nearest rounding */
	/* spurious inexact exceptions may be raised */
	fesetround(FE_TONEAREST);
	mul(&xy, &lo1, x, y);
	exy = getexp(xy);
	ez = getexp(z);
	if (ez > exy) {
		add(&hi, &lo2, z, xy);
	} else if (ez > exy - 12) {
		add(&hi, &lo2, xy, z);
		if (hi == 0) {
			fesetround(round);
			/* make sure that the sign of 0 is correct */
			return (xy + z) + lo1;
		}
	} else {
		/*
		ez <= exy - 12
		the 12 extra bits (1guard, 11round+sticky) are needed so with
			lo = dadd(lo1, lo2)
		elo <= ehi - 11, and we use the last 10 bits in adjust so
			dadd(hi, lo)
		gives correct result when rounded to double
		*/
		hi = xy;
		lo2 = z;
	}
	fesetround(round);
	if (round == FE_TONEAREST)
		return dadd(hi, dadd(lo1, lo2));
	return hi + (lo1 + lo2);
}

#endif
