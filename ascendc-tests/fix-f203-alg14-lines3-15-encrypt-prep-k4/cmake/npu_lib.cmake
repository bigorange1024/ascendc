if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
else()
    message(FATAL_ERROR "ascendc_kernel_cmake not found under ${ASCEND_CANN_PACKAGE_PATH}")
endif()
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE
    ${TEST_ROOT}
    ${PREP_AHAT}
    ${PREP_ALG7}
    ${PREP_ALG8}
    ${PREP_PRESAMPLE}
    ${SHAKE_XOF_INC}
    ${KECCAK_INC}
)

ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
)
