#include <clc/clc.h>

_CLC_DEF _CLC_OVERLOAD size_t get_global_offset(uint dim) {
  switch (dim) {
  case 0:
    return __builtin_riscv_global_offset_x();
  case 1:
    return __builtin_riscv_global_offset_y();
  case 2:
    return __builtin_riscv_global_offset_z();
  default:
    return 0;
  }
}
