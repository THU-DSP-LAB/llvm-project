#include <clc/clc.h>

// FIXME: how to detect the __opencl_c_subgroups feature??
// Opencl forbids the default argument setting
#if defined(__opencl_c_subgroups)
// llvm.riscv.ventus.barrier.with.scope means the function arguments with memory_scope
_CLC_DEF _CLC_OVERLOAD void barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barriersub");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barriersub.with.scope");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags, unsigned scope) __asm("llvm.riscv.ventus.barriersub.with.scope");

#else
_CLC_DEF _CLC_OVERLOAD void barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barrier");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barrier");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags, unsigned memory_scope) __asm("llvm.riscv.ventus.barrier.with.scope");

#endif
