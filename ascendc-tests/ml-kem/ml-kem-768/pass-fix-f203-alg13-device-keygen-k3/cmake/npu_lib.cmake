# @probe pass-fix-f203-alg13-device-keygen-k3
# @file cmake/npu_lib.cmake
# @layer cmake
# @role CMake 片段：编译/链接 prep、compute 或 keygen 目标。 / Build wiring for `npu_lib.cmake`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends CANN ascendc cmake 模块、同目录 cpu_lib/npu_lib 片段。
# @verify cmake --build 纳入 run.sh；编译失败即暴露。

if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
else()
    message(FATAL_ERROR "ascendc_kernel_cmake not found under ${ASCEND_CANN_PACKAGE_PATH}")
endif()
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${TEST_ROOT})
