//===-- lib/extendsfdf2.c - single -> double conversion -----------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from extendsfdf2.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

typedef float src_t;
typedef uint32_t src_rep_t;

static const int srcSigBits = 23;

typedef double dst_t;
typedef uint64_t dst_rep_t;

static const int dstSigBits = 52;

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

static __inline dst_t __extendXfYf2__(src_t a) {

  const int srcBits = sizeof(src_t) * 8;
  const int srcExpBits = srcBits - srcSigBits - 1;
  const int srcInfExp = (1 << srcExpBits) - 1;
  const int srcExpBias = srcInfExp >> 1;

  const src_rep_t srcMinNormal = 1U << srcSigBits;
  const src_rep_t srcInfinity = (src_rep_t)srcInfExp << srcSigBits;
  const src_rep_t srcSignMask = 1U << (srcSigBits + srcExpBits);
  const src_rep_t srcAbsMask = srcSignMask - 1;
  const src_rep_t srcQNaN = 1U << (srcSigBits - 1);
  const src_rep_t srcNaNCode = srcQNaN - 1;

  const int dstBits = sizeof(dst_t) * 8;
  const int dstExpBits = dstBits - dstSigBits - 1;
  const int dstInfExp = (1 << dstExpBits) - 1;
  const int dstExpBias = dstInfExp >> 1;

  const dst_rep_t dstMinNormal = 1UL << dstSigBits;

  const src_rep_t aRep = srcToRep(a);
  const src_rep_t aAbs = aRep & srcAbsMask;
  const src_rep_t sign = aRep & srcSignMask;
  dst_rep_t absResult;

  if ((src_rep_t)(aAbs - srcMinNormal) < srcInfinity - srcMinNormal) {

    absResult = (dst_rep_t)aAbs << (dstSigBits - srcSigBits);
    absResult += (dst_rep_t)(dstExpBias - srcExpBias) << dstSigBits;
  }

  else if (aAbs >= srcInfinity) {

    absResult = (dst_rep_t)dstInfExp << dstSigBits;
    absResult |= (dst_rep_t)(aAbs & srcQNaN) << (dstSigBits - srcSigBits);
    absResult |= (dst_rep_t)(aAbs & srcNaNCode) << (dstSigBits - srcSigBits);
  }

  else if (aAbs) {

    const int scale = __builtin_clz(aAbs) - __builtin_clz(srcMinNormal);
    absResult = (dst_rep_t)aAbs << (dstSigBits - srcSigBits + scale);
    absResult ^= dstMinNormal;
    const int resultExponent = dstExpBias - srcExpBias - scale + 1;
    absResult |= (dst_rep_t)resultExponent << dstSigBits;
  }

  else {

    absResult = 0;
  }

  const dst_rep_t result = absResult | (dst_rep_t)sign << (dstBits - srcBits);
  return dstFromRep(result);
}

double __extendsfdf2(float a) { return __extendXfYf2__(a); }


#endif
