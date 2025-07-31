/**
 * Address Space Detection Functions for RISC-V Ventus GPU
 * These functions only check address ranges and return 1/0
 */

#include "ventus.h"

// __is_global_ptr: Check if pointer is in global address space
  .section .text.__is_global_ptr,"ax",@progbits
  .global __is_global_ptr
  .type __is_global_ptr, @function
__is_global_ptr:
  # Input: pointer in a0, Output: 1 if global, 0 if not
  
  # Check if in local range
  csrr t0, CSR_LDS         # Local memory base
  csrr t1, CSR_NUMW        # Number of warps
  li t2, 1024
  mul t1, t1, t2           # Local memory size
  add t1, t0, t1           # Local memory end
  
  bltu a0, t0, check_private
  bgeu a0, t1, check_private
  li a0, 0                 # In local, not global
  ret
  
check_private:
  # Check if in private range
  csrr t0, CSR_PDS         # Private memory base  
  csrr t1, CSR_NUMT
  csrr t2, CSR_NUMW
  mul t1, t1, t2
  li t2, 1024
  mul t1, t1, t2           # Private memory size
  add t1, t0, t1           # Private memory end
  
  bltu a0, t0, is_global
  bgeu a0, t1, is_global
  li a0, 0                 # In private, not global
  ret
  
is_global:
  li a0, 1                 # Is global
  ret

// __is_local_ptr: Check if pointer is in local address space
  .section .text.__is_local_ptr,"ax",@progbits
  .global __is_local_ptr
  .type __is_local_ptr, @function
__is_local_ptr:
  # Input: pointer in a0, Output: 1 if local, 0 if not
  csrr t0, CSR_LDS
  csrr t1, CSR_NUMW
  li t2, 1024
  mul t1, t1, t2
  add t1, t0, t1
  
  bltu a0, t0, not_local
  bgeu a0, t1, not_local
  li a0, 1                 # Is local
  ret
  
not_local:
  li a0, 0                 # Not local
  ret

// __is_private_ptr: Check if pointer is in private address space
  .section .text.__is_private_ptr,"ax",@progbits
  .global __is_private_ptr
  .type __is_private_ptr, @function
__is_private_ptr:
  # Input: pointer in a0, Output: 1 if private, 0 if not
  csrr t0, CSR_PDS
  csrr t1, CSR_NUMT
  csrr t2, CSR_NUMW
  mul t1, t1, t2
  li t2, 1024
  mul t1, t1, t2
  add t1, t0, t1
  
  bltu a0, t0, not_private
  bgeu a0, t1, not_private
  li a0, 1                 # Is private
  ret
  
not_private:
  li a0, 0                 # Not private
  ret 
