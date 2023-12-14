//===------------------------ VentusProgramInfo.h - -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Define a strcut to collect the resource usage information of Ventus
//
//===----------------------------------------------------------------------===//

#ifndef VENTUS_PROGRAM_INFO_H
#define VENTUS_PROGRAM_INFO_H

#include <cstdint>
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {
  struct SubVentusProgramInfo {

    uint32_t VGPRUsage = 0; // The number of VGPRS which has been used
    uint32_t SGPRUsage = 0; // The number of SGPRS which has been used
    uint32_t LDSMemory = 0; // Used local memory size
    uint32_t PDSMemory = 0; // Used private memory size

    SubVentusProgramInfo() = default;

  };

  struct VentusProgramInfo {

    uint32_t VGPRUsage = 0; // The number of VGPRS which has been used
    uint32_t SGPRUsage = 0; // The number of SGPRS which has been used
    uint32_t LDSMemory = 0; // Used local memory size
    uint32_t PDSMemory = 0; // Used private memory size

    // Record the registers that have been added to prevent repeated additions.
    SmallVector<DenseSet<unsigned>> RegisterAddedSetVec;

    // Record the resource usage of each function.
    SmallVector<SubVentusProgramInfo> SubProgramInfoVec;

    VentusProgramInfo() = default;

  };

} // namespace llvm

#endif // VENTUS_PROGRAM_INFO_H
