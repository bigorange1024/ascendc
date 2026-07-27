# exp-fips203-mlkem-pke-encrypt-k2 — ML-KEM-512 全链 Encrypt（prep + compute+tail 集成）
# KERNEL：prep entry（1）+ compute/tail（5）→ 单 device 库 ascendc_kernels_${RUN_MODE}
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
set(MERGED_KYBER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(COMPUTE_DIR "${TEST_ROOT}/compute")
set(ALG11_PROBE_DIR "${TEST_ROOT}/multiply")
# prep 子树（按 k2 B4/B6 绿块裁剪）+ SHAKE/Keccak 共享库
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(SHARED_INC "${REPO_ROOT}/library/shared")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()

# ---- compute 侧开关（与 compute+tail 探针默认一致：生产全量路径）----
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split: 0=scalar 1=bulk vec")
set(F203_STAGE3_MOD "0" CACHE STRING "Stage3 mod variant")
set(ALG11_IMPL "1" CACHE STRING "Alg11: 0=C scalar 1=vector")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Vector deinterleave: 1=scalar B1 2=gather B2")
set(ALG11_VEC_OPTS "1" CACHE STRING "Vec micro opts: 0=legacy 1=sec9 opts")
set(ALG11_MEM_OPS "1" CACHE STRING "UB load: 0=SetValue/CreateVecIndex 1=GM ROM DataCopy")
set(F203_BYTE_DECODE12_IMPL "0" CACHE STRING "ByteDecode12: 0=scalar prod 1=zero-Gather vec backup")
# ---- prep 侧开关（与 prep 探针默认一致）----
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej: 1=vec_mins (default)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "0=scalar deinterleave (default)")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "2=dual AIV 2+2 (default)")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "0=sequential SHAKE (default)")
set(F203_ALG7_XOF_504 "0" CACHE STRING "0=672B XOF (default)")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "1=single AIV CBD batch5")

if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

# 6 kernel 源：prep entry + compute/tail 五段
set(KERNEL_FILES
    ${TEST_ROOT}/f203_encrypt_prep_entry.cpp
    ${COMPUTE_DIR}/f203_encrypt_ntt_y_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_at_jp_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_intt_e1_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_l18_l19_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_alg14_pack.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit
    ${TEST_ROOT}/main.cpp
    ${TEST_ROOT}/f203_encrypt_tiling.cpp
)

target_include_directories(ascendc_kernels_bbit PRIVATE
    ${TEST_ROOT} ${COMPUTE_DIR} ${MERGED_KYBER_ROOT} ${ALG11_PROBE_DIR}
    ${PREP_AHAT} ${PREP_ALG7} ${PREP_ALG8} ${PREP_PRESAMPLE}
    ${SHAKE_XOF_INC} ${KECCAK_INC} ${SHARED_INC}
    $<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:${CMAKE_BINARY_DIR}/include/ascendc_kernels_${RUN_MODE}>)

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
    F203_STAGE3_MOD=${F203_STAGE3_MOD}
    ALG11_IMPL=${ALG11_IMPL}
    ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
    ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
    ALG11_MEM_OPS=${ALG11_MEM_OPS}
    F203_BYTE_DECODE12_IMPL=${F203_BYTE_DECODE12_IMPL}
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
    F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
    F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
    F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
    F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
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
    pthread
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
