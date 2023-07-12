//===-- lib/comparedf2.c - Double-precision comparisons -----------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from comparedf2.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

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

enum {
  LE_LESS = -1,
  LE_EQUAL = 0,
  LE_GREATER = 1,
  LE_UNORDERED = 1,
};

static inline CMP_RESULT __leXf2__(fp_t a, fp_t b) {
  const srep_t aInt = toRep(a);
  const srep_t bInt = toRep(b);
  const rep_t aAbs =
      aInt & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const rep_t bAbs =
      bInt & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);

  if (aAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
              ((1UL << 52) - 1U)) ||
      bAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
              ((1UL << 52) - 1U)))
    return LE_UNORDERED;

  if ((aAbs | bAbs) == 0)
    return LE_EQUAL;

  if ((aInt & bInt) >= 0) {
    if (aInt < bInt)
      return LE_LESS;
    else if (aInt == bInt)
      return LE_EQUAL;
    else
      return LE_GREATER;
  } else {

    if (aInt > bInt)
      return LE_LESS;
    else if (aInt == bInt)
      return LE_EQUAL;
    else
      return LE_GREATER;
  }
}

enum { GE_LESS = -1, GE_EQUAL = 0, GE_GREATER = 1, GE_UNORDERED = -1 };

static inline CMP_RESULT __geXf2__(fp_t a, fp_t b) {
  const srep_t aInt = toRep(a);
  const srep_t bInt = toRep(b);
  const rep_t aAbs =
      aInt & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const rep_t bAbs =
      bInt & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);

  if (aAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
              ((1UL << 52) - 1U)) ||
      bAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
              ((1UL << 52) - 1U)))
    return GE_UNORDERED;
  if ((aAbs | bAbs) == 0)
    return GE_EQUAL;
  if ((aInt & bInt) >= 0) {
    if (aInt < bInt)
      return GE_LESS;
    else if (aInt == bInt)
      return GE_EQUAL;
    else
      return GE_GREATER;
  } else {
    if (aInt > bInt)
      return GE_LESS;
    else if (aInt == bInt)
      return GE_EQUAL;
    else
      return GE_GREATER;
  }
}

static inline CMP_RESULT __unordXf2__(fp_t a, fp_t b) {
  const rep_t aAbs =
      toRep(a) & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  const rep_t bAbs =
      toRep(b) & ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
  return aAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U)) ||
         bAbs > (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                 ((1UL << 52) - 1U));
}

CMP_RESULT __ledf2(fp_t a, fp_t b) { return __leXf2__(a, b); }
CMP_RESULT __ltdf2(fp_t a, fp_t b) { return __ledf2(a, b); }
CMP_RESULT __nedf2(fp_t a, fp_t b) { return __ledf2(a, b); }
CMP_RESULT __eqdf2(fp_t a, fp_t b) { return __ledf2(a, b); }
CMP_RESULT __gedf2(fp_t a, fp_t b) { return __geXf2__(a, b); }
CMP_RESULT __unorddf2(fp_t a, fp_t b) { return __unordXf2__(a, b); }

#endif
