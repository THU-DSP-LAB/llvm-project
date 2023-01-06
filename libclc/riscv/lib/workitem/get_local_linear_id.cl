#include <clc/clc.h>

_CLC_DEF _CLC_OVERFLOW size_t get_local_linear_id() {
  return __builtin_riscv_workitem_linear_id();
}
