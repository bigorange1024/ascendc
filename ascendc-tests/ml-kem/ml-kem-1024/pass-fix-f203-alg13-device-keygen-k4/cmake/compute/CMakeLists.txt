# KeyGen compute 段 — vec mixPass=0 单内核（行 16–21），源码 vendored 于 compute/
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(REPO_ROOT "${TEST_ROOT}/../..")
set(COMPUTE_ROOT "${TEST_ROOT}/compute")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${TEST_ROOT}/out_compute" CACHE STRING "install prefix" FORCE)
endif()

set(F203_KEYGEN_EK_PKE "1" CACHE STRING "1=fuse ek||rho in mmad launch")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split variant")
set(HAT_LINE18_DOT_ONLY "0" CACHE STRING "Line18 dot only")
set(HAT_BYTE_ENCODE "1" CACHE STRING "ByteEncode12")
set(F203_PIPELINE_PROBE "0" CACHE STRING "pipeline probe")
set(HAT_ALG11_VEC "1" CACHE STRING "Alg11 vec")
set(BYTE_ENCODE12_VEC "1" CACHE STRING "ByteEncode vec")
set(BYTE_ENCODE12_SCATTER_VEC "1" CACHE STRING "ByteEncode scatter vec")
set(BYTE_ENCODE12_PREFETCH "1" CACHE STRING "ByteEncode prefetch")
set(ALG11_IMPL "1" CACHE STRING "Alg11 impl")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 vec variant")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 vec opts")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem ops")

set(KERNEL_FILES ${COMPUTE_ROOT}/mmad_custom.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/compute/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/compute/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main_compute.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE
    ${COMPUTE_ROOT}
    ${TEST_ROOT}
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
