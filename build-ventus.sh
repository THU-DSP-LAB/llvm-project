#!/usr/bin/bash

DIR=$(cd "$(dirname "${0}")" &> /dev/null && (pwd -W 2> /dev/null || pwd))
VENTUS_BUILD_DIR=${DIR}/build
LIBCLC_BUILD_DIR=${DIR}/build-libclc
VENTUS_INSTALL_PREFIX=${DIR}/install
PROGRAMS_TOBUILD=(llvm-ventus ocl-icd libclc spike driver pocl)

# Helper function
help() {
  cat <<END

Build llvm-ventus, pocl, ocl-icd, libclc programs
Read ${DIR}/README.md to get started.

Usage: ${DIR}/$(basename ${0})
                          [--build <build programs>]
                          [--help | -h]

Options:
  --build <build programs>
    Chosen programs to build : llvm-ventus, pocl, ocl-icd, libclc
    Option format : "llvm-ventus;pocl", string are seperated by semicolon
    Default : "llvm-ventus;ocl-icd;libclc;spike;driver;pocl"
    'BUILD_TYPE' is default 'Debug' which can be changed by enviroment variable

  --help | -h
    Print this help message and exit.

END
}

# Parse command line options
while [ $# -gt 0 ]; do
  case $1 in
  -h | --help)
    help
    exit 0
    ;;

  --build)
    shift
    if [ ! -z "${1}" ];then
      PROGRAMS_TOBUILD=(${1//;/ })
    fi
    ;;

  ?*)
    echo "Invalid options: \"$1\" , try ${DIR}/$(basename ${0}) --help for help"
    exit -1
    ;;
  esac
  # Process next command-line option
  shift
done

# Need to get the pocl folder from enviroment variables
if [ -z "${POCL_DIR}" ]; then
  POCL_DIR=${DIR}/../pocl
fi
if [ ! -d "${POCL_DIR}" ]; then
  echo "pocl folder not found, please set or check!"
  echo "Default folder is set to be $(realpath ${POCL_DIR})"
  exit 1
fi
POCL_BUILD_DIR=${POCL_DIR}/build

# Get build type from env, otherwise use default value 'Debug'
if [ -z "${BUILD_TYPE}" ]; then
  BUILD_TYPE=Debug
fi

# Need to get the ventus-driver folder from enviroment variables
if [ -z "${DRIVER_DIR}" ]; then
  DRIVER_DIR=${DIR}/../ventus-driver
fi

if [ ! -d "${DRIVER_DIR}" ]; then
  echo "ventus-driver folder not found, please set or check!"
  echo "Default folder is set to be $(realpath ${DRIVER_DIR})"
  exit 1
fi

DRIVER_BUILD_DIR=${DRIVER_DIR}/build

# Need to get the ventud-driver folder from enviroment variables
if [ -z "${SPIKE_DIR}" ]; then
  SPIKE_DIR=${DIR}/../ventus-gpgpu-isa-simulator
fi

if [ ! -d "${SPIKE_DIR}" ]; then
  echo "spike folder not found, please set or check!"
  echo "Default folder is set to be $(realpath ${SPIKE_DIR})"
  exit 1
fi

SPIKE_BUILD_DIR=${SPIKE_DIR}/build

# Need to get the icd_loader folder from enviroment variables
if [ -z "${OCL_ICD_DIR}" ]; then
  OCL_ICD_DIR=${DIR}/../ocl-icd
fi

if [ ! -d "${OCL_ICD_DIR}" ]; then
  echo "ocl icd folder not found, please set or check"
  echo "Default folder is set to be $(realpath ${OCL_ICD_DIR})"
  exit 1
fi
OCL_ICD_BUILD_DIR=${OCL_ICD_DIR}/build

# Build llvm-ventus
build_ventus() {
  if [ ! -d "${VENTUS_BUILD_DIR}" ]; then
    mkdir ${VENTUS_BUILD_DIR}
  fi
  cd ${VENTUS_BUILD_DIR}
  cmake -G Ninja -B ${VENTUS_BUILD_DIR} ${DIR}/llvm \
    -DLLVM_CCACHE_BUILD=ON \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_PARALLEL_LINK_JOBS=12 \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DLLVM_ENABLE_PROJECTS="clang;lld;libclc" \
    -DLLVM_TARGETS_TO_BUILD="AMDGPU;X86;RISCV" \
    -DLLVM_TARGET_ARCH=riscv32 \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja
  ninja install
}

# Build ventus driver
build_driver() {
  mkdir ${DRIVER_BUILD_DIR} || true
  cd ${DRIVER_DIR}
  cmake -G Ninja -B ${DRIVER_BUILD_DIR} . \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DENABLE_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja -C ${DRIVER_BUILD_DIR}
  ninja -C ${DRIVER_BUILD_DIR} install
}

# Build spike simulator
build_spike() {
  # rm -rf ${SPIKE_BUILD_DIR} || true
  mkdir ${SPIKE_BUILD_DIR} ||true
  cd ${SPIKE_BUILD_DIR}
  ../configure --prefix=${VENTUS_INSTALL_PREFIX} --enable-commitlog
  make -j8
  make install
}

# Build pocl from THU
build_pocl() {
  mkdir ${POCL_BUILD_DIR} || true
  cd ${POCL_DIR}
  cmake -G Ninja -B ${POCL_BUILD_DIR} . \
    -DENABLE_HOST_CPU_DEVICES=OFF \
    -DENABLE_VENTUS=ON \
    -DENABLE_ICD=ON \
    -DDEFAULT_ENABLE_ICD=ON \
    -DENABLE_TESTS=OFF \
    -DSTATIC_LLVM=OFF \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-fuse-ld=lld" \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja -C ${POCL_BUILD_DIR}
  ninja -C ${POCL_BUILD_DIR} install
}

# Build libclc for pocl
build_libclc() {
  if [ ! -d "${LIBCLC_BUILD_DIR}" ]; then
    mkdir ${LIBCLC_BUILD_DIR}
  fi
  cd ${LIBCLC_BUILD_DIR}
  cmake -G Ninja -B ${LIBCLC_BUILD_DIR} ${DIR}/libclc \
    -DCMAKE_CLC_COMPILER=clang \
    -DCMAKE_LLAsm_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_FORCED=ON \
    -DCMAKE_LLAsm_FLAGS="-target riscv32 -mcpu=ventus-gpgpu -cl-std=CL2.0" \
    -DCMAKE_CLC_FLAGS="-target riscv32 -mcpu=ventus-gpgpu -cl-std=CL2.0 -I/work/ventus-git-not-codereview/libclc/generic/include -fno-optimize-sibling-calls" \
    -DLIBCLC_TARGETS_TO_BUILD="riscv32--" \
    -DCMAKE_CXX_FLAGS="-I ${DIR}/llvm/include/ -std=c++17" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja
  ninja install
  DstDir=${VENTUS_INSTALL_PREFIX}/share/pocl
  if [ ! -d "${DstDir}" ]; then
    mkdir -p ${DstDir}
  fi
  # TODO: make this copy process done during libclc build process?
  # cp ${LIBCLC_BUILD_DIR}/riscv32--.bc ${DstDir}/kernel-riscv32.bc
}

# Build icd_loader
build_icd_loader() {
  cd ${OCL_ICD_DIR}
  ./bootstrap
  ./configure --prefix=${VENTUS_INSTALL_PREFIX}
  make && make install
}

# Export needed path and enviroment variables
export_elements() {
  export PATH=${VENTUS_INSTALL_PREFIX}/bin:$PATH
  export LD_LIBRARY_PATH=${VENTUS_INSTALL_PREFIX}/lib:$LD_LIBRARY_PATH
  export SPIKE_SRC_DIR=${SPIKE_DIR}
  export SPIKE_TARGET_DIR=${VENTUS_INSTALL_PREFIX}
}

# When no need to build llvm-ventus, export needed elements
if [[ ! "${PROGRAMS_TOBUILD[*]}" =~ "llvm-ventus" ]];then
  export_elements
fi

# Check llvm-ventus is built or not
check_if_ventus_built() {
  if [ ! -d "${VENTUS_INSTALL_PREFIX}" ];then
    echo "Please build llvm-ventus first!"
    exit 1
  fi
}

# Check isa simulator is built or not
check_if_spike_built() {
  if [ ! -f "${VENTUS_INSTALL_PREFIX}/lib/libspike_main.so" ];then
    if [ ! -f "${SPIKE_BUILD_DIR}/lib/libspike_main.so" ];then
      echo "Please build isa-simulator first!"
      exit 1
    else
      cp ${SPIKE_BUILD_DIR}/lib/libspike_main.so ${VENTUS_INSTALL_PREFIX}/lib
    fi
  fi
}

# Check ocl-icd loader is built or not
# since pocl need ocl-icd and llvm built first
check_if_ocl_icd_built() {
  if [ ! -f "${VENTUS_INSTALL_PREFIX}/lib/libOpenCL.so" ];then
    echo "Please build ocl-icd first!"
    exit 1
  fi
}

# Process build options
for program in "${PROGRAMS_TOBUILD[@]}"
do
  if [ "${program}" == "llvm-ventus" ];then
    build_ventus
    export_elements
  elif [ "${program}" == "ocl-icd" ];then
    build_icd_loader
  elif [ "${program}" == "libclc" ];then
    check_if_ventus_built
    build_libclc
  elif [ "${program}" == "spike" ]; then
    build_spike
  elif [ "${program}" == "driver" ]; then
    check_if_spike_built
    build_driver
  elif [ "${program}" == "pocl" ]; then
    check_if_ventus_built
    check_if_ocl_icd_built
    build_pocl
  else
    echo "Invalid build options: \"${program}\" , try $0 --help for help"
    exit 1
  fi
done