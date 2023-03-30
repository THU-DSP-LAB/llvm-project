#!/usr/bin/bash

DIR=$(cd "$(dirname "${0}")" &> /dev/null && (pwd -W 2> /dev/null || pwd))
VENTUS_BUILD_DIR=${DIR}/build
LIBCLC_BUILD_DIR=${DIR}/build-libclc
VENTUS_INSTALL_PREFIX=${DIR}/install
PROGRAMS_TOBUILD=(llvm-ventus libclc ocl-icd pocl)

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
    Default : "llvm-ventus;pocl;ocl-icd;libclc"

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
    -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_ENABLE_PROJECTS="clang;lld;libclc" \
    -DLLVM_TARGETS_TO_BUILD="AMDGPU;X86;RISCV" \
    -DLLVM_TARGET_ARCH=riscv32 \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja
  ninja install
}

# Build pocl from THU
build_pocl() {
  rm -rf ${POCL_BUILD_DIR} || true
  mkdir ${POCL_BUILD_DIR}
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
  (ninja clean ||  true) >& /tmp/ninja-clean.log
  cmake -G Ninja -B ${LIBCLC_BUILD_DIR} ${DIR}/libclc \
    -DCMAKE_LLAsm_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_FORCED=ON \
    -DCMAKE_LLAsm_FLAGS="-target riscv32 -mcpu=ventus-gpgpu" \
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
  cp ${LIBCLC_BUILD_DIR}/riscv32--.bc ${DstDir}/kernel-riscv32.bc
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
  export LD_LIBRARY_PATH=${VENTUS_INSTALL_PREFIX}/lib
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
  elif [ "${program}" == "pocl" ]; then
    check_if_ventus_built
    check_if_ocl_icd_built
    build_pocl
  elif [ "${program}" == "ocl-icd" ];then
    build_icd_loader
  elif [ "${program}" == "libclc" ];then
    check_if_ventus_built
    build_libclc
  else
    echo "Invalid build options: \"${program}\" , try $0 --help for help"
    exit 1
  fi
done