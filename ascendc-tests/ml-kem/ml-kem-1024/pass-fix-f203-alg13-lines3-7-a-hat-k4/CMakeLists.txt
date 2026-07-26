# Alg.13 行 3–7：16× SampleNTT 向量探针
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT "${TEST_ROOT}/../..")
set(ALG7_INC "${TEST_ROOT}/../pass-fix-f203-alg7-sample-ntt-k4")
set(PRESAMPLE_INC "${TEST_ROOT}/../pass-fix-f203-alg13-lines8-15-se-k4")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej: 1=vec_mins (default)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "0=scalar deinterleave (default)")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "2=dual AIV 8+8 (default); 1=single AIV 16 poly debug")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "0=sequential SHAKE (default); 1=batch16 (SIM ~960k tick, negative)")
set(F203_ALG7_XOF_504 "0" CACHE STRING "0=672B XOF (default); 1=504B XOF experiment")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/f203_a_hat16_entry.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER} F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE} F203_ALG7_XOF_504=${F203_ALG7_XOF_504})

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE ${TEST_ROOT} ${ALG7_INC} ${PRESAMPLE_INC} ${SHAKE_XOF_INC}
    ${KECCAK_INC} ${CMAKE_INSTALL_PREFIX}/include)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
)

target_link_libraries(ascendc_kernels_bbit PRIVATE
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
    target_link_directories(ascendc_kernels_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kernels_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}"
        "-Wl,-rpath,${_ASCENDC_SIM_LIB}"
    )
endif()

install(TARGETS ascendc_kernels_bbit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
