# KeyGen 生产单入口 — 2 Launch（prep + mmad 融合行 21），源码 vendored 于本目录
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# examples/incubating/exp-* 比 ascendc-tests/* 多一层 incubating/
get_filename_component(_REPO_CAND "${TEST_ROOT}/../.." ABSOLUTE)
if(EXISTS "${_REPO_CAND}/library/shared")
    set(REPO_ROOT "${_REPO_CAND}")
else()
    get_filename_component(REPO_ROOT "${TEST_ROOT}/../../.." ABSOLUTE)
endif()
set(COMPUTE_ROOT "${TEST_ROOT}/compute")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
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

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${TEST_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES
    ${TEST_ROOT}/f203_keygen_prep_entry.cpp
    ${COMPUTE_ROOT}/mmad_custom.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/cpu_lib_keygen.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/npu_lib_keygen.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_keygen_bbit ${TEST_ROOT}/main_keygen.cpp)

target_include_directories(ascendc_keygen_bbit PRIVATE
    ${TEST_ROOT}
    ${COMPUTE_ROOT}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_PRESAMPLE}
    ${PREP_ALG8}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
    ${REPO_ROOT}/library/shared
    ${CMAKE_INSTALL_PREFIX}/include
)

target_compile_options(ascendc_keygen_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_keygen_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
)

target_link_libraries(ascendc_keygen_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_keygen_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_keygen_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_keygen_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
