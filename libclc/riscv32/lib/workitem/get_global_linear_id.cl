#include <clc/clc.h>

extern size_t __builtin_riscv_global_linear_id();

_CLC_DEF _CLC_OVERLOAD size_t get_global_linear_id() {
  return __builtin_riscv_global_linear_id();
}
