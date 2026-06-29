# KeyGen 准备段单内核 — 源码 vendored 于 prep/{ahat,alg7,presample,alg8}
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(REPO_ROOT "${TEST_ROOT}/../..")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej vec_mins")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "scalar deinterleave")
set(F203_AHAT16_BLOCK_DIM "1" CACHE STRING "prep blockDim")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "sequential SHAKE")
set(F203_ALG7_XOF_504 "0" CACHE STRING "672B XOF")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "P1b-single CBD")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${TEST_ROOT}/out_prep" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/f203_keygen_prep_entry.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

message(STATUS "f203_keygen_prep SE_VECTOR_V3 blockDim=${F203_AHAT16_BLOCK_DIM}")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/cpu_lib_prep.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/npu_lib_prep.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_keygen_prep_bbit ${TEST_ROOT}/main_keygen_prep.cpp)

target_include_directories(ascendc_keygen_prep_bbit PRIVATE
    ${TEST_ROOT}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_PRESAMPLE}
    ${PREP_ALG8}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
    ${CMAKE_INSTALL_PREFIX}/include
)

target_compile_options(ascendc_keygen_prep_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_keygen_prep_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
)

target_link_libraries(ascendc_keygen_prep_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api
    register
    platform
    ascendalog
    unified_dlog
    dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_keygen_prep_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_keygen_prep_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}"
        "-Wl,-rpath,${_ASCENDC_SIM_LIB}"
    )
endif()

install(TARGETS ascendc_keygen_prep_bbit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
