/**
 * Copyright (c) 2023 Terapines Technology (Wuhan) Co., Ltd
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
/**
 * This file defines the hardware CSR and kernel metadata buffer related const.
 *
 * kernel metadata buffer layout:
 * +-------4---------+----------4----------+-----4----+-------4-------+-------4-------
 * | kernel func ptr | kernel arg base ptr | work_dim | global_size_x | global_size_y
 * +-------4-------+------4-------+------4-------+------4-------+-------4---------
 * | global_size_z | local_size_x | local_size_y | local_size_z | global_offset_x
 * +--------4--------+--------4--------+------4-----+------4-----+
 * | global_offset_y | global_offset_z | print_addr | print_size |
 */

#ifndef __VENTUS_H__
#define __VENTUS_H__

// CSRs
#define CSR_TID   0x800    // smallest thread id in a warp
#define CSR_NUMW  0x801    // warp numbers in a workgroup
#define CSR_NUMT  0x802    // thread numbers in a warp
#define CSR_KNL   0x803    // Kernel metadata buffer base address
#define CSR_WGID  0x804    // workgroup id in a warp
#define CSR_WID   0x805    // warp id in a workgroup
#define CSR_LDS   0x806    // baseaddr for local memory allocated by workgroup
#define CSR_PDS   0x807    // baseaddr for private memory allocated by workgroup
#define CSR_GID_X 0x808    // group_id_x
#define CSR_GID_Y 0x809    // group_id_y
#define CSR_GID_Z 0x80a    // group_id_z
#define CSR_PRINT 0x80b    // for print buffer

// Kernel metadata buffer offsets
#define KNL_ENTRY 0
#define KNL_ARG_BASE 4
#define KNL_WORK_DIM 8
#define KNL_GL_SIZE_X 12
#define KNL_GL_SIZE_Y 16
#define KNL_GL_SIZE_Z 20
#define KNL_LC_SIZE_X 24
#define KNL_LC_SIZE_Y 28
#define KNL_LC_SIZE_Z 32
#define KNL_GL_OFFSET_X 36
#define KNL_GL_OFFSET_Y 40
#define KNL_GL_OFFSET_Z 44
#define KNL_PRINT_ADDR 48
#define KNL_PRINT_SIZE 52

#endif // __VENTUS_H__
