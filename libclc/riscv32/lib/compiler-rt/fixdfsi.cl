//===-- fixdfsi.c - Implement __fixdfsi -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from fixdfsi.c, but implemented in OpenCL language
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

#endif
