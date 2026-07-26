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
# cpu_lib.cmake — tikicpu 内核库
if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)

add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE
    ${TEST_ROOT} ${MERGED_KYBER_ROOT} ${POLYBATCH_ROOT}
    ${REPO_ROOT}/library/shared)
target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})
target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
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
target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE -g -O0 -std=c++17)
install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
