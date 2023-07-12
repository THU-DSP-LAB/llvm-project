//===-- lib/adddf3.c - Double-precision addition ------------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Referenced from subdf3.c, but implemented in OpenCL language
//
//===----------------------------------------------------------------------===//

#ifdef cl_khr_fp64

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include "types.h"

fp_t __subdf3(fp_t a, fp_t b) {
  return __adddf3(a, fromRep(toRep(b) ^ (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)))));
}

#endif
