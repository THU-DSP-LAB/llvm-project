# 承影项目入门


## 1: 如何注册ventus架构

> 更复杂的是如何注册新的类似于RISCV这样的target，这个可以参考[cpu0](https://jonathan2251.github.io/lbd/llvmstructure.html#brief-introduction)项目

这里我们是在基于RISCV指令集，注册新的VENTUS gpu架构，可以参考这个[commit](https://github.com/THU-DSP-LAB/llvm-project/commit/c8338710752d71834378e5a93bd0c611fdb26fc8)

主要是需要在tablegen里面增加一个对于VENTUS架构的描述

```
def : ProcessorModel<"ventus-gpgpu", RocketModel, [Feature32Bit,
                                                   FeatureStdExtM,
                                                   FeatureStdExtA,
                                                   FeatureStdExtV,
                                                   FeatureStdExtZfinx
                                                   ]>;
```

* `ventus-gpgpu`: 注册的cpu的名字
* `RocketModel`: 采用的机器模型, 主要是关于指令调度信息的一些描述
* `Feature list`: 后面的这一大串支持的RISCV扩展，属性就是这个cpu默认支持的扩展
    * 比如Feature32Bit，就是说明VENTUS架构只适用于32位的RISCV机器
    * FeatureStdExtA表示VENTUS默认支持A扩展


### 1.1: 验证VENTUS架构注册的信息

首先需要编译一下llvm

```
  cmake -G Ninja -B build -S llvm \
    -DLLVM_CCACHE_BUILD=ON \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_PARALLEL_LINK_JOBS=12 \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_TARGETS_TO_BUILD="RISCV" \
    -DLLVM_TARGET_ARCH=riscv32 \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DCMAKE_INSTALL_PREFIX=install
```
然后运行`./build/bin/clang -target riscv32 -print-supported-cpus`,正确输出应该如下:
```
clang version 16.0.0 (https://github.com/THU-DSP-LAB/llvm-project c8338710752d71834378e5a93bd0c611fdb26fc8)
Target: riscv32
Thread model: posix
InstalledDir: <path-to-llvm>/build/bin
Available CPUs for this target:

        generic
        generic-rv32
        generic-rv64
        rocket
        rocket-rv32
        rocket-rv64
        sifive-7-series
        sifive-e20
        sifive-e21
        sifive-e24
        sifive-e31
        sifive-e34
        sifive-e76
        sifive-s21
        sifive-s51
        sifive-s54
        sifive-s76
        sifive-u54
        sifive-u74
        ventus-gpgpu

Use -mcpu or -mtune to specify the target's processor.
For example, clang --target=aarch64-unknown-linux-gui -mcpu=cortex-a35
```

