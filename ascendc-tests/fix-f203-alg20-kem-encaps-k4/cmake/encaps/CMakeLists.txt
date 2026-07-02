# Alg.20 KEM Encaps — vendor Encrypt G5 + kem prep_re 融合
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(ENC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(TEST_ROOT "${ENC_ROOT}/vendor/pke_encrypt")
get_filename_component(REPO_ROOT "${ENC_ROOT}/../.." ABSOLUTE)
set(KEM_DIR "${ENC_ROOT}/kem")
set(PREP_AHAT_INC "${TEST_ROOT}/prep/a_hat")
set(PREP_ALG7_INC "${TEST_ROOT}/prep/a_hat/alg7")
set(PREP_RE_INC "${TEST_ROOT}/prep/re")
set(PREP_CBD_INC "${TEST_ROOT}/prep/re/cbd")
set(NTT_R_INC "${TEST_ROOT}/compute/ntt_r")
set(ALG11_INC "${TEST_ROOT}/compute/alg11")
set(INTT_INC "${TEST_ROOT}/compute/intt")
set(G4_INC "${TEST_ROOT}/compute/g4")
set(PACK_INC "${TEST_ROOT}/pack")
set(PREP_DECODE_INC "${TEST_ROOT}/prep/decode_ek")
set(AT_R5_INC "${TEST_ROOT}/compute/at_r5")
set(COMPUTE_FROZEN_INC "${TEST_ROOT}/compute/frozen")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_FUNCKEY_EXPERIMENT "0" CACHE STRING "funckey experiment")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "scalar deinterleave")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "a_hat blockDim")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "sequential SHAKE")
set(F203_ALG7_XOF_504 "0" CACHE STRING "672B XOF")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "CBD blockDim")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split")
set(F203_STAGE3_MOD "0" CACHE STRING "Stage3 mod")
set(ALG11_IMPL "1" CACHE STRING "Alg11")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 variant")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 opts")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest" CACHE STRING "CANN path")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${ENC_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES
    ${TEST_ROOT}/f203_encrypt_marker_custom.cpp
    ${PREP_AHAT_INC}/f203_encrypt_prep_a_hat_entry.cpp
    ${KEM_DIR}/f203_kem_enc_prep_re_entry.cpp
    ${PREP_ALG7_INC}/f203_alg7_rej_scalar.c
    ${NTT_R_INC}/f203_encrypt_ntt_r_entry.cpp
    ${PREP_DECODE_INC}/f203_encrypt_decode_t_hat_entry.cpp
    ${AT_R5_INC}/f203_encrypt_at_r5_kernel.cpp
    ${INTT_INC}/f203_encrypt_intt_entry.cpp
    ${G4_INC}/f203_encrypt_g4_noise_entry.cpp
    ${PACK_INC}/f203_encrypt_pack_entry.cpp
)
set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(${TEST_ROOT}/cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(${TEST_ROOT}/cmake/npu_lib.cmake)
    ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${ENC_ROOT})
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${KEM_DIR} ${ENC_ROOT})

add_executable(ascendc_kem_encaps_bbit ${ENC_ROOT}/main_kem_encaps.cpp ${ENC_ROOT}/main_kem_enc_g5_run.cpp)

set(_PREP_INCLUDES ${ENC_ROOT} ${TEST_ROOT} ${PREP_AHAT_INC} ${PREP_ALG7_INC} ${PREP_RE_INC} ${PREP_CBD_INC}
    ${PREP_DECODE_INC} ${NTT_R_INC} ${ALG11_INC} ${INTT_INC} ${G4_INC} ${PACK_INC} ${AT_R5_INC}
    ${COMPUTE_FROZEN_INC} ${KEM_DIR} ${SHAKE_XOF_INC} ${KECCAK_INC} ${CMAKE_INSTALL_PREFIX}/include)

target_include_directories(ascendc_kem_encaps_bbit PRIVATE ${_PREP_INCLUDES})

target_compile_options(ascendc_kem_encaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
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

target_compile_definitions(ascendc_kem_encaps_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    $<$<STREQUAL:${F203_FUNCKEY_EXPERIMENT},1>:F203_FUNCKEY_EXPERIMENT>
    SOC_VERSION="${SOC_VERSION}"
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
    F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
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

target_link_libraries(ascendc_kem_encaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kem_encaps_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kem_encaps_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kem_encaps_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
