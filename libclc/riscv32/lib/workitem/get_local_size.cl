#include <clc/clc.h>

extern size_t __builtin_riscv_local_size_x();
extern size_t __builtin_riscv_local_size_y();
extern size_t __builtin_riscv_local_size_z();

_CLC_DEF _CLC_OVERLOAD size_t get_local_size(uint dim) {
  switch (dim) {
  case 0:
    return __builtin_riscv_local_size_x();
  case 1:
    return __builtin_riscv_local_size_y();
  case 2:
    return __builtin_riscv_local_size_z();
  default:
    return 1;
  }
}

#if 0
_CLC_DEF _CLC_OVERLOAD size_t get_enqueued_local_size(uint dim) {
  assert(0 && "TODO: Support non-uniform work-group.");
  return 1;
}
#endif