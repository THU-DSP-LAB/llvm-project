#include <clc/clc.h>

_CLC_DEF _CLC_OVERLOAD void barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barrier");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags) __asm("llvm.riscv.ventus.barrier");
_CLC_DEF _CLC_OVERLOAD void work_group_barrier(cl_mem_fence_flags flags, unsigned memory_scope) __asm("llvm.riscv.ventus.barrier.with.scope");
