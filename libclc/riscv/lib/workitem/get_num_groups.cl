#include <clc/clc.h>

_CLC_DEF _CLC_OVERLOAD size_t get_num_groups(uint dim) {
  switch (dim) {
  case 0:
    return __builtin_riscv_num_groups_x();
  case 1:
    return __builtin_riscv_num_groups_y();
  case 2:
    return __builtin_riscv_num_groups_z();
  default:
    return 1;
  }
}
