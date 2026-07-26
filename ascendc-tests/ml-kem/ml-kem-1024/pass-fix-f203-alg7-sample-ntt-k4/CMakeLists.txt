# ------------------------------------------------------------------------------
# Alg.7 SampleNTT 单 poly 探针 CMake（pass-fix-f203-alg7-sample-ntt-k4）
#
# 构建产物：
#   - ascendc_kernels_${RUN_MODE}：设备核（f203_alg7_sample_ntt_entry.cpp）
#   - ascendc_kernels_bbit：Host main（读 input → 启动核 → 写 output）
#
# 关键 CACHE 开关（默认 = 生产验收）：
#   F203_ALG7_REJ_IMPL=1   向量 Mins 剔除 + Gather 交错 + 标量 compact
#   F203_ALG7_DUMP_XOF=0   跳过 xof GM dump
#   F203_ALG7_D12_GATHER=0 标量解交织（非 Gather 实验路径）
#
# 依赖：presample Keccak、library/shared/shake_xof_kernel
# 验收：bash run.sh -r cpu/sim -v Ascend910B4 → verify_result.py 对拍 golden
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)
# 向上查找仓库根（兼容 ml-kem/ml-kem-*/ 嵌套）
get_filename_component(_ASCENDC_WALK "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(REPO_ROOT "")
while(NOT REPO_ROOT)
  if(EXISTS "${_ASCENDC_WALK}/AGENTS.md" AND IS_DIRECTORY "${_ASCENDC_WALK}/scripts")
    set(REPO_ROOT "${_ASCENDC_WALK}")
  else()
    get_filename_component(_ASCENDC_PARENT "${_ASCENDC_WALK}/.." ABSOLUTE)
    if(_ASCENDC_PARENT STREQUAL _ASCENDC_WALK)
      message(FATAL_ERROR "cannot locate ascendc repo root from ${CMAKE_CURRENT_LIST_DIR}")
    endif()
    set(_ASCENDC_WALK "${_ASCENDC_PARENT}")
  endif()
endwhile()

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PRESAMPLE_INC "${TEST_ROOT}/../pass-fix-f203-alg13-lines8-15-se-k4")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej: 0=scalar 1=vec_mins(default) 2=vec_mask")
set(F203_ALG7_DUMP_XOF "0" CACHE STRING "1=dump xof GM for golden 0=skip (default)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "1=Gather deinterleave (exp) 0=scalar GetValue (default)")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(KERNEL_FILES ${TEST_ROOT}/f203_alg7_sample_ntt_entry.cpp)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_DUMP_XOF=${F203_ALG7_DUMP_XOF} F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER})

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main.cpp)

target_include_directories(ascendc_kernels_bbit PRIVATE ${TEST_ROOT} ${PRESAMPLE_INC} ${SHAKE_XOF_INC} ${KECCAK_INC}
    ${CMAKE_INSTALL_PREFIX}/include)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_DUMP_XOF=${F203_ALG7_DUMP_XOF}
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
