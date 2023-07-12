//===-- lib/adddf3.c - Double-precision addition ------------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from adddf3.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

static __inline fp_t __addXf3__(fp_t a, fp_t b) {
  rep_t aRep = toRep(a);
  rep_t bRep = toRep(b);
  const rep_t aAbs =
      aRep & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const rep_t bAbs =
      bRep & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);

  if (aAbs - 1UL >= (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                     ((1UL << 52) - 1U)) -
                        1UL ||
      bAbs - 1UL >= (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                     ((1UL << 52) - 1U)) -
                        1UL) {

    if (aAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                ((1UL << 52) - 1U)))
      return fromRep(toRep(a) | ((1UL << 52) >> 1));

    if (bAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                ((1UL << 52) - 1U)))
      return fromRep(toRep(b) | ((1UL << 52) >> 1));

    if (aAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U))) {

      if ((toRep(a) ^ toRep(b)) ==
          (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))))
        return fromRep(((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                         ((1UL << 52) - 1U)) |
                        ((1UL << 52) >> 1)));

      else
        return a;
    }

    if (bAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U)))
      return b;

    if (!aAbs) {

      if (!bAbs)
        return fromRep(toRep(a) & toRep(b));
      else
        return b;
    }

    if (!bAbs)
      return a;
  }

  if (bAbs > aAbs) {
    const rep_t temp = aRep;
    aRep = bRep;
    bRep = temp;
  }

  int aExponent = aRep >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  int bExponent = bRep >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  rep_t aSignificand = aRep & ((1UL << 52) - 1U);
  rep_t bSignificand = bRep & ((1UL << 52) - 1U);

  if (aExponent == 0)
    aExponent = normalize(&aSignificand);
  if (bExponent == 0)
    bExponent = normalize(&bSignificand);

  const rep_t resultSign =
      aRep & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)));
  const _Bool subtraction =
      (aRep ^ bRep) & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)));

  aSignificand = (aSignificand | (1UL << 52)) << 3;
  bSignificand = (bSignificand | (1UL << 52)) << 3;

  const unsigned int align = aExponent - bExponent;
  if (align) {
    if (align < (sizeof(rep_t) * 8)) {
      const _Bool sticky = (bSignificand << ((sizeof(rep_t) * 8) - align)) != 0;
      bSignificand = bSignificand >> align | sticky;
    } else {
      bSignificand = 1;
    }
  }
  if (subtraction) {
    aSignificand -= bSignificand;

    if (aSignificand == 0)
      return fromRep(0);

    if (aSignificand < (1UL << 52) << 3) {
      const int shift = rep_clz(aSignificand) - rep_clz((1UL << 52) << 3);
      aSignificand <<= shift;
      aExponent -= shift;
    }
  } else {
    aSignificand += bSignificand;

    if (aSignificand & (1UL << 52) << 4) {
      const _Bool sticky = aSignificand & 1;
      aSignificand = aSignificand >> 1 | sticky;
      aExponent += 1;
    }
  }

  if (aExponent >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1))
    return fromRep((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U)) |
                   resultSign);

  if (aExponent <= 0) {

    const int shift = 1 - aExponent;
    const _Bool sticky = (aSignificand << ((sizeof(rep_t) * 8) - shift)) != 0;
    aSignificand = aSignificand >> shift | sticky;
    aExponent = 0;
  }

  const int roundGuardSticky = aSignificand & 0x7;

  rep_t result = aSignificand >> 3 & ((1UL << 52) - 1U);

  result |= (rep_t)aExponent << 52;
  result |= resultSign;

  switch (__fe_getround()) {
  case CRT_FE_TONEAREST:
    if (roundGuardSticky > 0x4)
      result++;
    if (roundGuardSticky == 0x4)
      result += result & 1;
    break;
  case CRT_FE_DOWNWARD:
    if (resultSign && roundGuardSticky)
      result++;
    break;
  case CRT_FE_UPWARD:
    if (!resultSign && roundGuardSticky)
      result++;
    break;
  case CRT_FE_TOWARDZERO:
    break;
  }
  if (roundGuardSticky)
    __fe_raise_inexact();
  return fromRep(result);
}

double __adddf3(double a, double b) { return __addXf3__(a, b); }

#endif
