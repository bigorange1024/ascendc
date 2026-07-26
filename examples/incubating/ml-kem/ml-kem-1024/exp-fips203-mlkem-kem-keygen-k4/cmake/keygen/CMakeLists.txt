# Alg.19 KEM KeyGen incubating — 自包含 vendored PKE + 内嵌 Alg.16 尾（F203_KEM_KEYGEN_TAIL=1）
#
# 规格：exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.*
# 禁止：编译期依赖 pass-fix-…-device-k4 或其它探针源码路径。
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
set(TEST_ROOT "${KEM_ROOT}")
set(COMPUTE_ROOT "${KEM_ROOT}/compute")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${KEM_ROOT}/prep/alg7")
set(PREP_ALG7_STABLE "${TEST_ROOT}/prep/alg7")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(KEM_DIR "${KEM_ROOT}/kem")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest" CACHE STRING "CANN path")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "prep AIV shard count")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej vec_mins")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "scalar deinterleave")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "sequential SHAKE")
set(F203_ALG7_XOF_504 "0" CACHE STRING "672B XOF")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "CBD blockDim")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split")
set(HAT_LINE18_DOT_ONLY "0" CACHE STRING "Line18 debug")
set(HAT_BYTE_ENCODE "1" CACHE STRING "ByteEncode")
set(F203_PIPELINE_PROBE "0" CACHE STRING "probe")
set(HAT_ALG11_VEC "1" CACHE STRING "Alg11 vec")
set(BYTE_ENCODE12_VEC "1" CACHE STRING "BE12 vec")
set(BYTE_ENCODE12_SCATTER_VEC "1" CACHE STRING "BE12 scatter")
set(BYTE_ENCODE12_PREFETCH "1" CACHE STRING "BE12 prefetch")
set(ALG11_IMPL "1" CACHE STRING "Alg11 impl")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 variant")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 opts")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem")
# 生产默认：内嵌 Alg.16 尾（本用例即为 KEM KeyGen）
set(F203_KEM_KEYGEN_TAIL "1" CACHE STRING "1=内嵌 Alg.16 尾于 mmad")

set(KEM_KG_EXT_SEED "0" CACHE STRING "test-only: feed host kem_seed d||z (default off)")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${KEM_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

if(KEM_KG_EXT_SEED STREQUAL "1")
    set(_KEYGEN_PREP_ENTRY ${KEM_DIR}/f203_keygen_prep_entry_extseed.cpp)
else()
    set(_KEYGEN_PREP_ENTRY ${TEST_ROOT}/f203_keygen_prep_entry.cpp)
endif()

set(KERNEL_FILES
    ${_KEYGEN_PREP_ENTRY}
    ${COMPUTE_ROOT}/mmad_custom.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/cpu_lib_keygen.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/npu_lib_keygen.cmake)
    ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${KEM_ROOT} ${COMPUTE_ROOT})
    ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${PREP_ALG7_STABLE})
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${KEM_ROOT} ${COMPUTE_ROOT} ${PREP_ALG7_STABLE})

if("${RUN_MODE}" STREQUAL "cpu")
    target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
        KEM_KG_EXT_SEED=${KEM_KG_EXT_SEED}
        F203_KEM_KEYGEN_TAIL=${F203_KEM_KEYGEN_TAIL})
else()
    ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
        KEM_KG_EXT_SEED=${KEM_KG_EXT_SEED}
        F203_KEM_KEYGEN_TAIL=${F203_KEM_KEYGEN_TAIL})
endif()

add_executable(ascendc_kem_keygen_bbit ${KEM_ROOT}/main_kem_keygen.cpp)

target_include_directories(ascendc_kem_keygen_bbit PRIVATE
    ${KEM_ROOT}
    ${TEST_ROOT}
    ${COMPUTE_ROOT}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_ALG7_STABLE}
    ${PREP_PRESAMPLE}
    ${PREP_ALG8}
    ${KEM_DIR}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
    ${REPO_ROOT}/library/shared
    ${CMAKE_INSTALL_PREFIX}/include
)

target_compile_options(ascendc_kem_keygen_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kem_keygen_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    KEM_KG_EXT_SEED=${KEM_KG_EXT_SEED}
)

target_link_libraries(ascendc_kem_keygen_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kem_keygen_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kem_keygen_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kem_keygen_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
