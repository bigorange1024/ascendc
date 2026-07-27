# @probe exp-fips203-mlkem-pke-keygen-k2
# @file cmake/compute/npu_lib.cmake
# @layer cmake
# @role CMake 片段：编译/链接 prep、compute 或 keygen 目标。 / Build wiring for `npu_lib.cmake`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends CANN ascendc cmake 模块、同目录 cpu_lib/npu_lib 片段。
# @verify cmake --build 纳入 run.sh；编译失败即暴露。

# npu_lib.cmake — compute 段 SIM/NPU 内核库
if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
else()
    message(FATAL_ERROR "ascendc_kernel_cmake not found under ${ASCEND_CANN_PACKAGE_PATH}")
endif()
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${COMPUTE_ROOT} ${TEST_ROOT})

ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
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
