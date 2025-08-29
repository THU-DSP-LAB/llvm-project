#!/usr/bin/env bash
ORIG_ARGC=$#
DIR=$(cd "$(dirname "${0}")" &> /dev/null && (pwd -W 2> /dev/null || pwd))
VENTUS_BUILD_DIR=${DIR}/build
LIBCLC_BUILD_DIR=${DIR}/build-libclc
VENTUS_INSTALL_PREFIX=${DIR}/install
PROGRAMS_TOBUILD=(llvm ocl-icd libclc spike driver pocl rodinia test-pocl)

# Helper function
help() {
  cat <<END

Build llvm, pocl, ocl-icd, libclc, driver, spike programs.
Run the rodinia and test-pocl test suites.
Read ${DIR}/README.md to get started.

Usage: ${DIR}/$(basename ${0})
                          [--build <build programs>]
                          [--help | -h]

Options:
  --build <build programs>
    Chosen programs to build : llvm, ocl-icd, libclc, spike, driver, pocl, rodinia, test-pocl
    Option format : "llvm;pocl", string are seperated by semicolon
    Default : "llvm;ocl-icd;libclc;spike;driver;pocl;rodinia;test-pocl"
    'BUILD_TYPE' is default 'Release' which can be changed by enviroment variable

  --help | -h
    Print this help message and exit.

END
}

# Check the to be built program exits in file system or not
check_if_program_exits() {
  if [ ! -d "$1" ]; then
    echo "WARNING:*************************************************************"
    echo
    echo "$2 folder not found, please set or check!"
    echo "Default folder is set to be $(realpath $1)"
    echo
    echo "WARNING:*************************************************************"
  fi
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
check_if_program_exits $POCL_DIR "pocl"
POCL_BUILD_DIR=${POCL_DIR}/build

# Get build type from env, otherwise use default value 'Release'
if [ -z "${BUILD_TYPE}" ]; then
  BUILD_TYPE=Release
fi

# Need to get the ventus-driver folder from enviroment variables
if [ -z "${DRIVER_DIR}" ]; then
  DRIVER_DIR=${DIR}/../ventus-driver
fi
check_if_program_exits ${DRIVER_DIR} "ventus-driver"
DRIVER_BUILD_DIR=${DRIVER_DIR}/build

# Need to get the ventud-driver folder from enviroment variables
if [ -z "${SPIKE_DIR}" ]; then
  SPIKE_DIR=${DIR}/../ventus-gpgpu-isa-simulator
fi

check_if_program_exits ${SPIKE_DIR} "spike"

SPIKE_BUILD_DIR=${SPIKE_DIR}/build

# Need to get the icd_loader folder from enviroment variables
if [ -z "${OCL_ICD_DIR}" ]; then
  OCL_ICD_DIR=${DIR}/../ocl-icd
fi

check_if_program_exits ${OCL_ICD_DIR} "ocl-icd"
OCL_ICD_BUILD_DIR=${OCL_ICD_DIR}/build

# Need to get the gpu-rodinia folder from enviroment variables
if [ -z "${RODINIA_DIR}" ]; then
  RODINIA_DIR=${DIR}/../gpu-rodinia
fi

check_if_program_exits ${RODINIA_DIR} "gpu-rodinia"

# Build llvm
build_llvm() {
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
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  ninja -C ${POCL_BUILD_DIR}
  ninja -C ${POCL_BUILD_DIR} install
}

# Build libclc for pocl
build_libclc() {
  if [ -d "${LIBCLC_BUILD_DIR}" ]; then
      rm -rf "${LIBCLC_BUILD_DIR}"
  fi
  mkdir "${LIBCLC_BUILD_DIR}"
  cd ${LIBCLC_BUILD_DIR}
  cmake -G Ninja -B ${LIBCLC_BUILD_DIR} ${DIR}/libclc \
    -DCMAKE_CLC_COMPILER=clang \
    -DCMAKE_LLAsm_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_WORKS=ON \
    -DCMAKE_CLC_COMPILER_FORCED=ON \
    -DCMAKE_LLAsm_FLAGS="-target riscv32 -mcpu=ventus-gpgpu -cl-std=CL2.0 -Dcl_khr_fp64 -ffunction-sections -fdata-sections" \
    -DCMAKE_CLC_FLAGS="-target riscv32 -mcpu=ventus-gpgpu -cl-std=CL2.0 -I${DIR}/libclc/generic/include -Dcl_khr_fp64  -ffunction-sections -fdata-sections"\
    -DLIBCLC_TARGETS_TO_BUILD="riscv32--" \
    -DCMAKE_CXX_FLAGS="-I ${DIR}/llvm/include/ -std=c++17 -Dcl_khr_fp64 -ffunction-sections -fdata-sections" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
  ninja
  ninja install
  # TODO: There are bugs in linking all libclc object files now
  echo "************* Building riscv32 libclc object file ************"
  bash ${DIR}/libclc/build_riscv32clc.sh ${DIR}/libclc ${LIBCLC_BUILD_DIR} ${VENTUS_INSTALL_PREFIX} || true

  DstDir=${VENTUS_INSTALL_PREFIX}/share/pocl
  if [ ! -d "${DstDir}" ]; then
    mkdir -p ${DstDir}
  fi
}

# Build icd_loader
build_icd_loader() {
  cd ${OCL_ICD_DIR}
  ./bootstrap
  ./configure --prefix=${VENTUS_INSTALL_PREFIX}
  make && make install
}

# Test the rodinia test suit
test_rodinia() {
   cd ${RODINIA_DIR}
   make OCL_clean
   make OPENCL
}

# TODO : More test cases of the pocl will be added
test_pocl() {
   cd "${POCL_BUILD_DIR}/examples/vecadd"
   mkdir -p test_vecadd
   cd test_vecadd
   ../vecadd 
   cd "${POCL_BUILD_DIR}/examples/matadd"
   mkdir -p test_matadd
   cd test_matadd
   ../matadd
}

# Export needed path and enviroment variables
export_elements() {
  export PATH=${VENTUS_INSTALL_PREFIX}/bin:$PATH
  export LD_LIBRARY_PATH=${VENTUS_INSTALL_PREFIX}/lib:$LD_LIBRARY_PATH
  export SPIKE_SRC_DIR=${SPIKE_DIR}
  export SPIKE_TARGET_DIR=${VENTUS_INSTALL_PREFIX}
  export VENTUS_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
  export POCL_ENABLE_UNINIT=1
  export POCL_DEVICES="ventus"
  export OCL_ICD_VENDORS=${VENTUS_INSTALL_PREFIX}/lib/libpocl.so
}

# When no need to build llvm, export needed elements
if [[ ! "${PROGRAMS_TOBUILD[*]}" =~ "llvm" ]];then
  export_elements
fi

# Check llvm is built or not
check_if_ventus_built() {
  if [ ! -d "${VENTUS_INSTALL_PREFIX}" ];then
    echo "Please build llvm first!"
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

#In full compilation mode
if [ "$ORIG_ARGC" -eq 0 ]; then
  for dir in "${VENTUS_BUILD_DIR}" "${LIBCLC_BUILD_DIR}" "${POCL_BUILD_DIR}" "${DRIVER_BUILD_DIR}" "${SPIKE_BUILD_DIR}" "${OCL_ICD_BUILD_DIR}" "${VENTUS_INSTALL_PREFIX}"; do
    if [ -d "$dir" ]; then
      rm -rf "$dir"
    fi
    mkdir -p "$dir"
  done
fi

#In incremental compilation mode, process build options
for program in "${PROGRAMS_TOBUILD[@]}"
do
  if [ "${program}" == "llvm" ];then
    build_llvm
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
  elif [ "${program}" == "rodinia" ]; then
    check_if_ventus_built
    check_if_ocl_icd_built
    check_if_spike_built
    test_rodinia
  elif [ "${program}" == "test-pocl" ]; then
    check_if_ventus_built
    check_if_ocl_icd_built
    check_if_spike_built
    test_pocl
  else
    echo "Invalid build options: \"${program}\" , try $0 --help for help"
    exit 1
  fi
done
