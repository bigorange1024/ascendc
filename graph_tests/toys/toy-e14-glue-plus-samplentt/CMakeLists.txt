cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

# E14 toy：E13 Encrypt 粘合 + 真 SampleNTT(Â) 替换 u 路 stub；vendor/sample_ntt 自包含 Alg.7。
set(MERGED_KYBER_ROOT "${CMAKE_CURRENT_LIST_DIR}/.")
set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_LIBRARY_SHARED "${CMAKE_CURRENT_LIST_DIR}/../../../library/shared")
set(SHAKE_XOF_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/shake_xof_kernel")
set(KECCAK_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/keccak_f1600_kernel")
set(BASEMUL_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/basemul_scalar")
set(CBD_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/cbd_eta2")
set(COMPRESS_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/compress_d")
set(BYTEENCODE_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/byteencode_d")
set(DECOMPRESS_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/decompress_d")
set(SAMPLE_NTT_INC "${CMAKE_CURRENT_LIST_DIR}/vendor/sample_ntt")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "E11: 1=P1b-style single-poly CBD")
set(F203_COMPRESS_D "4" CACHE STRING "E11 Compress_d bit-width (default 4)")
set(COMPRESS_D_VEC "1" CACHE STRING "E11 Compress vector path (default 1)")
set(F203_BYTE_ENCODE_D "4" CACHE STRING "E11 ByteEncode_d bit-width (default 4)")
set(BYTE_ENCODE_D_VEC "1" CACHE STRING "E11 ByteEncode vector path (default 1)")
set(F203_DECOMPRESS_D "1" CACHE STRING "E11 Decompress d=1 message embed")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "SampleNTT rej: 0=scalar 1=vec_mins(default) 2=vec_mask")
set(F203_ALG7_DUMP_XOF "0" CACHE STRING "SampleNTT xof GM dump (default 0)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "SampleNTT d12 Gather (default 0)")
set(OMIT_SET4 "0" CACHE STRING "Fault inject: omit AIV Set(4) (0/1)")

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
if("${RUN_MODE}" STREQUAL "cpu" AND "${F203_ALG7_REJ_IMPL}" STREQUAL "0")
    list(APPEND KERNEL_FILES ${SAMPLE_NTT_INC}/f203_alg7_rej_scalar.c)
endif()
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
    ${REPO_LIBRARY_SHARED} ${SHAKE_XOF_INC} ${KECCAK_INC} ${BASEMUL_INC} ${CBD_INC} ${COMPRESS_INC}
    ${BYTEENCODE_INC} ${DECOMPRESS_INC} ${SAMPLE_NTT_INC})

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
    F203_COMPRESS_D=${F203_COMPRESS_D}
    COMPRESS_D_VEC=${COMPRESS_D_VEC}
    F203_BYTE_ENCODE_D=${F203_BYTE_ENCODE_D}
    BYTE_ENCODE_D_VEC=${BYTE_ENCODE_D_VEC}
    F203_DECOMPRESS_D=${F203_DECOMPRESS_D}
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_DUMP_XOF=${F203_ALG7_DUMP_XOF}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
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
