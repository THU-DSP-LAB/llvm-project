#include <clc/clc.h>

// get_global_size(unit dim) / get_num_groups(unit dim)

_CLC_DEF _CLC_OVERLOAD size_t get_enqueued_local_size(uint dim) {
  return get_local_size(dim);
}
