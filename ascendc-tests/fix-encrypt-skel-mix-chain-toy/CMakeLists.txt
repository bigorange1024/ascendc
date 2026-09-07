cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(MERGED_KYBER_ROOT "${CMAKE_CURRENT_LIST_DIR}/.")
set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/mmad_custom.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

# SKEL_GATE：1=插入 CrossCore 4↔8（默认开）；0=无 GATE（仅 SKIPNTT=0 时生效）
set(SKEL_GATE "1" CACHE STRING "Enable GATE CrossCore flag 4/8 (0/1)")
if(NOT SKEL_GATE STREQUAL "0" AND NOT SKEL_GATE STREQUAL "1")
    message(FATAL_ERROR "SKEL_GATE must be 0 or 1, got: ${SKEL_GATE}")
endif()
message(STATUS "SKEL_GATE=${SKEL_GATE}")

# SKEL_HEAVY：1=16×64×64 + 4 轮 flag 1/3；0=16×32×32 + 2 轮（接近基线）
set(SKEL_HEAVY "0" CACHE STRING "Amplify Cube load: larger MMAD + 4 flag1/3 rounds (0/1)")
if(NOT SKEL_HEAVY STREQUAL "0" AND NOT SKEL_HEAVY STREQUAL "1")
    message(FATAL_ERROR "SKEL_HEAVY must be 0 or 1, got: ${SKEL_HEAVY}")
endif()
message(STATUS "SKEL_HEAVY=${SKEL_HEAVY}")

# SKEL_SKIPNTT：1=AIC 入口即 Wait(4)（对齐 l18 skipNtt）；默认 0
set(SKEL_SKIPNTT "0" CACHE STRING "skipNtt entry AIC Wait(4) topology (0/1)")
if(NOT SKEL_SKIPNTT STREQUAL "0" AND NOT SKEL_SKIPNTT STREQUAL "1")
    message(FATAL_ERROR "SKEL_SKIPNTT must be 0 or 1, got: ${SKEL_SKIPNTT}")
endif()
message(STATUS "SKEL_SKIPNTT=${SKEL_SKIPNTT}")

# SKEL_OMIT_SET4：1=AIV 故意不 SET(4)（故障注入；仅与 SKIPNTT=1 联用）；默认 0
set(SKEL_OMIT_SET4 "0" CACHE STRING "Fault inject: omit AIV Set(4) under skipNtt (0/1)")
if(NOT SKEL_OMIT_SET4 STREQUAL "0" AND NOT SKEL_OMIT_SET4 STREQUAL "1")
    message(FATAL_ERROR "SKEL_OMIT_SET4 must be 0 or 1, got: ${SKEL_OMIT_SET4}")
endif()
message(STATUS "SKEL_OMIT_SET4=${SKEL_OMIT_SET4}")

# SKEL_HOST_MU：1=Host 折 μ 占位、设备跳过 μ-stub（默认；TASK-005）；0=设备 μ-stub
set(SKEL_HOST_MU "1" CACHE STRING "Host fold mu placeholder under skipNtt (0/1)")
if(NOT SKEL_HOST_MU STREQUAL "0" AND NOT SKEL_HOST_MU STREQUAL "1")
    message(FATAL_ERROR "SKEL_HOST_MU must be 0 or 1, got: ${SKEL_HOST_MU}")
endif()
message(STATUS "SKEL_HOST_MU=${SKEL_HOST_MU}")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE ${TEST_ROOT} ${MERGED_KYBER_ROOT}
    ${CMAKE_CURRENT_LIST_DIR}/../../library/shared)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    SKEL_GATE=${SKEL_GATE}
    SKEL_HEAVY=${SKEL_HEAVY}
    SKEL_SKIPNTT=${SKEL_SKIPNTT}
    SKEL_OMIT_SET4=${SKEL_OMIT_SET4}
    SKEL_HOST_MU=${SKEL_HOST_MU}
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
