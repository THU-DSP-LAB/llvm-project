@__clc_riscv32_work_group_scratch = hidden addrspace(3) global [1024 x i32] undef, align 4

define hidden ptr addrspace(3) @__clc_riscv32_get_work_group_scratch() {
  ret ptr addrspace(3) @__clc_riscv32_work_group_scratch
}
