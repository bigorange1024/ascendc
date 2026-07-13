# pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2 — CMake 根
# 编译期宏经 CACHE 注入 cpu_lib.cmake / npu_lib.cmake → ascendc_kernels_bbit
# 生产全链路：HAT_LINE18_DOT_ONLY=0 HAT_BYTE_ENCODE=1（run.sh / CMake 默认值）
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(MERGED_KYBER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
set(F203_KEYGEN_EK_PKE "1" CACHE STRING "1=fuse ek||rho in mmad launch (行21, default on)")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split: 0=scalar 1=bulk vec 2=tile vec")
# 默认 = Alg.13 行 16–20 全链路（与 run.sh 一致）；调试分段用 -D 覆盖
set(HAT_LINE18_DOT_ONLY "0" CACHE STRING "Line18: 0=dot+ê+mod (default) 1=dot only (debug)")
set(HAT_BYTE_ENCODE "1" CACHE STRING "Line19-20: 1=ByteEncode (default) 0=skip (debug)")
set(F203_PIPELINE_PROBE "0" CACHE STRING "Dev probe: 1=CPU printf sample coeffs (see pipeline_probe.hpp)")
set(BYTE_ENCODE12_VEC "1" CACHE STRING "ByteEncode12: 0=scalar 1=vector")
set(BYTE_ENCODE12_SCATTER_VEC "1" CACHE STRING "ByteEncode12 scatter: 0=scalar SetValue 1=pack+DataCopy")
set(BYTE_ENCODE12_PREFETCH "1" CACHE STRING "ByteEncode12 prefetch: 0=tile32 loop 1=whole-poly ROM+Gather once")
set(ALG11_IMPL "1" CACHE STRING "Alg11: 0=scalar 1=vector")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 variant: 1=B1 2=B2 Gather")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 vec opts: 0=legacy Barrett 1=micro-opt")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem: 0=legacy 1=ROM DataCopy")
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/mmad_custom.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

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
    HAT_ALG11_VEC=${HAT_ALG11_VEC}
    HAT_LINE18_DOT_ONLY=${HAT_LINE18_DOT_ONLY}
    HAT_BYTE_ENCODE=${HAT_BYTE_ENCODE}
    F203_PIPELINE_PROBE=${F203_PIPELINE_PROBE}
    F203_KEYGEN_EK_PKE=${F203_KEYGEN_EK_PKE}
    BYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC}
    BYTE_ENCODE12_SCATTER_VEC=${BYTE_ENCODE12_SCATTER_VEC}
    BYTE_ENCODE12_PREFETCH=${BYTE_ENCODE12_PREFETCH}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
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
