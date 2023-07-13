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

si_int __fixdfsi(fp_t a) { return __fixint(a); }

du_int __fixuint(fp_t a) {

  const rep_t aRep = toRep(a);
  const rep_t aAbs =
      aRep & ((1U << (23 + ((sizeof(rep_t) * 8) - 23 - 1))) - 1U);
  const int sign =
      aRep & (1U << (23 + ((sizeof(rep_t) * 8) - 23 - 1))) ? -1 : 1;
  const int exponent =
      (aAbs >> 23) - (((1 << ((sizeof(rep_t) * 8) - 23 - 1)) - 1) >> 1);
  const rep_t significand = (aAbs & ((1U << 23) - 1U)) | (1U << 23);

  if (sign == -1 || exponent < 0)
    return 0;

  if ((unsigned)exponent >= sizeof(fixuint_t) * 8)
    return ~(fixuint_t)0;

  if (exponent < 23)
    return significand >> (23 - exponent);
  else
    return (fixuint_t)significand << (exponent - 23);
}

du_int __fixunssfdi(fp_t a) { return __fixuint(a); }

 di_int __fixsfdi(fp_t a) { return __fixint(a); }
su_int __fixunsdfsi(fp_t a) { return __fixuint(a); }
du_int __fixunsdfdi(fp_t a) { return __fixuint(a); }

#endif
