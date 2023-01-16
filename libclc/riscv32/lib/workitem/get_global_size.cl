#include <clc/clc.h>

extern size_t __buitlin_riscv_global_size_x();
extern size_t __buitlin_riscv_global_size_y();
extern size_t __buitlin_riscv_global_size_z();

_CLC_DEF _CLC_OVERLOAD size_t get_global_size(uint dim) {
  switch (dim) {
  case 0:
    return __buitlin_riscv_global_size_x();
  case 1:
    return __buitlin_riscv_global_size_y();
  case 2:
    return __buitlin_riscv_global_size_z();
  default:
    return 1;
  }
}
