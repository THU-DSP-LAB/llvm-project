# This is the Ventus GPGPU port of LLVM Compiler Infrastructure

Ventus GPGPU is based on RISCV RV32IMAZfinxZve32f ISA with fully redefinition the concept of V-extension.

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

### Build libclc

```
LLVM_DIR=~/workspace/ventus-llvm/build/lib/cmake/llvm cmake -DLLVM_CONFIG=~/workspace/ventus-llvm/install/bin/llvm-config ../../libclc -DCMAKE_LLAsm_COMPILER_WORKS=ON -DCMAKE_CLC_COMPILER_WORKS=ON -DCMAKE_CLC_COMPILER_FORCED=ON  -DCMAKE_LLAsm_FLAGS="-target riscv32 -mcpu=ventus-gpgpu" -DLIBCLC_TARGETS_TO_BUILD="riscv32--" -DCMAKE_CXX_FLAGS="-I ~/workspace/ventus-llvm/llvm/include -std=c++17" -G Ninja
```

### Build pocl

```
# Add ventus-llvm built llvm-config to PATH
export PATH=<path_to_ventus_llvm_install_bin>:$PATH
git clone https://github.com/THU-DSP-LAB/pocl.git
cd pocl
mkdir build && cd build
# NOTE: LLC_TRIPLE and LLC_HOST_CPU is used for kernel target triple and cpu of the OpenCL device, their name should be definitely renamed.
cmake -DCMAKE_INSTALL_PREFIX=../install -DENABLE_HOST_CPU_DEVICES=OFF -DENABLE_VENTUS=ON -DENABLE_ICD=ON -DENABLE_TESTS=OFF -DSTATIC_LLVM=OFF -G Ninja ../
ninja

# Use following command to building pocl with system installed llvm.
cmake -DCMAKE_INSTALL_PREFIX=../install -DENABLE_ICD=ON -DENABLE_TESTS=OFF -DSTATIC_LLVM=OFF -DLLC_HOST_CPU=x86-64 -G Ninja ../
```

Build pocl as libOpenCL.so(icd loader+icd driver) instead of libpocl.so(icd driver), `-DENABLE_ICD=OFF` must be specified to cmake.

NOTE: the install folder of ventus-pocl should be merged with the install folder of ventus-llvm in order to correctly locate shared libraries, header files etc.

NOTE: `-DPOCL_DEBUG_MESSAGES=ON` is default on, you can set env variable `POCL_DEBUG` to enable debugging output(see pocl_debug.c for details).


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
$ poclcc -l

LIST OF DEVICES:
0:
  Vendor:   THU
    Name:   Ventus GPGPU device
 Version:   2.2 HSTR: THU-ventus-gpgpu
```

Then you can build your OpenCL program with -lOpenCL.


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

### TODOs

* Emit `barrier` instruction for all stores to local/global memory except sGPR spill.
* Stacks for sGPR spilling and per-thread usage is supported by using RISCV::X2 as warp level stack, RISCV::X4 as per-thread level stack. But the 2 stack size calculation are not yet splitted out, so a lot of stack slots are wasted.
* VentusRegextInsertion pass may generate incorrect register ordering for next instruction, see FIXME in that pass. To avoid breaking def-use chain, we could keep the extended instruction unmodified by removing `Op.setRegIgnoreDUChain()` from the pass, the elf generation pass should ignore the higher bit(>2^5) of the register encoding automatically.
* Pattern match VV and VX optimization. There is only type information in the DAG pattern matching, we can't specify whether to match a DAG to a vop.vv or vop.vx MIR in a tblgen pattern, so a fix pass should be ran after codegen pass.
