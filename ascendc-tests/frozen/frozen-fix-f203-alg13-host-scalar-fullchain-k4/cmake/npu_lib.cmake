# npu_lib.cmake — SIM/NPU 内核库（ascendc_library 编译 mmad_custom.cpp）
# 宏与 cpu_lib.cmake 对齐；不含 ASCENDC_CPU_DEBUG
if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
else()
    message(FATAL_ERROR "ascendc_kernel_cmake does not exist ,please check whether the cann package is installed")
endif()
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${TEST_ROOT} ${MERGED_KYBER_ROOT}
    ${TEST_ROOT}/../../library/shared)

ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
    HAT_ALG11_VEC=${HAT_ALG11_VEC}
    HAT_LINE18_DOT_ONLY=${HAT_LINE18_DOT_ONLY}
    HAT_BYTE_ENCODE=${HAT_BYTE_ENCODE}
    F203_PIPELINE_PROBE=${F203_PIPELINE_PROBE}
    BYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC}
    BYTE_ENCODE12_SCATTER_VEC=${BYTE_ENCODE12_SCATTER_VEC}
    BYTE_ENCODE12_PREFETCH=${BYTE_ENCODE12_PREFETCH}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
)
