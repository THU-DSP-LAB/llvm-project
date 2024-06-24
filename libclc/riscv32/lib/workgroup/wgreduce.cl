#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"
#include <clc/clc.h>

#define GEN_REDUCE(TYPE) \
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_reduce_add(TYPE a) \
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
\
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) { \
    TYPE res = 0; \
	for (int j = 0; j < n; j++) \
        res += p[j]; \
    p[0] = res; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[0]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_reduce_max(TYPE a) \
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
\
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) { \
    TYPE res = p[0]; \
	for (int j = 0; j < n; j++) \
        res = p[j] > res ? p[j] : res; \
    p[0] = res; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[0]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_reduce_min(TYPE a) \
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
\
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    if (i == 0) { \
    TYPE res = p[0]; \
	for (int j = 0; j < n; j++) \
        res = p[j] < res ? p[j] : res; \
    p[0] = res; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[0]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
}

GEN_REDUCE(int)
GEN_REDUCE(uint)
GEN_REDUCE(float)

#endif

