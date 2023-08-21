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

## 4: 指令定义相关

本章节涉及到承影软件设计部分最重要的部分,指令定义以及指令选择, 指令生成相关, 读者应该先阅读OpenCL的内存模型，GPU SIMT架构设计相关知识, 或者已经拥有相关背景知识

* [OpenCL model](https://www.khronos.org/assets/uploads/developers/library/2012-pan-pacific-road-show-June/OpenCL-Details-Taiwan_June-2012.pdf)
* [GPGPU architecture](https://link.springer.com/book/10.1007/978-3-031-01759-9)

### 4.1: 指令集以及指令定义
承影架构绝大多数指令承用了RISCV官方的指令集, 使用的指令集有`IMAZfinxZve32f`

* 使用`zfinx`,是因为硬件中虽然有fpu, 但是架构设计中并没有设计浮点寄存器，所以使用了zfinx这个扩展来支持浮点操作，但是这个支持官方并没有支持完，因为目前只是完成了MC级别的支持, 可以参考[这个文档](https://github.com/riscv/riscv-zfinx/blob/main/Zfinx_ISA_spec_addendum.adoc#overview), 指令定义详见[ventus-zfinx](llvm/lib/Target/RISCV/VentusInstrInfoF.td)这个文件

* V扩展几乎全部沿用了RISCV-V扩展官方指令，取消了mask的概念，同时增加了承影架构中自定义的很多自定义指令, 指令定义详见[ventus-v](llvm/lib/Target/RISCV/VentusInstrInfoV.td)

指令集的信息也可以在定义[ventus ProcessorModel](#1-如何注册ventus架构)里的feature list里面看到

### 4.2: 指令选择以及使用

这个章节的内容会比较复杂，因为涉及到的内容会比较多, 不同于RVV的SIMD(Single instruction multiple data), 承影架构是SIMT(Single instruction multiple thread), 软件层面的汇编码，其实会被多个线程执行，里面涉及到的核心知识点如以下:

* Divergence与Convergence: 分叉点与汇合点
  * 因为一个warp里面多个线程共用一个PC, 多线程执行同一份汇编码的时候，涉及分支指令必然会导致不同的执行路径, 有些线程会走if分支, 有些线程会走else分支, 如何让多线程执行完代码之后汇合到一个点就是SIMT架构的关键, 否则代码执行起来会有错误

* vALU与sALU: 向量寄存器vGPR里面的操作都在vALU中执行, 标量寄存器sGPR里面的操作都在sALU中执行

* VGPR的宽度是threadNum * 32bit,每一个线程的vGPR都是不同的

* 访存指令: 因为承影目前的架构中没有MMU, 所以目前用访存指令来区分访问的是什么内存的数据，这里需要先补一下OpenCL的内存模型相关知识
  * Global memory: 与传统的RISCV 访存指令保持一致(lw/sw等)
  * Local memory: 采用的是承影架构的自定义指令(vlw12.v/vsw12.v)指令
  * Private memory: 采用的是承影架构的自定义指令(vlw.v/vsw.v)指令

#### 4.2.1: Divergence analysis

大部分功能由[LegacyDivergenceAnalysis](llvm/lib/Analysis/LegacyDivergenceAnalysis.cpp)这个pass提供, 每个Target下面还有一个hook函数`TargetLowering::isSDNodeSourceOfDivergence`, 以RISCV为例, 此函数定义在`RISCVTargetLowering::isSDNodeSourceOfDivergence(const SDNode *N, FunctionLoweringInfo *FLI, LegacyDivergenceAnalysis *KDA)`, 经过分析之后能够得到

* 每一个DAG都会被标记是不是divergent节点

* Divergent操作在vALU中执行, Uniform操作在sALU中执行，或者换句话说, sALU相关的操作都是涉及sGPR, vALU相关的操作都是涉及vGPR.

#### 4.2.2: 指令选择

经由上述Divergence Analysis之后, 后面就是进行指令选择以及代码生成, 以Binary Operation(涉及到两个操作数的指令)为例

```cpp
class UniformBinFrag<SDPatternOperator Op> : PatFrag<
  (ops node:$src0, node:$src1),
  (Op $src0, $src1),
  [{ return !N->isDivergent(); }]>;

class DivergentBinFrag<SDPatternOperator Op> : PatFrag<
  (ops node:$src0, node:$src1),
  (Op $src0, $src1),
  [{ return N->isDivergent(); }]>;
```
UniformBinFrag
这是tablegen的一种语法, 后面的`[{ return !N->isDivergent(); }]>`是一种predicate, 这两个class的意思就是, `DivergentBinFrag`匹配的是Binary Operation DAG节点被标记成Divergent的节点, `UniformBinFrag`意思相反

```
class PatGprGpr<SDPatternOperator OpNode, RVInst Inst>
    : Pat<(OpNode GPR:$rs1, GPR:$rs2), (Inst GPR:$rs1, GPR:$rs2)>;
def : PatGprGpr<UniformBinFrag<add>, ADD>;
```
这里就是普通的sGPR操作的add指令匹配的定义, 相当于只有uniform操作的add指令才会被匹配成普通的sGPR add操作

```
multiclass PatVXIBin<SDPatternOperator Op, list<RVInst> Insts,
                                        Operand optype = simm5> {

  def : Pat<(Op (XLenVT VGPR:$rs1), (XLenVT VGPR:$rs2)),
            (XLenVT (Insts[0] VGPR:$rs1, VGPR:$rs2))>;
                                        }
defm : PatVXIBin<DivergentBinFrag<add>,  [VADD_VV, VADD_VX, VADD_VI]>;

```
这里就是向量指令的vGPR操作的add指令匹配的定义, 相当于只有divergent操作的add指令才会被匹配成普通的vGPR add操作

---
以下是代码示例:

```OpenCL
kernel void test(__global int *a, __local int *b) {
  a[2] = a[0] + a[1]; // sALU operation
  b[2] = b[0] + b[1]; // vALU operation
}
```

最后拿到的汇编码如下

```as
test:
	addi	sp, sp, 4
	sw	ra, -4(sp)
	lw	t0, 0(a0)
	lw	t1, 0(t0)
	lw	t2, 4(t0)
	lw	s1, 4(a0)
	add	t1, t2, t1 # sALU operation
	sw	t1, 8(t0)
	vmv.v.x	v0, s1
	vlw12.v	v1, 0(v0)
	vlw12.v	v2, 4(v0)
	vadd.vv	v1, v2, v1 # vALU operation
	vsw12.v	v1, 8(v0)
	lw	ra, -4(sp)
	addi	sp, sp, -4
	ret
```

### 4.3: 总结

以上只是涉及到指令选择以及代码生成的一些核心概念，有些知识必须结合承影架构设计来理解，当然核心都是LLVM的知识点，涉及到的一些文件有

* [RISCVIselLowering.cpp](llvm/lib/Target/RISCV/RISCVISelLowering.cpp)
* [VentusInstrInfoV.td](llvm/lib/Target/RISCV/VentusInstrInfoV.td)








