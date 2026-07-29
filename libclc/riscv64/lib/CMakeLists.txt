# RV64 startup stays separate so RV32 and RV64 may coexist in one build.
project(ventus-riscv64-builtin VERSION 0.2.0 LANGUAGES ASM)

set(CMAKE_ASM_COMPILER clang)
set(CMAKE_ASM_FLAGS ${CMAKE_LLAsm_FLAGS})

add_library(ctr0_rv64_obj OBJECT crt0.S)
add_library(ctr0_rv64 SHARED $<TARGET_OBJECTS:ctr0_rv64_obj>)

add_custom_command(TARGET ctr0_rv64
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${LLVM_LIBRARY_OUTPUT_INTDIR}/riscv64
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_OBJECTS:ctr0_rv64_obj>
    ${LLVM_LIBRARY_OUTPUT_INTDIR}/riscv64/crt0.o
)
