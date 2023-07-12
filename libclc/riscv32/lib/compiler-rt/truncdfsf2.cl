//===-- lib/truncdfsf2.c - double -> single conversion ------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from truncdfsf2.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

typedef double src_t;
typedef uint64_t src_rep_t;

static const int srcSigBits = 52;

typedef float dst_t;
typedef uint32_t dst_rep_t;

static const int dstSigBits = 23;

static __inline src_rep_t srcToRep(src_t x) {
  const union {
    src_t f;
    src_rep_t i;
  } rep = {.f = x};
  return rep.i;
}

static __inline dst_t dstFromRep(dst_rep_t x) {
  const union {
    dst_t f;
    dst_rep_t i;
  } rep = {.i = x};
  return rep.f;
}

static __inline dst_t __truncXfYf2__(src_t a) {

  const int srcBits = sizeof(src_t) * 8;
  const int srcExpBits = srcBits - srcSigBits - 1;
  const int srcInfExp = (1 << srcExpBits) - 1;
  const int srcExpBias = srcInfExp >> 1;

  const src_rep_t srcMinNormal = 1UL << srcSigBits;
  const src_rep_t srcSignificandMask = srcMinNormal - 1;
  const src_rep_t srcInfinity = (src_rep_t)srcInfExp << srcSigBits;
  const src_rep_t srcSignMask = 1UL << (srcSigBits + srcExpBits);
  const src_rep_t srcAbsMask = srcSignMask - 1;
  const src_rep_t roundMask = (1UL << (srcSigBits - dstSigBits)) - 1;
  const src_rep_t halfway = 1UL << (srcSigBits - dstSigBits - 1);
  const src_rep_t srcQNaN = 1UL << (srcSigBits - 1);
  const src_rep_t srcNaNCode = srcQNaN - 1;

  const int dstBits = sizeof(dst_t) * 8;
  const int dstExpBits = dstBits - dstSigBits - 1;
  const int dstInfExp = (1 << dstExpBits) - 1;
  const int dstExpBias = dstInfExp >> 1;

  const int underflowExponent = srcExpBias + 1 - dstExpBias;
  const int overflowExponent = srcExpBias + dstInfExp - dstExpBias;
  const src_rep_t underflow = (src_rep_t)underflowExponent << srcSigBits;
  const src_rep_t overflow = (src_rep_t)overflowExponent << srcSigBits;

  const dst_rep_t dstQNaN = 1U << (dstSigBits - 1);
  const dst_rep_t dstNaNCode = dstQNaN - 1;

  const src_rep_t aRep = srcToRep(a);
  const src_rep_t aAbs = aRep & srcAbsMask;
  const src_rep_t sign = aRep & srcSignMask;
  dst_rep_t absResult;

  if (aAbs - underflow < aAbs - overflow) {

    absResult = aAbs >> (srcSigBits - dstSigBits);
    absResult -= (dst_rep_t)(srcExpBias - dstExpBias) << dstSigBits;

    const src_rep_t roundBits = aAbs & roundMask;

    if (roundBits > halfway)
      absResult++;

    else if (roundBits == halfway)
      absResult += absResult & 1;
  } else if (aAbs > srcInfinity) {

    absResult = (dst_rep_t)dstInfExp << dstSigBits;
    absResult |= dstQNaN;
    absResult |=
        ((aAbs & srcNaNCode) >> (srcSigBits - dstSigBits)) & dstNaNCode;
  } else if (aAbs >= overflow) {

    absResult = (dst_rep_t)dstInfExp << dstSigBits;
  } else {

    const int aExp = aAbs >> srcSigBits;
    const int shift = srcExpBias - dstExpBias - aExp + 1;

    const src_rep_t significand = (aRep & srcSignificandMask) | srcMinNormal;

    if (shift > srcSigBits) {
      absResult = 0;
    } else {
      const _Bool sticky = (significand << (srcBits - shift)) != 0;
      src_rep_t denormalizedSignificand = significand >> shift | sticky;
      absResult = denormalizedSignificand >> (srcSigBits - dstSigBits);
      const src_rep_t roundBits = denormalizedSignificand & roundMask;

      if (roundBits > halfway)
        absResult++;

      else if (roundBits == halfway)
        absResult += absResult & 1;
    }
  }

  const dst_rep_t result = absResult | sign >> (srcBits - dstBits);
  return dstFromRep(result);
}

float __truncdfsf2(double a) { return __truncXfYf2__(a); }

#endif
