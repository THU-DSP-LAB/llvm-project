# This is the Ventus GPGPU port of LLVM Compiler Infrastructure

Ventus GPGPU is based on RISCV RV32IMAZfinxZve32f ISA with fully redefined concept of V-extension.

The Ventus GPGPU OpenCL compiler based on LLVM is developed by [Terapines Technology (Wuhan) Co., Ltd](https://www.terapines.com/)

承影GPGPU OpenCL编译器由Terapines([兆松科技](https://www.terapines.com/))负责开发

For more architecture detail, please refer to
[Ventus GPGPU Arch](https://github.com/THU-DSP-LAB/ventus-gpgpu/blob/master/docs/Ventus-GPGPU-doc.md)

## Getting Started

### 1: Programs related repositories

Download all the repositories firstly and place them in the same path.

* llvm-ventus : git clone https://github.com/THU-DSP-LAB/llvm-project.git
* pocl : git clone https://github.com/THU-DSP-LAB/pocl.git
* ocl-icd : git clone https://github.com/OCL-dev/ocl-icd.git
* isa-simulator(spike) : git clone https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator.git
* driver : git clone https://github.com/THU-DSP-LAB/ventus-driver.git
* rodinia : git clone https://github.com/THU-DSP-LAB/gpu-rodinia.git  (The method to download the dataset is in the `ventus_readme.md`.)

> ATTENTION: Remember to check branch for every repository, cause the project are under development, if you get any build errors, feel free to give an issue or just contact authors

### 2: Build all the programs

Our program is based on LLVM, so the need packages to build ventus are almost the same as what are needed to build LLVM, you can refer to [official website](https://llvm.org/docs/GettingStarted.html) for detailed llvm building guidance, we just list most important needed packages here.

* ccache
* cmake
* ninja
* clang

> NOTE: If you see any packages missing information, just install them.

The following packages are needed for other repositories:

* device-tree-compiler
* bsdmainutils

> ATTENTION: In addition, we also provide Dockerfiles for Ubuntu and CentOS in `.github/workflows/containers/dockerfiles`. You can use them directly if needed. The following "6: Docker image" has the corresponding usage.

Before running `./build-ventus.sh` to automatically build all the programs, we need to set the following commands:
* For developers who want to build `Debug` version for llvm, `export BUILD_TYPE=Debug`, since it's set default to be 'Release'.
* `export POCL_DIR=<path-to-pocl-dir>`, default folder path will be set to be `<path-to-llvm-ventus>/../pocl`.
* `export OCL_ICD_DIR=<path-to-ocl-icd-dir>`, default folder path will be set to be `<path-to-llvm-ventus>/../ocl-icd`.

You can dive into `build-ventus.sh` file to see the detailed information about build process.

### 3: Bridge icd loader

Run `export VENTUS_INSTALL_PREFIX=<path_to_install>` to set `VENTUS_INSTALL_PREFIX` environment variable(system environment variable recommended), default folder path will be set to be `<path-to-llvm-ventus>/install`.

Run `export LD_LIBRARY_PATH=${VENTUS_INSTALL_PREFIX}/lib` to tell OpenCL application to use your own built `libOpenCL.so`, also to correctly locate LLVM shared libraries.

Run `export OCL_ICD_VENDORS=${VENTUS_INSTALL_PREFIX}/lib/libpocl.so` to tell ocl icd loader where the icd driver is.

Finally, run `export POCL_DEVICES="ventus"` to tell pocl driver which device is available(should we set ventus as default device?). You will see Ventus GPGPU device is found if your setup is correct:
```
$ <pocl-install-dir>/bin/poclcc -l

// The following output should be shown:
LIST OF DEVICES:
0:
  Vendor:   THU
    Name:   Ventus GPGPU device
 Version:   2.2 HSTR: THU-ventus-gpgpu
```

> NOTE: OpenCL host side program should be linked with icd loader `-lOpenCL`.

Also, you can try to set `POCL_DEBUG=all` and run example under `<pocl-build-dir>` to see the full OpenCL software stack execution pipeline. For example:
```
./<pocl-install-dir>/examples/vecadd/vecadd
```
You will see that the program runs correctly.

### 4: Compiler using example

We can now use our built compiler to generate an ELF file, and using [spike](https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator) to complete the isa simulation.

> NOTE: Cause the address space requirement in spike, we use a customized linker script for our compiler.

First, name the following program `vecadd.cl`,  and place it under `<path-to-llvm-ventus>`:
```
__kernel void vectorAdd(__global float* A, __global float* B) {
  unsigned tid = get_global_id(0);
  A[tid] += B[tid];
}
```
Then, run the commands listed as follows under the same directory.

> NOTE: Remember to build libclc too because we need the libclc library.

#### 4.1: Generate ELF file

##### 4.1.1 Compile directly

```
./install/bin/clang -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu vecadd.cl  ./install/lib/crt0.o -L./install/lib -lworkitem -I./libclc/generic/include -nodefaultlibs ./libclc/riscv32/lib/workitem/get_global_id.cl -O1 -cl-std=CL2.0 -Wl,-T,utils/ldscripts/ventus/elf32lriscv.ld -o vecadd.riscv
```

##### 4.1.2 Compile step-by-step

1. Compile OpenCL code to LLVM IR assembly (.ll file):

```sh
./install/bin/clang -S -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu vecadd.cl -emit-llvm -o vecadd.ll
```

2. Compile LLVM IR to RISC-V assembly or object file:

```sh
./install/bin/llc -mtriple=riscv32 -mcpu=ventus-gpgpu vecadd.ll -o vecadd.s
```

```sh
./install/bin/llc -mtriple=riscv32 -mcpu=ventus-gpgpu --filetype=obj vecadd.ll -o vecadd.o
```

3. Link essential library:
Linking `crt0` and `libclc`
All the libclc workitem functions' implementation is included in `riscv32clc.o`
```sh
./install/bin/ld.lld -o vecadd.riscv -T utils/ldscripts/ventus/elf32lriscv.ld vecadd.o ./install/lib/crt0.o ./install/lib/riscv32clc.o -L./install/lib -lworkitem --gc-sections
```

##### 4.1.3 Compile assembly code to object file (`.s` to `.o`)
Take custome instructions `custome.s` as an example :
```asm
vftta.vv v0, v0, v1
vfexp v0, v1
vadd12.vi v0, v1, 8
```

```sh
./install/bin/clang -c -target riscv32 -mcpu=ventus-gpgpu custom.s -o custom.o
```

#### 4.2: Dump file

```
./install/bin/llvm-objdump -d --mattr=+v,+zfinx vecadd.riscv >& vecadd.txt
```

you will see output like below, `0x80000000` is the space address required by spike for `_start` function, this is the reason why we use a customized linker script:

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

or you can check encoding of custom instructions:

```
./install/bin/llvm-objdump -d --mattr=+v,+zfinx custom.o >& custom.txt
```

```
custom.o:       file format elf32-littleriscv

Disassembly of section .text:

00000000 <.text>:
       0: 0b c0 00 0e   vftta.vv        v0, v0, v1
       4: 0b 60 10 0a   vfexp   v0, v1
       8: 0b 80 80 00   vadd12.vi       v0, v1, 8
```

#### 4.3: Running in spike

We need to run the isa simulator to verify our compiler. Use [spike](https://github.com/THU-DSP-LAB/ventus-gpgpu-isa-simulator) from THU and follow the `README.md`.

#### 4.4: Driver using example

Accordingly, after all the building process, you can change directory to `<path-to-llvm-ventus>/../pocl/build/examples/vecadd` directory, then export variables as what [3: Bridge icd loader](#3-bridge-icd-loader) does, finally just execute the file `vecadd`.

### 5: Github actions

the workflow file is `.github/workflows/ventus-build.yml`, including below jobs:

* Build llvm
* Build ocl-icd
* Build libclc
* Build isa-simulator
* Build sumulator-driver
* Build pocl
* Isa simulation test
* GPU-rodinia testsuite
* Pocl testing

### 6: Docker image

If the user needs to build the toolchain of the Ventus project in an environment other than Ubuntu, such as the CentOS system, we provide the Dockerfile for building the CentOS image. The file is under `.github/workflows/containers/dockerfiles`.

Note: When using build-ventus.sh to build the instantiated centos container, the following modifications are required, which are different from [2: Build all the programs](#2-build-all-the-programs):

```
--- a/build-ventus.sh
+++ b/build-ventus.sh
@@ -232,7 +232,7 @@ export_elements() {
   export SPIKE_TARGET_DIR=${VENTUS_INSTALL_PREFIX}
   export VENTUS_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
   export POCL_DEVICES="ventus"
-  export OCL_ICD_VENDORS=${VENTUS_INSTALL_PREFIX}/lib/libpocl.so
+  export OCL_ICD_VENDORS=${VENTUS_INSTALL_PREFIX}/lib64/libpocl.so
 }
```
