# Build crt0 and workitem builtin implementation

project(ventus-builtin VERSION 0.2.0 LANGUAGES C ASM)

set(CMAKE_ASM_COMPILER clang)
set(CMAKE_ASM_FLAGS ${CMAKE_LLAsm_FLAGS})

# workitem builtins
add_library(workitem STATIC workitem/workitem.S)
target_include_directories(workitem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


# crts
add_library(crt0 OBJECT crt0.S)
