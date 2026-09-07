if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)

add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${TEST_ROOT} ${MERGED_KYBER_ROOT}
    ${TEST_ROOT}/../../library/shared)
target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})
# 仅保留本 toy 实际使用的故障注入宏（已删 SKEL_GATE/HEAVY/SKIPNTT/HOST_MU 空宏）
target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SKEL_OMIT_SET4=${SKEL_OMIT_SET4}
    SKEL_OMIT_SLOT0=${SKEL_OMIT_SLOT0}
    SKEL_OMIT_SET4_R2=${SKEL_OMIT_SET4_R2}
)
target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE -g -O0 -std=c++17)
install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
