#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"
#include <clc/clc.h>

#define GEN_SCAN_INCLUSIVE(TYPE) \
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_inclusive_add(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] + p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[i]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_inclusive_max(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] > p[j] ? p[j-1] : p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[i]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_inclusive_min(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] < p[j] ? p[j-1] : p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[i]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
}

GEN_SCAN_INCLUSIVE(int)
GEN_SCAN_INCLUSIVE(uint)
GEN_SCAN_INCLUSIVE(float)

#define GEN_SCAN_EXCLUSIVE(TYPE, LB, UB) \
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_exclusive_add(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] + p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) \
        a = 0; \
    else \
        a = p[i-1]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_exclusive_max(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] > p[j] ? p[j-1] : p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) \
        a = LB; \
    else \
        a = p[i-1]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_scan_exclusive_min(TYPE a) \
{ \
    uint n = get_local_size(0); \
    if (n == 1) \
        return a; \
 \
    int gid = get_group_id(0); \
    int i = get_local_id(0); \
    __global TYPE *p = (__global TYPE *)&__wi_scratch[gid * MAX_THREAD_PER_WG]; \
\
    p[i] = a; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
\
    if (i == 0) { \
	for (int j = 1; j < n; j++) \
        p[j] = p[j-1] < p[j] ? p[j-1] : p[j]; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) \
        a = UB; \
    else \
        a = p[i-1]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
}

GEN_SCAN_EXCLUSIVE(int, INT_MIN, INT_MAX)
GEN_SCAN_EXCLUSIVE(uint, 0U, UINT_MAX)
GEN_SCAN_EXCLUSIVE(float, -INFINITY, INFINITY)

#endif

