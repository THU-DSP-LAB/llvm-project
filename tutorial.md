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

## 2: 添加ABI信息

初始的[commit](https://github.com/THU-DSP-LAB/llvm-project/commit/9c54c010b2d68c3546aeae942555ad5d3f2d4e30#diff-d5392fbd34fbb607cc468738adce16985f9461c9a3f5bc7c5ed64dba44137d19)
主要是参考了AMDGPU的ABI设计，针对kernel和non-kernel函数有不同的处理

### 2.1: ventus ABI信息

主要是实例化`VentusRISCVABIInfo`这个类，继承自`DefaultABIInfo`

* 确定参数的传递方式
  * 约定超过多少个参数往栈上存，其余的参数直接用寄存器传递
* 确认函数返回值传递方式
* <font color='red'>确定函数调用约定</font>
* <font color='red'>管理对齐要求</font>

然后让LLVM针对RISCV平台下的ABI调用使用`VentusRISCVABIInfo`这个类, 这样就算是把ventus ABI信息注册上了

```cpp
  RISCVTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT, unsigned XLen,
                         unsigned FLen)
      : TargetCodeGenInfo(std::make_unique<VentusRISCVABIInfo>(CGT, XLen)) {}
```

### 2.2: 后端codegen

前端定义好ABI信息后, 后端中跟这个比较相关的接口在`RISCVISelLowering.cpp`这个文件中，这里面有后端中关于怎么处理函数参数，函数返回值的接口， 这两个接口略复杂，需要时间消化，可以后面再学，需要参考承影项目的设计文档

* RISCVTargetLowering::LowerFormalArguments
* RISCVTargetLowering::LowerReturn

### 2.3: 测试

写一个简单的文件测试一下ABI的函数参数传递

```
int test_abi(int16 a, int16 b, int c) {
  return a.x + b.x + c;
}
```
运行命令行`./build/bin/clang -target riscv32 -mcpu=ventus-gpgpu abi.cl -S`,可以得到以下输出

```
test_abi:
	addi	sp, sp, 4
	addi	tp, tp, 16
	regext	zero, zero, 1
	vmv.v.x	v32, tp
	sw	ra, -4(sp)
	regext	zero, zero, 8
	vlw.v	v1, -20(v32)
	vadd.vv	v0, v16, v0
	vadd.vv	v0, v0, v1
	lw	ra, -4(sp)
	addi	sp, sp, -4
	addi	tp, tp, -16
	ret
```
ventus ABI规定，non-kernel函数传递使用32个向量寄存器，其余的用栈传递，这里的c参数是第33个参数，所以这里是直接从栈上加载过来，其余的参数都是寄存器，感兴趣可以测试一下kernel函数


## 3: 寄存器的定义与使用

Ventus架构中使用了64个sGPR与256个vGPR,详细定义请参考[ventus寄存器定义](llvm/lib/Target/RISCV/VentusRegisterInfo.td),扩展寄存器或者自定义寄存器可以参考现有的一些架构的tablegen设计

```
def GPR : RVRegisterClass<"RISCV", [XLenVT], 32, (add
    (sequence "X%u", 5, 63),
    (sequence "X%u", 0, 4)
  )> {
  let RegInfos = XLenRI;
  let IsSGPR = 1;
}
```
以上就是64个sGPR寄存器的完整定义，build完可以参考build/lib/Target/RISCV/RISCVGenRegisterInfo.inc这个文件, 这里面会列出所有的在tablegen里面寄存器信息，关于寄存器定义有几个比较重要的点

* 寄存器的名字(alias)
* 寄存器支持处理的数据类型

在llvm/lib/Target/RISCV/RISCVISelLowering.cpp这个文件中，会定义每种寄存器原生支持什么类型的数据

```cpp
  if (Subtarget.hasVInstructions()) {
    // TODO: add more data type mapping
    addRegisterClass(MVT::i32, &RISCV::VGPRRegClass);

    addRegisterClass(MVT::f32, &RISCV::VGPRRegClass);
  }
```

这个的意思就是表明VGPR寄存器里面支持合法存放i32与f32类型的数据