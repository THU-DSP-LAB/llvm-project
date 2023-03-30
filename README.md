# This is the Ventus GPGPU port of LLVM Compiler Infrastructure

Ventus GPGPU is based on RISCV RV32IMACZfinxZve32f ISA with fully redefined concept of V-extension.

For more architecture detail, please refer to
[Ventus GPGPU Arch](https://github.com/THU-DSP-LAB/ventus-gpgpu/blob/master/docs/Ventus-GPGPU-doc.md)

## Getting Started

### 1: Programs related repositories

Download all the repositories firstly

* llvm-ventus : git clone https://github.com/THU-DSP-LAB/llvm-project.git
* pocl : git clone https://github.com/THU-DSP-LAB/pocl.git
* ocl-icd : git clone https://github.com/OCL-dev/ocl-icd.git

> libclc can be built from llvm-ventus repository

### 2: Build all the programs

Assume you have already installed essential build tools such as cmake, clang, ninja etc.

Run `./build-ventus.sh` to automatically build all the programs, but we need to run firstly
* `export POCL_DIR=<path-to-pocl-dir>`, default folder path will be set to be **`<llvm-ventus-parentFolder>`/pocl**
* `export OCL_ICD_DIR=<path-to-ocl-icd-dir>`, default folder path will be set to be **`<llvm-ventus-parentFolder>`/ocl-icd**

You can dive into `build-ventus.sh` file to see the detailed information about build process

### 3: Bridge icd loader

Run `export LD_LIBRARY_PATH=<path_to>/ventus-llvm/install/lib` to tell OpenCL application to use your own built `libOpenCL.so`, also to correctly locate LLVM shared libraries

Run `export OCL_ICD_VENDORS=<path_to>/libpocl.so` to tell ocl icd loader where the icd driver is.

Finally, run `export POCL_DEVICES="ventus"` to tell pocl driver which device is available(should we set ventus as default device?).

You will see Ventus GPGPU device is found if your setup is correct.

NOTE: OpenCL host side program should be linked with icd loader `-lOpenCL`.
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
[2023-01-19 01:58:36.582896453]POCL: in fn POclCreateCommandQueue at line 98:
  |   GENERAL |  Created Command Queue 3 (0x561c8b324d90) on device 0
[2023-01-19 01:58:36.583753425]POCL: in fn void pocl_llvm_create_context(cl_context) at line 431:
  |      LLVM |  Created context 2 (0x561c8b324c60)
[2023-01-19 01:58:36.583777345]POCL: in fn POclCreateContext at line 227:
  |   GENERAL |  Created Context 2 (0x561c8b324c60)
[2023-01-19 01:58:36.583811014]POCL: in fn POclRetainContext at line 32:
  | REFCOUNTS |  Retain Context 2 (0x561c8b324c60), Refcount: 2
[2023-01-19 01:58:36.583817177]POCL: in fn POclCreateCommandQueue at line 98:
  |   GENERAL |  Created Command Queue 4 (0x561c8b3275b0) on device 0
[2023-01-19 01:58:36.584022807]POCL: in fn POclRetainContext at line 32:
  | REFCOUNTS |  Retain Context 2 (0x561c8b324c60), Refcount: 3
[2023-01-19 01:58:36.584082504]POCL: in fn compile_and_link_program at line 719:
  |      LLVM |  building program with options
[2023-01-19 01:58:36.584105109]POCL: in fn compile_and_link_program at line 756:
  |      LLVM |  building program for 1 devs with options
[2023-01-19 01:58:36.584127804]POCL: in fn compile_and_link_program at line 760:
  |      LLVM |     BUILDING for device: ventus
[2023-01-19 01:58:36.584144767]POCL: in fn pocl_driver_build_source at line 712:
  |      LLVM |  building from sources for device 0
[2023-01-19 01:58:36.585000531]POCL: in fn int pocl_llvm_build_program(cl_program, unsigned int, cl_uint, _cl_program* const*, const char**, int) at line 406:
  |      LLVM |  all build options: -DPOCL_DEVICE_ADDRESS_BITS=32 -D__USE_CLANG_OPENCL_C_H -xcl -Dinline= -I. -cl-kernel-arg-info  -D__ENDIAN_LITTLE__=1 -DCL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE=0 -D__OPENCL_VERSION__=200 -cl-std=CL2.0 -D__OPENCL_C_VERSION__=200 -fno-builtin -triple=riscv32 -target-cpu ventus-gpgpu
[2023-01-19 01:58:37.002888436]POCL: in fn POclRetainContext at line 32:
  | REFCOUNTS |  Retain Context 2 (0x561c8b324c60), Refcount: 4
[2023-01-19 01:58:37.002944911]POCL: in fn POclCreateBuffer at line 256:
  |    MEMORY |  Created Buffer 6 (0x561c8b329330), MEM_HOST_PTR: 0x561c8b65c9d0, device_ptrs[0]: (nil), SIZE 512, FLAGS 36
[2023-01-19 01:58:37.002959038]POCL: in fn POclRetainContext at line 32:
  | REFCOUNTS |  Retain Context 2 (0x561c8b324c60), Refcount: 5
[2023-01-19 01:58:37.002964856]POCL: in fn POclCreateBuffer at line 256:
  |    MEMORY |  Created Buffer 7 (0x561c8b331280), MEM_HOST_PTR: 0x561c8b39bb50, device_ptrs[0]: (nil), SIZE 512, FLAGS 36
[2023-01-19 01:58:37.002971025]POCL: in fn POclRetainContext at line 32:
  | REFCOUNTS |  Retain Context 2 (0x561c8b324c60), Refcount: 6
[2023-01-19 01:58:37.002976107]POCL: in fn POclCreateBuffer at line 256:
  |    MEMORY |  Created Buffer 8 (0x561c8b32c130), MEM_HOST_PTR: (nil), device_ptrs[0]: (nil), SIZE 512, FLAGS 1
[2023-01-19 01:58:37.002991572]POCL: in fn POclCreateKernel at line 139:
  |   GENERAL |  Created Kernel vecadd (0x561c8b32bd30)
[2023-01-19 01:58:37.003011888]POCL: in fn POclSetKernelArg at line 107:
  |   GENERAL |  Kernel          vecadd || SetArg idx   0 ||   float* || Local 0 || Size      8 || Value 0x7ffca34f6350 || Pointer 0x561c8b329330 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  3093328B 1C560000
[2023-01-19 01:58:37.003033612]POCL: in fn POclSetKernelArg at line 107:
  |   GENERAL |  Kernel          vecadd || SetArg idx   1 ||   float* || Local 0 || Size      8 || Value 0x7ffca34f6358 || Pointer 0x561c8b331280 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  8012338B 1C560000
[2023-01-19 01:58:37.003047371]POCL: in fn POclSetKernelArg at line 107:
  |   GENERAL |  Kernel          vecadd || SetArg idx   2 ||   float* || Local 0 || Size      8 || Value 0x7ffca34f6360 || Pointer 0x561c8b32c130 || *(uint32*)Value:        0 || *(uint64*)Value:        0 ||
Hex Value:  30C1328B 1C560000
[2023-01-19 01:58:37.003077696]POCL: in fn pocl_kernel_calc_wg_size at line 168:
  |   GENERAL |  Preparing kernel vecadd with local size 128 x 1 x 1 group sizes 1 x 1 x 1...
[2023-01-19 01:58:37.003111530]POCL: in fn pocl_driver_alloc_mem_obj at line 420:
  |    MEMORY |  Basic device ALLOC 0x561c8b65c9d0 / size 512
[2023-01-19 01:58:37.003123085]POCL: in fn pocl_driver_alloc_mem_obj at line 420:
  |    MEMORY |  Basic device ALLOC 0x561c8b39bb50 / size 512
[2023-01-19 01:58:37.003136372]POCL: in fn pocl_driver_alloc_mem_obj at line 420:
  |    MEMORY |  Basic device ALLOC 0x561c8b65bfb0 / size 512
[2023-01-19 01:58:37.003148678]POCL: in fn POclRetainCommandQueue at line 33:
  | REFCOUNTS |  Retain Command Queue 4 (0x561c8b3275b0), Refcount: 2
[2023-01-19 01:58:37.003159512]POCL: in fn pocl_create_event at line 526:
  |    EVENTS |  Created event 1 (0x561c8b329500) Command ndrange_kernel
[2023-01-19 01:58:37.003187245]POCL: in fn pocl_create_command_struct at line 669:
  |    EVENTS |  Created immediate command struct: CMD 0x561c8b32e440 (event 1 / 0x561c8b329500, type: ndrange_kernel)
[2023-01-19 01:58:37.003217828]POCL: in fn POclRetainKernel at line 33:
  | REFCOUNTS |  Retain Kernel vecadd (0x561c8b32bd30), Refcount: 2
[2023-01-19 01:58:37.003231820]POCL: in fn pocl_command_enqueue at line 1191:
  |    EVENTS |  In-order Q; adding event syncs
[2023-01-19 01:58:37.003263335]POCL: in fn pocl_command_enqueue at line 1236:
  |    EVENTS |  Pushed Event 1 to CQ 4.
[2023-01-19 01:58:37.003286127]POCL: in fn pocl_update_event_queued at line 2084:
  |    EVENTS |  Event queued: 1
[2023-01-19 01:58:37.003365527]POCL: in fn pocl_check_kernel_disk_cache at line 941:
  |   GENERAL |  Using a cached WG function: /work/tptuser/.cache/pocl/kcache/KK/IMMMNPJCCGPBGPCPFBDFJAFNGPBHCOEHIPAHC/vecadd/128-1-1-goffs0/vecadd.so
dlopen("/work/tptuser/.cache/pocl/kcache/KK/IMMMNPJCCGPBGPCPFBDFJAFNGPBHCOEHIPAHC/vecadd/128-1-1-goffs0/vecadd.so") failed with '/work/tptuser/.cache/pocl/kcache/KK/IMMMNPJCCGPBGPCPFBDFJAFNGPBHCOEHIPAHC/vecadd/128-1-1-goffs0/vecadd.so: wrong ELF class: ELFCLASS32'.
note: missing symbols in the kernel binary might be reported as 'file not found' errors.
Aborted (core dumped)
```

### 4: Compiler using example

we can now use our built compiler to generate an ELF file, and using [spike](https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator) to complete the isa simulation.

> Cause the address space requirement in spike, we use a customized linker script for our compiler

Take `vector_add.cl` below as an example :

```
__kernel void vectorAdd(__global float* A, __global float* B) {
  unsigned tid = get_global_id(0);
  A[tid] += B[tid];
}
```

#### 4.1: Generate ELF file

> Remember to build libclc too because we need the libclc library

Use command line under the root directory of `llvm-ventus`

```
./install/bin/clang -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu demo.cl  ./install/lib/crt0.o -L./install/lib -lworkitem -I./libclc/generic/include -nodefaultlibs ./libclc/riscv32/lib/workitem/get_global_id.cl -O1 -cl-std=CL2.0 -Wl,-T,utils/ldscripts/ventus/elf32lriscv.ld -o vecadd.riscv
```
Because the whole libclc library for `RISCV` is under tested, we don't use whole library, we just show a simple example now, after running the command line above, we will get `vecadd.riscv`.

#### 4.2: Dump file

```
./install/bin/llvm-objdump -d --mattr=+v vecadd.riscv >& vecadd.txt
```

you will see output like below, `0x80000000` is the space address required by spike for `_start` function, this is the reason why we use a customized linker script

```
vecadd.riscv:	file format elf32-littleriscv

Disassembly of section .text:

80000000 <_start>:
80000000: 97 21 00 00  	auipc	gp, 2
80000004: 93 81 01 80  	addi	gp, gp, -2048
80000008: 93 0e 00 02  	li	t4, 32
8000000c: d7 fe 0e 0d  	vsetvli	t4, t4, e32, m1, ta, ma
80000010: b7 2e 00 00  	lui	t4, 2
80000014: f3 ae 0e 30  	csrrs	t4, mstatus, t4
80000018: 93 0e 00 00  	li	t4, 0
8000001c: 73 21 60 80  	csrr	sp, 2054
80000020: 73 22 70 80  	csrr	tp, 2055

80000024 <.Lpcrel_hi1>:
80000024: 17 15 00 00  	auipc	a0, 1
80000028: 13 05 85 fe  	addi	a0, a0, -24

....
....
....
```

#### 4.3: Running in spike

We need to run the isa simulator to verify our compiler

Use [spike](https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator) from THU and follow the `README.md`


### 5: TODOs

* Emit `barrier` instruction for all stores to local/global memory except sGPR spill.
* Stacks for sGPR spilling and per-thread usage is supported by using RISCV::X2 as warp level stack, RISCV::X4 as per-thread level stack. But the 2 stack size calculation are not yet splitted out, so a lot of stack slots are wasted.
* VentusRegextInsertion pass may generate incorrect register ordering for next instruction, see FIXME in that pass. To avoid breaking def-use chain, we could keep the extended instruction unmodified by removing `Op.setRegIgnoreDUChain()` from the pass, the elf generation pass should ignore the higher bit(>2^5) of the register encoding automatically.
* ~~Pattern match VV and VX optimization. There is only type information in the DAG pattern matching, we can't specify whether to match a DAG to a vop.vv or vop.vx MIR in a tblgen pattern, so a fix pass should be ran after codegen pass~~.
* Opencl kernel api - get_enqueued_local_size, need to support non-uniform workgroup
* `mem_fence` builtin support
