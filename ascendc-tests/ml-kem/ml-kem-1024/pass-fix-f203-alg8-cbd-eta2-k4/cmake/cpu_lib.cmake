if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)

set(_CBD_DEFS F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM})
if(CBD_P0_SCALAR STREQUAL "ON")
    list(APPEND _CBD_DEFS F203_CBD_ETA2_P0_SCALAR=1)
endif()
if(CBD_P1A_SCALAR_IO STREQUAL "ON")
    list(APPEND _CBD_DEFS F203_CBD_ETA2_P1A_SCALAR_IO=1)
endif()

add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${TEST_ROOT})
target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})
target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    ${_CBD_DEFS}
)
target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE -g -O0 -std=c++17)
install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
