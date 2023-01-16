#include <clc/clc.h>

extern size_t __builtin_riscv_work_dim();

_CLC_DEF _CLC_OVERLOAD uint get_work_dim(void) {
  return __builtin_riscv_work_dim();
}
