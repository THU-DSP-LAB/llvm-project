# Build crt0 and workitem builtin implementation

project(ventus-builtin VERSION 0.2.0 LANGUAGES C ASM)

set(CMAKE_ASM_COMPILER clang)
set(CMAKE_ASM_FLAGS ${CMAKE_LLAsm_FLAGS})

# workitem builtins
llvm_add_library(workitem STATIC workitem/workitem.S)

# crts
set(LIBNAME "ctr0")
set(LIBNAME_OBJ "${LIBNAME}_obj")
add_library(${LIBNAME_OBJ} OBJECT crt0.S)
add_library(${LIBNAME} SHARED $<TARGET_OBJECTS:${LIBNAME_OBJ}>)

target_include_directories(workitem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Copy crt0 object library to lib folder to be automatically installed
add_custom_command(TARGET ${LIBNAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_OBJECTS:${LIBNAME_OBJ}>
    ${LLVM_LIBRARY_OUTPUT_INTDIR}/crt0.o
)

add_subdirectory(printf)