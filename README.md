# This is the Ventus GPGPU port of LLVM Compiler Infrastructure

Ventus GPGPU is based on RISCV RV32IMACZfinxZve32f ISA with fully redefined concept of V-extension.

For more architecture detail, please refer to
[Ventus GPGPU Arch](https://github.com/THU-DSP-LAB/ventus-gpgpu/blob/master/docs/Ventus-GPGPU-doc.md)

## Getting Started

### 1: Programs related repositories

Download all the repositories firstly and place them in the same path.

* llvm-ventus : git clone https://github.com/THU-DSP-LAB/llvm-project.git
* pocl : git clone https://github.com/THU-DSP-LAB/pocl.git
* ocl-icd : git clone https://github.com/OCL-dev/ocl-icd.git
* isa-simulator(spike) : git clone https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator.git
* driver : git clone https://github.com/yangzexia/ventus-driver.git

> ATTENTION: Remember to check branch for every repository, cause the project are under development, if you get any build errors, feel free to give a issue or just contact authors

### 2: Build all the programs

Our program is based on LLVM, so the need packages to build ventus are almost the same as what are needed to build LLVM, you can refer to [official website](https://llvm.org/docs/GettingStarted.html) for detailed llvm building guidance, we just list most important needed packages here.

* ccache
* cmake
* ninja
* clang(optional)

> If you see any packages missing information, just install them

The following packages are needed for other repositories:

* device-tree-compiler
* bsdmainutils


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

#### 4.4: Driver using example

Accordingly, after all the building process, you can change directory to `<llvm-ventus-parentFolder>/pocl/build/examples/vecadd` directory, then export variables as what [Bridge icd loader](#3-bridge-icd-loader) does, finally just execute the file `vecadd`
### 5: TODOs

* Emit `barrier` instruction for all stores to local/global memory except sGPR spill.
* Stacks for sGPR spilling and per-thread usage is supported by using RISCV::X2 as warp level stack, RISCV::X4 as per-thread level stack. But the 2 stack size calculation are not yet splitted out, so a lot of stack slots are wasted.
* VentusRegextInsertion pass may generate incorrect register ordering for next instruction, see FIXME in that pass. To avoid breaking def-use chain, we could keep the extended instruction unmodified by removing `Op.setRegIgnoreDUChain()` from the pass, the elf generation pass should ignore the higher bit(>2^5) of the register encoding automatically.
* ~~Pattern match VV and VX optimization. There is only type information in the DAG pattern matching, we can't specify whether to match a DAG to a vop.vv or vop.vx MIR in a tblgen pattern, so a fix pass should be ran after codegen pass~~.
* Opencl kernel api - get_enqueued_local_size, need to support non-uniform workgroup
* `mem_fence` builtin support

### 6: Github actions

We add a customized action for VENTUS including `Building ventus` and `Building ISA simulator`, later we will add `Test workflow` to make all the process automatable,
the workflow file is `.github/workflows/ventus-build.yml`

