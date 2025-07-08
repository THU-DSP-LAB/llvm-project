/**
 * Address Space Qualifier Runtime Functions for OpenCL C
 * These implement the runtime functions called by the compiler
 * when to_global(), to_local(), to_private() builtins are used
 */

#include <clc/clctypes.h>
#include <clc/clcfunc.h>
#include <clc/synchronization/cl_mem_fence_flags.h>

// Declare assembly helper functions
extern int __is_global_ptr(const void *ptr);
extern int __is_local_ptr(const void *ptr);
extern int __is_private_ptr(const void *ptr);

// Runtime function implementations that the compiler calls
global void *__to_global(generic void *ptr) {
    return __is_global_ptr(ptr) ? (global void *)ptr : (global void *)0;
}

local void *__to_local(generic void *ptr) {
    return __is_local_ptr(ptr) ? (local void *)ptr : (local void *)0;
}

private void *__to_private(generic void *ptr) {
    return __is_private_ptr(ptr) ? (private void *)ptr : (private void *)0;
}

// get_fence implementation - returns appropriate memory fence flags based on address space
_CLC_OVERLOAD cl_mem_fence_flags get_fence(generic void *ptr) {
    if (__is_local_ptr(ptr)) {
        return CLK_LOCAL_MEM_FENCE;
    } else if (__is_global_ptr(ptr)) {
        return CLK_GLOBAL_MEM_FENCE;
    } else {
        // For private memory or unknown address space, return 0
        // Private memory doesn't typically need memory fences
        return 0;
    }
}
