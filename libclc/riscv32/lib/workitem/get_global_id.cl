#include <clc/clc.h>

extern size_t __builtin_riscv_global_id_x();
extern size_t __builtin_riscv_global_id_y();
extern size_t __builtin_riscv_global_id_z();

_CLC_DEF _CLC_OVERLOAD size_t get_global_id(uint dim) {
  switch (dim) {
  case 0:
    return __builtin_riscv_global_id_x();
  case 1:
    return __builtin_riscv_global_id_y();
  case 2:
    return __builtin_riscv_global_id_z();
  default:
    return 0;
  }
}
