# cpu_lib.cmake — tikicpu 内核 f203_se_vector_k4（默认 F203_SE_VECTOR_V3）
if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)

set(_SE_SHAKE_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(_SE_KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(_SE_ALG8_INC "${TEST_ROOT}/../pass-fix-f203-alg8-cbd-eta2-k4")
set(_SE_ALG7_INC "${TEST_ROOT}/../pass-fix-f203-alg7-sample-ntt-k4")

add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE
    ${TEST_ROOT} ${_SE_SHAKE_INC} ${_SE_KECCAK_INC} ${_SE_ALG8_INC} ${_SE_ALG7_INC})
target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})

if(F203_SE_V25 STREQUAL "ON")
    set(_SE_STAGE_DEF F203_SE_VECTOR_V25=1)
else()
    set(_SE_STAGE_DEF F203_SE_VECTOR_V3=1)
endif()

target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
    ${_SE_STAGE_DEF}
)
target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE -g -O0 -std=c++17)
install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
