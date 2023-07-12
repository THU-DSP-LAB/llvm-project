//===-- floatdisf.c - Implement __floatdisf -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from floatdisf.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

float __floatdisf(di_int a) {
  if (a == 0)
    return 0.0F;
  const unsigned N = sizeof(di_int) * 8;
  const di_int s = a >> (N - 1);
  a = (a ^ s) - s;
  int sd = N - __builtin_clzll(a);
  si_int e = sd - 1;
  if (sd > 24) {

    switch (sd) {
    case 24 + 1:
      a <<= 1;
      break;
    case 24 + 2:
      break;
    default:
      a = ((du_int)a >> (sd - (24 + 2))) |
          ((a & ((du_int)(-1) >> ((N + 24 + 2) - sd))) != 0);
    };

    a |= (a & 4) != 0;
    ++a;
    a >>= 2;

    if (a & ((du_int)1 << 24)) {
      a >>= 1;
      ++e;
    }

  } else {
    a <<= (24 - sd);
  }
  float_bits fb;
  fb.u =
      ((su_int)s & 0x80000000) | ((e + 127) << 23) | ((su_int)a & 0x007FFFFF);
  return fb.f;
}

#endif
