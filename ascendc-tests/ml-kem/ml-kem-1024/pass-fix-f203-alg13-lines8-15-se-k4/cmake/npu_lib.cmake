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
# npu_lib.cmake — SIM/NPU 内核 f203_se_vector_k4（默认 F203_SE_VECTOR_V3）
if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
else()
    message(FATAL_ERROR "ascendc_kernel_cmake not found under ${ASCEND_CANN_PACKAGE_PATH}")
endif()
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

set(_SE_SHAKE_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(_SE_KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(_SE_ALG8_INC "${TEST_ROOT}/../pass-fix-f203-alg8-cbd-eta2-k4")
set(_SE_ALG7_INC "${TEST_ROOT}/../pass-fix-f203-alg7-sample-ntt-k4")

ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE
    ${TEST_ROOT} ${_SE_SHAKE_INC} ${_SE_KECCAK_INC} ${_SE_ALG8_INC} ${_SE_ALG7_INC})

if(F203_SE_V25 STREQUAL "ON")
    set(_SE_STAGE_DEF F203_SE_VECTOR_V25=1)
else()
    set(_SE_STAGE_DEF F203_SE_VECTOR_V3=1)
endif()

ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
    ${_SE_STAGE_DEF}
)
