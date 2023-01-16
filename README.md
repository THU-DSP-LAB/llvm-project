# This is the Ventus GPGPU port of LLVM Compiler Infrastructure

Ventus GPGPU is based on RISCV RV32IMAZfinxZve32f ISA with fully redefined concept of V-extension.

For more architecture detail, please refer to
[Ventus GPGPU Arch](https://github.com/THU-DSP-LAB/ventus-gpgpu/blob/master/docs/Ventus-GPGPU-doc.md)

## Getting Started

### Build the toolchain

Assume you have already installed essential build tools such as cmake, clang, ninja etc.

```
git clone https://github.com/THU-DSP-LAB/llvm-project.git
cd llvm-project
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DLLVM_CCACHE_BUILD=ON -DLLVM_ENABLE_PROJECTS="clang;lld;libclc" -DLLVM_TARGETS_TO_BUILD="AMDGPU;X86;RISCV" -DLLVM_TARGET_ARCH="riscv32" -DBUILD_SHARED_LIBS=ON -DLLVM_BUILD_LLVM_DYLIB=ON -DLLVM_ENABLE_TERMINFO=ON -DCMAKE_INSTALL_PREFIX=../install -G Ninja ../llvm
ninja
```

### Build pocl

```
# Add ventus-llvm built llvm-config to PATH
export PATH=<path_to_ventus_llvm_install_bin>:$PATH
git clone https://github.com/THU-DSP-LAB/pocl.git
cd pocl
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=../install -DENABLE_HOST_CPU_DEVICES=OFF -DENABLE_VENTUS=ON -DENABLE_ICD=ON -DENABLE_TESTS=OFF -DSTATIC_LLVM=OFF -G Ninja ../
ninja

# You can use following command to building pocl with system installed llvm.
cmake -DCMAKE_INSTALL_PREFIX=../install -DENABLE_ICD=ON -DENABLE_TESTS=OFF -DSTATIC_LLVM=OFF -DLLC_HOST_CPU=x86-64 -G Ninja ../
```

Build pocl as libOpenCL.so(icd loader+icd driver) instead of libpocl.so(icd driver), `-DENABLE_ICD=OFF` must be specified to cmake.

NOTE: the install folder of ventus-pocl should be merged with the install folder of ventus-llvm in order to correctly locate shared libraries, header files etc.

NOTE: `-DPOCL_DEBUG_MESSAGES=ON` is default on, you can set env variable `POCL_DEBUG` to enable debugging output(see pocl_debug.c for details).


### Build libclc(llvm version instead of pocl) (Work in progress)

```
# Add ventus-llvm built llvm-config to PATH
export PATH=<path_to_ventus_llvm_install_bin>:$PATH
cd llvm-project
mkdir build-libclc && cd build-libclc
cmake ../libclc -DCMAKE_LLAsm_COMPILER_WORKS=ON -DCMAKE_CLC_COMPILER_WORKS=ON -DCMAKE_CLC_COMPILER_FORCED=ON  -DCMAKE_LLAsm_FLAGS="-target riscv32 -mcpu=ventus-gpgpu" -DLIBCLC_TARGETS_TO_BUILD="riscv32--" -DCMAKE_CXX_FLAGS="-I <llvm-project-root>/llvm/include -std=c++17" -G Ninja
ninja
# Manually copy kernel builtins to the location where pocl driver can locate
cp riscv32--.bc <pocl_install_dir>/share/pocl/kernel-riscv32.bc
```


### Build icd loader

You can use `apt install ocl-icd-libopencl1` to install ocl icd loader `libOpenCL.so`, but we want to build our own icd loader with debug information, so here it is.

```
git clone https://github.com/OCL-dev/ocl-icd.git
cd ocl-icd
./bootstrap
./configure
make
ls .libs # libOpenCL.so is located in .libs
```

Then copy `libOpenCL.so*` from ocl-icd/.libs to `ventus-llvm/install/lib` folder(where LLVM shared libraries located)

Run `export LD_LIBRARY_PATH=<path_to>/ventus-llvm/install/lib` to tell OpenCL application to use your own built `libOpenCL.so`, also to correctly locate LLVM shared libraries.


### Bridge icd loader `libOpenCL.so` and pocl ocl device driver `libpocl.so`(pocl built with ENABLE_ICD=ON)

Run `export OCL_ICD_VENDORS=<path_to>/libpocl.so` to tell ocl icd loader where the icd driver is.

Finally, run `export POCL_DEVICES="ventus"` to tell pocl driver which device is available(should we set ventus as default device?).

You will see Ventus GPGPU device is found if your setup is correct.
```
$ <pocl-install-dir>/bin/poclcc -l

LIST OF DEVICES:
0:
  Vendor:   THU
    Name:   Ventus GPGPU device
 Version:   2.2 HSTR: THU-ventus-gpgpu
```

Also, you can try to set `POCL_DEBUG=all` and run example under `<pocl-build-dir>` to see the full OpenCL software stack execution pipeline. For example(Work in progress).
```
aries@legion:~/workspace/ventus-pocl/build-ventus$ ./examples/vecadd/vecadd
** Final POCL_DEBUG flags: FFFFFFFFFFFFFFFF
[2023-01-16 07:59:38.108659834]POCL: in fn POclCreateCommandQueue at line 99:
  |   GENERAL |  Created Command Queue 3 (0x56134aba4a10) on device 0
[2023-01-16 07:59:38.109002683]POCL: in fn void pocl_llvm_create_context(cl_context) at line 431:
  |      LLVM |  Created context 2 (0x56134aba48e0)
[2023-01-16 07:59:38.139981683132353]POCL: in fn POclCreateContext at line 228:
  |   GENERAL |  Created Context 2 (0x56134aba48e0)
[2023-01-16 07:59:38.109029362]POCL: in fn POclRetainContext at line 33:
  | REFCOUNTS |  Retain Context 2 (0x56134aba48e0), Refcount: 2
[2023-01-16 07:59:38.30173808955]POCL: in fn POclCreateCommandQueue at line 99:
  |   GENERAL |  Created Command Queue 4 (0x56134aba7230) on device 0
[2023-01-16 07:59:38.109127629]POCL: in fn POclRetainContext at line 33:
  | REFCOUNTS |  Retain Context 2 (0x56134aba48e0), Refcount: 3
[2023-01-16 07:59:38.109146625]POCL: in fn compile_and_link_program at line 720:
  |      LLVM |  building program with options 
[2023-01-16 07:59:38.94639713517827]POCL: in fn compile_and_link_program at line 757:
  |      LLVM |  building program for 1 devs with options 
[2023-01-16 07:59:38.109153330]POCL: in fn compile_and_link_program at line 761:
  |      LLVM |     BUILDING for device: ventus
[2023-01-16 07:59:38.206267587868]POCL: in fn pocl_driver_build_source at line 712:
  |      LLVM |  building from sources for device 0
[2023-01-16 07:59:38.139981683681094]POCL: in fn int pocl_llvm_build_program(cl_program, unsigned int, cl_uint, const cl_program *, const char **, int) at line 406:
  |      LLVM |  all build options: -DPOCL_DEVICE_ADDRESS_BITS=32 -D__USE_CLANG_OPENCL_C_H -xcl -Dinline= -I. -cl-kernel-arg-info  -D__ENDIAN_LITTLE__=1 -DCL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE=0 -D__OPENCL_VERSION__=200 -cl-std=CL2.0 -D__OPENCL_C_VERSION__=200 -fno-builtin -triple=riscv32 -target-cpu ventus-gpgpu 
[2023-01-16 07:59:38.337265138]POCL: in fn POclRetainContext at line 33:
  | REFCOUNTS |  Retain Context 2 (0x56134aba48e0), Refcount: 4
[2023-01-16 07:59:38.30402060096]POCL: in fn POclCreateBuffer at line 262:
  |    MEMORY |  Created Buffer 6 (0x56134aba8fb0), MEM_HOST_PTR: 0x56134abab710, device_ptrs[0]: 0x56134aee15e0, SIZE 512, FLAGS 36 
[2023-01-16 07:59:38.206495725866]POCL: in fn POclRetainContext at line 33:
  | REFCOUNTS |  Retain Context 2 (0x56134aba48e0), Refcount: 5
[2023-01-16 07:59:38.30402070991]POCL: in fn POclCreateBuffer at line 262:
  |    MEMORY |  Created Buffer 7 (0x56134ac25be0), MEM_HOST_PTR: 0x56134aedc5c0, device_ptrs[0]: 0x56134aee5920, SIZE 512, FLAGS 36 
[2023-01-16 07:59:38.206495738647]POCL: in fn POclRetainContext at line 33:
  | REFCOUNTS |  Retain Context 2 (0x56134aba48e0), Refcount: 6
[2023-01-16 07:59:38.30402084819]POCL: in fn POclCreateBuffer at line 262:
  |    MEMORY |  Created Buffer 8 (0x56134ababd20), MEM_HOST_PTR: (nil), device_ptrs[0]: 0x56134ac99af0, SIZE 512, FLAGS 1 
[2023-01-16 07:59:38.337321919]POCL: in fn POclCreateKernel at line 139:
  |   GENERAL |  Created Kernel vecadd (0x56134abafe80)
[2023-01-16 07:59:38.94639941695914]POCL: in fn POclSetKernelArg at line 113:
  |   GENERAL |  Kernel          vecadd || SetArg idx   0 ||   float* || Local 0 || Size      8 || Value 0x7ffea8ab8580 || Pointer 0x56134aba8fb0 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  B08FBA4A 13560000
[2023-01-16 07:59:38.337380166]POCL: in fn POclSetKernelArg at line 113:
  |   GENERAL |  Kernel          vecadd || SetArg idx   1 ||   float* || Local 0 || Size      8 || Value 0x7ffea8ab8588 || Pointer 0x56134ac25be0 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  E05BC24A 13560000
[2023-01-16 07:59:38.337387290]POCL: in fn POclSetKernelArg at line 113:
  |   GENERAL |  Kernel          vecadd || SetArg idx   2 ||   float* || Local 0 || Size      8 || Value 0x7ffea8ab8590 || Pointer 0x56134ababd20 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  20BDBA4A 13560000
[2023-01-16 07:59:38.337396648]POCL: in fn pocl_kernel_calc_wg_size at line 173:
  |   GENERAL |  Preparing kernel vecadd with local size 128 x 1 x 1 group sizes 1 x 1 x 1...
[2023-01-16 07:59:38.337407683]POCL: in fn POclRetainCommandQueue at line 35:
  | REFCOUNTS |  Retain Command Queue 4 (0x56134aba7230), Refcount: 2
[2023-01-16 07:59:38.30402184622]POCL: in fn pocl_create_event at line 528:
  |    EVENTS |  Created event 1 (0x56134ab4bec0) Command ndrange_kernel
[2023-01-16 07:59:38.337438693]POCL: in fn pocl_create_command_struct at line 672:
  |    EVENTS |  Created immediate command struct: CMD 0x56134abaf990 (event 1 / 0x56134ab4bec0, type: ndrange_kernel)
[2023-01-16 07:59:38.140729235867670]POCL: in fn POclRetainCommandQueue at line 35:
  | REFCOUNTS |  Retain Command Queue 3 (0x56134aba4a10), Refcount: 2
[2023-01-16 07:59:38.30402222755]POCL: in fn pocl_create_event at line 528:
  |    EVENTS |  Created event 2 (0x56134aba6270) Command migrate_mem_objects
[2023-01-16 07:59:38.337454896]POCL: in fn pocl_create_command_struct at line 649:
  |    EVENTS |  event pointer provided
[2023-01-16 07:59:38.337460553]POCL: in fn pocl_create_command_struct at line 672:
  |    EVENTS |  Created immediate command struct: CMD 0x56134abad5f0 (event 2 / 0x56134aba6270, type: migrate_mem_objects)
[2023-01-16 07:59:38.337465721]POCL: in fn pocl_command_enqueue at line 1237:
  |    EVENTS |  Pushed Event 2 to CQ 3.
[2023-01-16 07:59:38.140729235893092]POCL: in fn pocl_update_event_queued at line 2084:
  |    EVENTS |  Event queued: 2
[2023-01-16 07:59:38.140729235922914]POCL: in fn pocl_update_event_submitted at line 2104:
  |    EVENTS |  Event submitted: 2
[2023-01-16 07:59:38.2676586394723155918]POCL: in fn pocl_update_event_running_unlocked at line 2123:
  |    EVENTS |  Event running: 2
[2023-01-16 07:59:38.140729235937301]POCL: in fn pocl_update_event_finished at line 2217:
  |    EVENTS |  ventus: Command complete, event 2
[2023-01-16 07:59:38.140729235941632]POCL: in fn pocl_exec_command at line 498:
  |    TIMING |       >>>         6.286  us    Event Migrate Buffer(s)     
[2023-01-16 07:59:38.69057002800]POCL: in fn POclReleaseMemObject at line 50:
  | REFCOUNTS |  Release Memory Object 6 (0x56134aba8fb0), Refcount: 2
[2023-01-16 07:59:38.215085903784]POCL: in fn POclReleaseEvent at line 40:
  | REFCOUNTS |  Release Event 2 (0x56134aba6270), Refcount: 1
[2023-01-16 07:59:38.140729235963562]POCL: in fn pocl_create_event_sync at line 544:
  |    EVENTS |  create event sync: waiting 1 , notifier 2
[2023-01-16 07:59:38.206495976875]POCL: in fn POclReleaseEvent at line 40:
  | REFCOUNTS |  Release Event 1 (0x56134ab4bec0), Refcount: 3
[2023-01-16 07:59:38.140729235973060]POCL: in fn POclRetainKernel at line 34:
  | REFCOUNTS |  Retain Kernel vecadd (0x56134abafe80), Refcount: 2
[2023-01-16 07:59:38.337557073]POCL: in fn pocl_command_enqueue at line 1191:
  |    EVENTS |  In-order Q; adding event syncs
[2023-01-16 07:59:38.337560356]POCL: in fn pocl_command_enqueue at line 1237:
  |    EVENTS |  Pushed Event 1 to CQ 4.
[2023-01-16 07:59:38.140729235985422]POCL: in fn pocl_update_event_queued at line 2084:
  |    EVENTS |  Event queued: 1
Segmentation fault (core dumped)
```

### Compile a OpenCL C program into Ventus GPGPU assembly

vector_add.cl:

```
__kernel void vectorAdd(__global float* A, __global float* B) {
  unsigned tid = get_global_id(0);
  A[tid] += B[tid];
}
```

Compiler OpenCL C into Ventus assembly:

```
clang -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu -O1 -S vector_add.cl -o vector_add.s
```

NOTE: OpenCL host side program should be linked with icd loader `-lOpenCL`.

### TODOs

* Emit `barrier` instruction for all stores to local/global memory except sGPR spill.
* Stacks for sGPR spilling and per-thread usage is supported by using RISCV::X2 as warp level stack, RISCV::X4 as per-thread level stack. But the 2 stack size calculation are not yet splitted out, so a lot of stack slots are wasted.
* VentusRegextInsertion pass may generate incorrect register ordering for next instruction, see FIXME in that pass. To avoid breaking def-use chain, we could keep the extended instruction unmodified by removing `Op.setRegIgnoreDUChain()` from the pass, the elf generation pass should ignore the higher bit(>2^5) of the register encoding automatically.
* Pattern match VV and VX optimization. There is only type information in the DAG pattern matching, we can't specify whether to match a DAG to a vop.vv or vop.vx MIR in a tblgen pattern, so a fix pass should be ran after codegen pass.
