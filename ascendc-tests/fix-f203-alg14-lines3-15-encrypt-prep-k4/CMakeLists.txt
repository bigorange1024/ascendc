# Alg.14 Encrypt prep — Â + r/e₁/e₂（单 launch，stable prep vendored）
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT "${TEST_ROOT}/../..")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej: 1=vec_mins (default)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "0=scalar deinterleave (default)")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "2=dual AIV 8+8 (default)")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "0=sequential SHAKE (default)")
set(F203_ALG7_XOF_504 "0" CACHE STRING "0=672B XOF (default)")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "1=single AIV CBD batch9")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/f203_encrypt_prep_entry.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
)

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE
    ${TEST_ROOT}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_ALG8}
    ${PREP_PRESAMPLE}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
    ${CMAKE_INSTALL_PREFIX}/include
)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
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

install(TARGETS ascendc_kernels_bbit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
