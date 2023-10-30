//===-- floatundisf.cl - Implement __floatundisf --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from muldf3.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

// Returns: convert a to a float, rounding toward even.

// Assumption: float is a IEEE 32 bit floating point type
//            long is a 64 bit integral type

// seee eeee emmm mmmm mmmm mmmm mmmm mmmm

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

float __floatundisf(long a) {
  if (a == 0)
    return 0.0F;
  const unsigned N = sizeof(long) * 8;
  int sd = N - clz64(a); // number of significant digits
  int e = sd - 1;               // 8 exponent
  if (sd > FLT_MANT_DIG) {
    //  start:  0000000000000000000001xxxxxxxxxxxxxxxxxxxxxxPQxxxxxxxxxxxxxxxxxx
    //  finish: 000000000000000000000000000000000000001xxxxxxxxxxxxxxxxxxxxxxPQR
    //                                                12345678901234567890123456
    //  1 = msb 1 bit
    //  P = bit FLT_MANT_DIG-1 bits to the right of 1
    //  Q = bit FLT_MANT_DIG bits to the right of 1
    //  R = "or" of all bits to the right of Q
    switch (sd) {
    case FLT_MANT_DIG + 1:
      a <<= 1;
      break;
    case FLT_MANT_DIG + 2:
      break;
    default:
      a = (a >> (sd - (FLT_MANT_DIG + 2))) |
          ((a & ((long)(-1) >> ((N + FLT_MANT_DIG+ 2) - sd))) != 0);
    };
    // finish:
    a |= (a & 4) != 0; // Or P into R
    ++a;               // round - this step may add a significant bit
    a >>= 2;           // dump Q and R
    // a is now rounded to FLT_MANT_DIG or FLT_MANT_DIG+1 bits
    if (a & ((long)1 << FLT_MANT_DIG)) {
      a >>= 1;
      ++e;
    }
    // a is now rounded to FLT_MANT_DIG bits
  } else {
    a <<= (FLT_MANT_DIG - sd);
    // a is now rounded to FLT_MANT_DIG bits
  }
  float_bits fb;
  fb.u = ((e + 127) << 23) |       // exponent
         ((unsigned)a & 0x007FFFFF); // mantissa
  return fb.f;
}

#endif
