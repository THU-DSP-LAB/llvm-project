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
cmake -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS="clang;lld;libclc" -DLLVM_TARGETS_TO_BUILD="RISCV" -DCMAKE_INSTALL_PREFIX=../install -G Ninja ../llvm
ninja
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

### TODOs

* VentusRegextInsertion pass may generate incorrect register ordering for next instruction, see FIXME in that pass. To avoid breaking def-use chain, we could keep the extended instruction unmodified by removing `Op.setRegIgnoreDUChain()` from the pass, the elf generation pass should ignore the higher bit(>2^5) of the register encoding automatically.
* Pattern match VV and VX optimization. There is only type information in the DAG pattern matching, we can't specify whether to match a DAG to a vop.vv or vop.vx MIR in a tblgen pattern, so a fix pass should be ran after codegen pass.
