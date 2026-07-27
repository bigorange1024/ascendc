# Alg.20 KEM Encaps device-k2 — D14 k2 Encrypt（SIM 2 launch）+ prep 前段并入 H/G
#
# 工程策略（2026-07-15）：
#   1) KERNEL：本目录 kem/f203_kem_enc_prep_entry.cpp + D14 k2 compute 五段；
#   2) prep Â/CBD 头文件编译期引用活跃 D14 k2；
#   3) Host：main_kem_encaps.cpp。
#
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(KEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
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
set(ENCRYPT_ROOT "${KEM_ROOT}")
set(TEST_ROOT "${ENCRYPT_ROOT}")
set(COMPUTE_DIR "${ENCRYPT_ROOT}/compute")
set(MERGED_KYBER_ROOT "${ENCRYPT_ROOT}")
set(ALG11_PROBE_DIR "${KEM_ROOT}/multiply")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(KEM_DIR "${KEM_ROOT}/kem")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(SHARED_INC "${REPO_ROOT}/library/shared")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest" CACHE STRING "CANN path")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split")
set(F203_STAGE3_MOD "0" CACHE STRING "Stage3 mod")
set(ALG11_IMPL "1" CACHE STRING "Alg11")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 variant")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 opts")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem")
set(F203_BYTE_DECODE12_IMPL "0" CACHE STRING "ByteDecode12")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "d12")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "dual AIV")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "shake")
set(F203_ALG7_XOF_504 "0" CACHE STRING "xof")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "cbd")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${KEM_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES
    ${KEM_DIR}/f203_kem_enc_prep_entry.cpp
    ${COMPUTE_DIR}/f203_encrypt_ntt_y_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_at_jp_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_intt_e1_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_l18_l19_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_alg14_pack.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/npu_lib.cmake)
    ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${KEM_ROOT})
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${KEM_ROOT})

add_executable(ascendc_kem_encaps_bbit
    ${KEM_ROOT}/main_kem_encaps.cpp
    ${TEST_ROOT}/f203_encrypt_tiling.cpp
)

target_include_directories(ascendc_kem_encaps_bbit PRIVATE
    ${KEM_ROOT}
    ${KEM_DIR}
    ${TEST_ROOT}
    ${COMPUTE_DIR}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_ALG8}
    ${PREP_PRESAMPLE}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
    ${SHARED_INC}
    $<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:${CMAKE_BINARY_DIR}/include/ascendc_kernels_${RUN_MODE}>
)

target_compile_options(ascendc_kem_encaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kem_encaps_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
    F203_STAGE3_MOD=${F203_STAGE3_MOD}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
    F203_BYTE_DECODE12_IMPL=${F203_BYTE_DECODE12_IMPL}
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
)

target_link_libraries(ascendc_kem_encaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl pthread
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kem_encaps_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kem_encaps_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kem_encaps_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
