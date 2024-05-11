#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"
#include <clc/clc.h>

__attribute__((overloadable, always_inline)) _CLC_DEF _CLC_OVERLOAD int work_group_all(int predicate)
{
    int n = get_local_size(0);
    int a = predicate;
    if (n == 1)
	    return a;

    __global int *p = (__global int *)__wg_scratch;;
    int gid = get_group_id(0);
    int i = get_local_id(0);
    if(i==0) {
      p[gid] = a;
    }
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    if(!a) p[gid]=a;
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    a = p[gid];
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    return a;
}

_CLC_DEF _CLC_OVERLOAD int work_group_any(int predicate)
{
    int n = get_local_size(0);
    int a = predicate;
    if (n == 1)
	    return a;

    __global int *p = __wg_scratch;
    int gid = get_group_id(0);
    int i = get_local_id(0);
    if(i==0) {
      p[gid] = a;
    }
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    if(a) p[gid]=a;
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    a = p[gid];
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);
    return a;
}

#endif
