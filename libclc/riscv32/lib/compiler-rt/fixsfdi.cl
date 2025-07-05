//===-- fixsfdi.c - Implement __fixsfdi -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

typedef si_int fixint_t;
typedef su_int fixuint_t;

static __inline fixint_t __fixint(fp_t a) {
  const fixint_t fixint_max = (fixint_t)((~(fixuint_t)0) / 2);
  const fixint_t fixint_min = -fixint_max - 1;

  const rep_t aRep = toRep(a);
  const rep_t aAbs =
      aRep & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const fixint_t sign =
      aRep & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) ? -1 : 1;
  const int exponent =
      (aAbs >> 52) - (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1);
  const rep_t significand = (aAbs & ((1UL << 52) - 1U)) | (1UL << 52);

  if (exponent < 0)
    return 0;

  if ((unsigned)exponent >= sizeof(fixint_t) * 8)
    return sign == 1 ? fixint_max : fixint_min;

  if (exponent < 52)
    return sign * (significand >> (52 - exponent));
  else
    return sign * ((fixint_t)significand << (exponent - 52));
}

static __inline du_int __fixuint_double(fp_t a) {
  const rep_t aRep = toRep(a);
  const rep_t aAbs =
      aRep & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const int sign =
      aRep & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) ? -1 : 1;
  const int exponent =
      (aAbs >> 52) - (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1);
  const rep_t significand = (aAbs & ((1UL << 52) - 1U)) | (1UL << 52);

  if (sign == -1 || exponent < 0)
    return 0;

  if ((unsigned)exponent >= sizeof(fixuint_t) * 8)
    return ~(fixuint_t)0;

  if (exponent < 52)
    return significand >> (52 - exponent);
  else
    return (fixuint_t)significand << (exponent - 52);
}

static __inline du_int __fixuint_single(float a) {
  union { float f; uint32_t i; } u = { .f = a };
  const uint32_t aRep = u.i;
  
  const uint32_t aAbs = aRep & 0x7FFFFFFF;
  const int sign = (aRep & 0x80000000) ? -1 : 1; 
  const int exponent = (aAbs >> 23) - 127; 
  const uint32_t significand = (aAbs & 0x007FFFFF) | 0x00800000; 

  if (sign == -1 || exponent < 0)
    return 0;

  if ((unsigned)exponent >= sizeof(fixuint_t) * 8)
    return ~(fixuint_t)0;

  if (exponent < 23)
    return significand >> (23 - exponent);
  else
    return (fixuint_t)significand << (exponent - 23);
}

static __inline di_int __fixint_single(float a) {
  const di_int fixint_max = (di_int)((~(du_int)0) / 2);
  const di_int fixint_min = -fixint_max - 1;

  union { float f; uint32_t i; } u = { .f = a };
  const uint32_t aRep = u.i;
  
  const uint32_t aAbs = aRep & 0x7FFFFFFF;
  const int sign = (aRep & 0x80000000) ? -1 : 1;
  const int exponent = (aAbs >> 23) - 127;
  const uint32_t significand = (aAbs & 0x007FFFFF) | 0x00800000;

  if (exponent < 0)
    return 0;

  if ((unsigned)exponent >= sizeof(di_int) * 8)
    return sign == 1 ? fixint_max : fixint_min;

  if (exponent < 23)
    return sign * (significand >> (23 - exponent));
  else
    return sign * ((di_int)significand << (exponent - 23));
}

du_int __fixunssfdi(float a) { return __fixuint_single(a); } 
di_int __fixsfdi(float a) { return __fixint_single(a); }     
su_int __fixunsdfsi(fp_t a) { return (su_int)__fixuint_double(a); }
du_int __fixunsdfdi(fp_t a) { return __fixuint_double(a); }

#endif
