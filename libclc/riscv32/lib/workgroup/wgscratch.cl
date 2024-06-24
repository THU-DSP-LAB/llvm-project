#include "wg.h"

__global int __wg_scratch[MAX_WORKGROUP];
__global int __wi_scratch[MAX_THREAD_PER_WG * MAX_WORKGROUP];
