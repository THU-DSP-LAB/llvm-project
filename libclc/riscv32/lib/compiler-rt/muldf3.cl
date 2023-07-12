//===-- lib/muldf3.c - Double-precision multiplication ------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------muldf3.cl----------------------------------===//
//
// Referenced from muldf3.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

static __inline fp_t __mulXf3__(fp_t a, fp_t b) {
  const unsigned int aExponent =
      toRep(a) >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  const unsigned int bExponent =
      toRep(b) >> 52 & ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1);
  const rep_t productSign =
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

      if (bAbs)
        return fromRep(aAbs | productSign);

      else
        return fromRep(((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                         ((1UL << 52) - 1U)) |
                        ((1UL << 52) >> 1)));
    }

    if (bAbs == (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U))) {

      if (aAbs)
        return fromRep(bAbs | productSign);

      else
        return fromRep(((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                         ((1UL << 52) - 1U)) |
                        ((1UL << 52) >> 1)));
    }

    if (!aAbs)
      return fromRep(productSign);

    if (!bAbs)
      return fromRep(productSign);

    if (aAbs < (1UL << 52))
      scale += normalize(&aSignificand);
    if (bAbs < (1UL << 52))
      scale += normalize(&bSignificand);
  }

  aSignificand |= (1UL << 52);
  bSignificand |= (1UL << 52);

  rep_t productHi, productLo;
  wideMultiply(aSignificand, bSignificand << ((sizeof(rep_t) * 8) - 52 - 1),
               &productHi, &productLo);

  int productExponent = aExponent + bExponent -
                        (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1) +
                        scale;

  if (productHi & (1UL << 52))
    productExponent++;
  else
    wideLeftShift(&productHi, &productLo, 1);

  if (productExponent >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1))
    return fromRep((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U)) |
                   productSign);

  if (productExponent <= 0) {

    const unsigned int shift = 1UL - (unsigned int)productExponent;
    if (shift >= (sizeof(rep_t) * 8))
      return fromRep(productSign);

    wideRightShiftWithSticky(&productHi, &productLo, shift);
  } else {

    productHi &= ((1UL << 52) - 1U);
    productHi |= (rep_t)productExponent << 52;
  }

  productHi |= productSign;

  if (productLo > (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))))
    productHi++;
  if (productLo == (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))))
    productHi += productHi & 1;
  return fromRep(productHi);
}

fp_t __muldf3(fp_t a, fp_t b) { return __mulXf3__(a, b); }

#endif
