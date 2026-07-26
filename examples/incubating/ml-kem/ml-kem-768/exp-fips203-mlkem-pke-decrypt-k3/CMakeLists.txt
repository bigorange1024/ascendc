# exp-fips203-mlkem-pke-decrypt-k3
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)
# 向上查找仓库根（兼容 ml-kem/ml-kem-*/ 嵌套）
get_filename_component(_ASCENDC_WALK "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(REPO_ROOT "")
while(NOT REPO_ROOT)
  if(EXISTS "${_ASCENDC_WALK}/AGENTS.md" AND IS_DIRECTORY "${_ASCENDC_WALK}/scripts")
    set(REPO_ROOT "${_ASCENDC_WALK}")
  else()
    get_filename_component(_ASCENDC_PARENT "${_ASCENDC_WALK}/.." ABSOLUTE)
    if(_ASCENDC_PARENT STREQUAL _ASCENDC_WALK)
      message(FATAL_ERROR "cannot locate ascendc repo root from ${CMAKE_CURRENT_LIST_DIR}")
    endif()
    set(_ASCENDC_WALK "${_ASCENDC_PARENT}")
  endif()
endwhile()

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(G4_FULL_INC "${TEST_ROOT}/compute/g4_full")
set(UNPACK_INC "${TEST_ROOT}/unpack")
set(PREP_DECODE_INC "${TEST_ROOT}/prep/decode_dk")
set(NTT_U_INC "${TEST_ROOT}/compute/ntt_u")
set(INTT_W_INC "${TEST_ROOT}/compute/intt_w")
set(ALG11_INC "${TEST_ROOT}/compute/alg11")
set(SU_DOT_INC "${TEST_ROOT}/compute/su_dot")
set(FINAL_INC "${TEST_ROOT}/compute/final")
set(TAIL_C1_INC "${TEST_ROOT}/compute/compress1_byteencode1")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_STAGE1_SPLIT "1" CACHE STRING "NTT Stage1 split")
set(F203_STAGE3_MOD "0" CACHE STRING "NTT Stage3 mod")
set(ALG11_IMPL "1" CACHE STRING "su_dot vector")
set(ALG11_VEC_VARIANT "2" CACHE STRING "alg11 deinterleave")
set(ALG11_VEC_OPTS "1" CACHE STRING "alg11 vec opts")
set(ALG11_MEM_OPS "1" CACHE STRING "alg11 ROM load")
set(COMPRESS_1_VEC "1" CACHE STRING "Compress1: 0=scalar Barrett 1=vector Barrett (default)")
set(DECOMPRESS_D_VEC "1" CACHE STRING "prep Decompress: 0=scalar 1=vector (default)")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES
    ${G4_FULL_INC}/f203_decrypt_device_fused_entry.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main_decrypt.cpp ${TEST_ROOT}/main_decrypt_g4_run.cpp)

set(_INCLUDES ${TEST_ROOT} ${G4_FULL_INC} ${UNPACK_INC} ${PREP_DECODE_INC} ${NTT_U_INC} ${INTT_W_INC} ${ALG11_INC} ${SU_DOT_INC} ${FINAL_INC} ${TAIL_C1_INC}
    ${CMAKE_INSTALL_PREFIX}/include)

target_include_directories(ascendc_kernels_bbit PRIVATE ${_INCLUDES})

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
    F203_STAGE3_MOD=${F203_STAGE3_MOD}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
    COMPRESS_1_VEC=${COMPRESS_1_VEC}
    DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC}
)

target_link_libraries(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kernels_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kernels_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kernels_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
