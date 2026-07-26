# pass-fix-f203-alg13-lines8-15-se-k4 — Alg.13 行 8–15 设备预采样
# 默认 F203_SE_VECTOR_V3（生产）；F203_SE_V25=ON 仅实验对照（更慢，禁止集成）
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT "${TEST_ROOT}/../..")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "CBD block dim (V3 锁定 P1b-single=1)")
set(F203_SE_V25 "OFF" CACHE STRING "实验 v2.5 bulk UB CBD（更慢，禁止集成）；OFF=V3 生产默认")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/f203_se_vector_entry.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if(F203_SE_V25 STREQUAL "ON")
    message(STATUS "F203_SE_VECTOR_V25 (v2.5 experimental) — NOT for KeyGen integration")
else()
    message(STATUS "F203_SE_VECTOR_V3 (production default)")
endif()

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE
    ${TEST_ROOT} ${CMAKE_INSTALL_PREFIX}/include
    ${REPO_ROOT}/library/shared/shake_xof_kernel
    ${REPO_ROOT}/library/shared/keccak_f1600_kernel)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

if(F203_SE_V25 STREQUAL "ON")
    set(_HOST_STAGE_DEF F203_SE_VECTOR_V25=1)
else()
    set(_HOST_STAGE_DEF F203_SE_VECTOR_V3=1)
endif()

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    ${_HOST_STAGE_DEF}
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
