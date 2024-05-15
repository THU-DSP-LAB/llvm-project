#if __OPENCL_C_VERSION__ >= 200

#include "wg.h"
#include <clc/clc.h>

#define GEN_BROADCAST(TYPE) \
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_broadcast(TYPE a, uint local_id_x) \
{ \
    __global TYPE *p = (__global TYPE *)__wg_scratch; \
    int gid = get_group_id(0); \
    if (get_local_id(0) == local_id_x) { \
      p[gid] = a; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[gid]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
\
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_broadcast(TYPE a, size_t local_id_x, size_t local_id_y) \
{ \
    __global TYPE *p = (__global TYPE *)__wg_scratch; \
    int gid = get_group_id(0); \
    if (get_local_id(0) == local_id_x && get_local_id(1) == local_id_y) { \
      p[gid] = a; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[gid]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
} \
 \
__attribute__((overloadable,weak,always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE \
work_group_broadcast(TYPE a, size_t local_id_x,size_t local_id_y, \
                     size_t local_id_z) \
{ \
    __global TYPE *p = (__global TYPE *)__wg_scratch; \
    int gid = get_group_id(0); \
    if (get_local_id(0) == local_id_x && \
        get_local_id(1) == local_id_y && \
        get_local_id(2) == local_id_z) { \
      p[gid] = a; \
    } \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    a = p[gid]; \
    work_group_barrier(CLK_GLOBAL_MEM_FENCE); \
    return a; \
}

GEN_BROADCAST(uint)
GEN_BROADCAST(int)
GEN_BROADCAST(float)

#endif

