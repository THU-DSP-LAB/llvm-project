//===-- floatdidf.c - Implement __floatdidf -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from floatdidf.c , but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

double __floatdidf(di_int a) {
  if (a == 0)
    return 0.0;
  const unsigned N = sizeof(di_int) * 8;
  const di_int s = a >> (N - 1);
  a = (a ^ s) - s;
  int sd = N - clz64(a);
  int e = sd - 1;
  if (sd > DBL_MANT_DIG) {

    switch (sd) {
    case DBL_MANT_DIG + 1:
      a <<= 1;
      break;
    case DBL_MANT_DIG + 2:
      break;
    default:
      a = ((du_int)a >> (sd - (DBL_MANT_DIG + 2))) |
          ((a & ((du_int)(-1) >> ((N + DBL_MANT_DIG + 2) - sd))) != 0);
    };

    a |= (a & 4) != 0;
    ++a;
    a >>= 2;

    if (a & ((du_int)1 << DBL_MANT_DIG)) {
      a >>= 1;
      ++e;
    }

  } else {
    a <<= (DBL_MANT_DIG - sd);
  }
  double_bits fb;
  fb.u.s.high = ((su_int)s & 0x80000000) | ((su_int)(e + 1023) << 20) |
                ((su_int)(a >> 32) & 0x000FFFFF);
  fb.u.s.low = (su_int)a;
  return fb.f;
}

#endif
