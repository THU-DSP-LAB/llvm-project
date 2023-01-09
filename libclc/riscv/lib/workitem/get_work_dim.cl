#include <clc/clc.h>

_CLC_DEF _CLC_OVERLOAD uint get_work_dim(void) {
  return __builtin_riscv_work_dim();
}
