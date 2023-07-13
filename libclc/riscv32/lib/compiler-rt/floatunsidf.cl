//===-- lib/floatunsidf.c - uint -> double-precision conversion ---*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from floatunsidf.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

fp_t __floatunsidf(su_int a) {

  const int aWidth = sizeof a * 8;

  if (a == 0)
    return fromRep(0);

  const int exponent = (aWidth - 1) - __builtin_clz(a);
  rep_t result;

  const int shift = 52 - exponent;
  result = (rep_t)a << shift ^ (1UL << 52);

  result +=
      (rep_t)(exponent + (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1))
      << 52;
  return fromRep(result);
}

#endif
