#include <clc/clc.h>

_CLC_DEF _CLC_OVERLOAD size_t get_global_linear_id() {
  uint dim = get_work_dim() - 1;
  switch (dim) {
  case 0:
    return get_global_id(0) - get_global_offset(0);
    ;
  case 1:
    return (get_global_id(1) - get_global_offset(1)) * get_global_size(0) +
           (get_global_id(0) - get_global_offset(0));
  case 2:
    return ((get_global_id(2) - get_global_offset(2)) * get_global_size(1) +
            (get_global_id(1) - get_global_offset(1))) *
               get_global_size(0) +
           (get_global_id(0) - get_global_offset(0));
  default:
    return 0;
  }
}
