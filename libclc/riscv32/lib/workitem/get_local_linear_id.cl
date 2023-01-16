#include <clc/clc.h>

extern size_t __builtin_riscv_workitem_linear_id();

_CLC_DEF _CLC_OVERLOAD size_t get_local_linear_id() {
  return __builtin_riscv_workitem_linear_id();
}
