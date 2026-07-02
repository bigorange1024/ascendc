if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)
add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
# f203_alg7_rej_scalar.c 为纯 C，但 tikicpulib INTERFACE 无语言过滤地传播 -std=c++17；
# 按 C++ 编译（头文件已有 extern "C"）可消除 cc1 告警，语义不变。
set_source_files_properties(${TEST_ROOT}/prep/a_hat/alg7/f203_alg7_rej_scalar.c PROPERTIES LANGUAGE CXX)
target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE
    ${TEST_ROOT} ${PREP_AHAT_INC} ${PREP_ALG7_INC} ${PREP_RE_INC} ${PREP_CBD_INC}
    ${NTT_R_INC} ${ALG11_INC} ${INTT_INC} ${G4_INC} ${PACK_INC} ${AT_R5_INC}
    ${SHAKE_XOF_INC} ${KECCAK_INC})
target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})
target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
    F203_SE_VECTOR_V3=1
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
    F203_STAGE3_MOD=${F203_STAGE3_MOD}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
)
# KERNEL_FILES 含 prep/a_hat/alg7/f203_alg7_rej_scalar.c（C）；-std=c++17 仅作用于 C++ 源，避免 cc1 告警。
target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE
    -g -O0
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
    $<$<COMPILE_LANGUAGE:C>:-std=c11>
)
install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
