//===-- lib/divdf3.c - Double-precision division ------------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from divdf3.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

static __inline fp_t __divXf3__(fp_t a, fp_t b) {

  const unsigned int aExponent =
      toRep(a) >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  const unsigned int bExponent =
      toRep(b) >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  const rep_t quotientSign =
      (toRep(a) ^ toRep(b)) & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)));

  rep_t aSignificand = toRep(a) & ((1UL << 52) - 1U);
  rep_t bSignificand = toRep(b) & ((1UL << 52) - 1U);
  int scale = 0;

  if (aExponent - 1U >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) - 1U ||
      bExponent - 1U >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) - 1U) {

    const rep_t aAbs =
        toRep(a) & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
    const rep_t bAbs =
        toRep(b) & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);

    if (aAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                ((1UL << 52) - 1U)))
      return fromRep(toRep(a) | ((1UL << 52) >> 1));

    if (bAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                ((1UL << 52) - 1U)))
      return fromRep(toRep(b) | ((1UL << 52) >> 1));

    if (aAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U))) {

      if (bAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                   ((1UL << 52) - 1U)))
        return fromRep(((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                         ((1UL << 52) - 1U)) |
                        ((1UL << 52) >> 1)));

      else
        return fromRep(aAbs | quotientSign);
    }

    if (bAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U)))
      return fromRep(quotientSign);

    if (!aAbs) {

      if (!bAbs)
        return fromRep(((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                         ((1UL << 52) - 1U)) |
                        ((1UL << 52) >> 1)));

      else
        return fromRep(quotientSign);
    }

    if (!bAbs)
      return fromRep((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                      ((1UL << 52) - 1U)) |
                     quotientSign);

    if (aAbs < (1UL << 52))
      scale += normalize(&aSignificand);
    if (bAbs < (1UL << 52))
      scale -= normalize(&bSignificand);
  }

  aSignificand |= (1UL << 52);
  bSignificand |= (1UL << 52);

  int writtenExponent = (aExponent - bExponent + scale) +
                        (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1);

  const rep_t b_UQ1 = bSignificand << ((sizeof(rep_t) * 8) - 52 - 1);

  const half_rep_t b_UQ1_hw =
      bSignificand >> (52 + 1 - ((sizeof(rep_t) * 8) / 2));

  const half_rep_t C_hw = 0x7504F333U << (((sizeof(rep_t) * 8) / 2) - 32);

  half_rep_t x_UQ0_hw = C_hw - (b_UQ1_hw);

  {
    half_rep_t corr_UQ1_hw =
        0 - ((rep_t)x_UQ0_hw * b_UQ1_hw >> ((sizeof(rep_t) * 8) / 2));
    x_UQ0_hw = (rep_t)x_UQ0_hw * corr_UQ1_hw >> (((sizeof(rep_t) * 8) / 2) - 1);
  }
  {
    half_rep_t corr_UQ1_hw =
        0 - ((rep_t)x_UQ0_hw * b_UQ1_hw >> ((sizeof(rep_t) * 8) / 2));
    x_UQ0_hw = (rep_t)x_UQ0_hw * corr_UQ1_hw >> (((sizeof(rep_t) * 8) / 2) - 1);
  }
  {
    half_rep_t corr_UQ1_hw =
        0 - ((rep_t)x_UQ0_hw * b_UQ1_hw >> ((sizeof(rep_t) * 8) / 2));
    x_UQ0_hw = (rep_t)x_UQ0_hw * corr_UQ1_hw >> (((sizeof(rep_t) * 8) / 2) - 1);
  }

  x_UQ0_hw -= 1U;
  rep_t x_UQ0 = (rep_t)x_UQ0_hw << ((sizeof(rep_t) * 8) / 2);
  x_UQ0 -= 1U;

  rep_t blo = b_UQ1 & (-1UL >> ((sizeof(rep_t) * 8) / 2));

  rep_t corr_UQ1 =
      0U - ((rep_t)x_UQ0_hw * b_UQ1_hw +
            ((rep_t)x_UQ0_hw * blo >> ((sizeof(rep_t) * 8) / 2)) - 1UL);
  rep_t lo_corr = corr_UQ1 & (-1UL >> ((sizeof(rep_t) * 8) / 2));
  rep_t hi_corr = corr_UQ1 >> ((sizeof(rep_t) * 8) / 2);

  x_UQ0 = ((rep_t)x_UQ0_hw * hi_corr << 1) +
          ((rep_t)x_UQ0_hw * lo_corr >> (((sizeof(rep_t) * 8) / 2) - 1)) - 2UL;

  x_UQ0 -= 1U;

  x_UQ0 -= 2U;

  x_UQ0 -= 220UL;

  rep_t quotient_UQ1, dummy;
  wideMultiply(x_UQ0, aSignificand << 1, &quotient_UQ1, &dummy);

  rep_t residualLo;
  if (quotient_UQ1 < ((1UL << 52) << 1)) {

    residualLo = (aSignificand << (52 + 1)) - quotient_UQ1 * bSignificand;
    writtenExponent -= 1;
    aSignificand <<= 1;
  } else {

    quotient_UQ1 >>= 1;
    residualLo = (aSignificand << 52) - quotient_UQ1 * bSignificand;
  }

  if (writtenExponent >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1))
    return fromRep((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U)) |
                   quotientSign);

  rep_t absResult;
  if (writtenExponent > 0) {

    absResult = quotient_UQ1 & ((1UL << 52) - 1U);

    absResult |= (rep_t)writtenExponent << 52;
    residualLo <<= 1;
  } else {

    if (52 + writtenExponent < 0)
      return fromRep(quotientSign);

    absResult = quotient_UQ1 >> (-writtenExponent + 1);

    residualLo = (aSignificand << (52 + writtenExponent)) -
                 (absResult * bSignificand << 1);
  }

  residualLo += absResult & 1;

  absResult += residualLo > bSignificand;

  return fromRep(absResult | quotientSign);
}

fp_t __divdf3(fp_t a, fp_t b) { return __divXf3__(a, b); }

#endif
