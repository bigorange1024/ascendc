# Alg.21 KEM Decaps — decrypt / encrypt 分库编译，避免 tiling 头重定义
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(DEC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DEC_VENDOR "${DEC_ROOT}/vendor/pke_decrypt")
set(ENC_VENDOR "${DEC_ROOT}/vendor/pke_encrypt")
get_filename_component(REPO_ROOT "${DEC_ROOT}/../.." ABSOLUTE)
set(KEM_DIR "${DEC_ROOT}/kem")

set(G4_FULL_INC "${DEC_VENDOR}/compute/g4_full")
set(UNPACK_INC "${DEC_VENDOR}/unpack")
set(PREP_DECODE_DK_INC "${DEC_VENDOR}/prep/decode_dk")
set(NTT_U_INC "${DEC_VENDOR}/compute/ntt_u")
set(INTT_W_INC "${DEC_VENDOR}/compute/intt_w")
set(ALG11_DEC_INC "${DEC_VENDOR}/compute/alg11")
set(SU_DOT_INC "${DEC_VENDOR}/compute/su_dot")
set(FINAL_INC "${DEC_VENDOR}/compute/final")

set(PREP_AHAT_INC "${ENC_VENDOR}/prep/a_hat")
set(PREP_ALG7_INC "${ENC_VENDOR}/prep/a_hat/alg7")
set(PREP_RE_INC "${ENC_VENDOR}/prep/re")
set(PREP_CBD_INC "${ENC_VENDOR}/prep/re/cbd")
set(NTT_R_INC "${ENC_VENDOR}/compute/ntt_r")
set(ALG11_ENC_INC "${ENC_VENDOR}/compute/alg11")
set(INTT_INC "${ENC_VENDOR}/compute/intt")
set(G4_INC "${ENC_VENDOR}/compute/g4")
set(PACK_INC "${ENC_VENDOR}/pack")
set(PREP_DECODE_INC "${ENC_VENDOR}/prep/decode_ek")
set(AT_R5_INC "${ENC_VENDOR}/compute/at_r5")
set(COMPUTE_FROZEN_INC "${ENC_VENDOR}/compute/frozen")
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
    set(CMAKE_INSTALL_PREFIX "${DEC_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

set(DECRYPT_KERNEL_FILES
    ${G4_FULL_INC}/f203_decrypt_g4_prep_entry.cpp
    ${G4_FULL_INC}/f203_decrypt_g4_chain_ntt_entry.cpp
    ${KEM_DIR}/f203_kem_dec_chain_intt_entry.cpp
    ${KEM_DIR}/f203_kem_dec_g_entry.cpp
)

set(ENCRYPT_KERNEL_FILES
    ${PREP_AHAT_INC}/f203_encrypt_prep_a_hat_entry.cpp
    ${PREP_RE_INC}/f203_encrypt_prep_re_entry.cpp
    ${PREP_ALG7_INC}/f203_alg7_rej_scalar.c
    ${NTT_R_INC}/f203_encrypt_ntt_r_entry.cpp
    ${PREP_DECODE_INC}/f203_encrypt_decode_t_hat_entry.cpp
    ${AT_R5_INC}/f203_encrypt_at_r5_kernel.cpp
    ${INTT_INC}/f203_encrypt_intt_entry.cpp
    ${G4_INC}/f203_encrypt_g4_noise_entry.cpp
    ${PACK_INC}/f203_encrypt_pack_entry.cpp
    ${KEM_DIR}/f203_kem_dec_pack_entry.cpp
)

set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

set(_DECRYPT_INCLUDES ${DEC_VENDOR} ${DEC_ROOT} ${KEM_DIR} ${KECCAK_INC}
    ${G4_FULL_INC} ${UNPACK_INC} ${PREP_DECODE_DK_INC} ${NTT_U_INC} ${INTT_W_INC}
    ${ALG11_DEC_INC} ${SU_DOT_INC} ${FINAL_INC})

set(_ENCRYPT_INCLUDES ${ENC_VENDOR} ${DEC_ROOT} ${KEM_DIR} ${KECCAK_INC}
    ${PREP_AHAT_INC} ${PREP_ALG7_INC} ${PREP_RE_INC} ${PREP_CBD_INC}
    ${PREP_DECODE_INC} ${NTT_R_INC} ${ALG11_ENC_INC} ${INTT_INC} ${G4_INC} ${PACK_INC}
    ${AT_R5_INC} ${COMPUTE_FROZEN_INC} ${SHAKE_XOF_INC})

if("${RUN_MODE}" STREQUAL "cpu")
    if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
        set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
    endif()
    find_package(tikicpulib REQUIRED)

    add_library(ascendc_kernels_decrypt_${RUN_MODE} SHARED ${DECRYPT_KERNEL_FILES})
    add_library(ascendc_kernels_encrypt_${RUN_MODE} SHARED ${ENCRYPT_KERNEL_FILES})

    set_source_files_properties(${PREP_ALG7_INC}/f203_alg7_rej_scalar.c PROPERTIES LANGUAGE CXX)

    foreach(_tgt ascendc_kernels_decrypt_${RUN_MODE} ascendc_kernels_encrypt_${RUN_MODE})
        target_compile_definitions(${_tgt} PRIVATE
            $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
            $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
            F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
            F203_STAGE3_MOD=${F203_STAGE3_MOD}
            ALG11_IMPL=${ALG11_IMPL}
            ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
            ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
            ALG11_MEM_OPS=${ALG11_MEM_OPS}
        )
        target_compile_options(${_tgt} PRIVATE -g -O0
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
            $<$<COMPILE_LANGUAGE:C>:-std=c11>)
        target_link_libraries(${_tgt} PUBLIC tikicpulib::${SOC_VERSION})
        install(TARGETS ${_tgt} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endforeach()

    target_compile_definitions(ascendc_kernels_encrypt_${RUN_MODE} PRIVATE
        F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
        F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
        F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
        F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
        F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
        F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
        F203_SE_VECTOR_V3=1
    )

elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
        set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
        set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    else()
        message(FATAL_ERROR "ascendc_kernel_cmake not found")
    endif()
    include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

    ascendc_library(ascendc_kernels_decrypt_${RUN_MODE} SHARED ${DECRYPT_KERNEL_FILES})
    ascendc_library(ascendc_kernels_encrypt_${RUN_MODE} SHARED ${ENCRYPT_KERNEL_FILES})

    # SIM/NPU：include 须经 ascendc_include_directories 传到 precompile 步（target_include_directories 不生效）
    ascendc_include_directories(ascendc_kernels_decrypt_${RUN_MODE} PRIVATE ${_DECRYPT_INCLUDES})
    ascendc_include_directories(ascendc_kernels_encrypt_${RUN_MODE} PRIVATE ${_ENCRYPT_INCLUDES})

    foreach(_tgt ascendc_kernels_decrypt_${RUN_MODE} ascendc_kernels_encrypt_${RUN_MODE})
        ascendc_compile_definitions(${_tgt} PRIVATE
            $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
            F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT}
            F203_STAGE3_MOD=${F203_STAGE3_MOD}
            ALG11_IMPL=${ALG11_IMPL}
            ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT}
            ALG11_VEC_OPTS=${ALG11_VEC_OPTS}
            ALG11_MEM_OPS=${ALG11_MEM_OPS}
        )
        install(TARGETS ${_tgt} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endforeach()

    ascendc_compile_definitions(ascendc_kernels_encrypt_${RUN_MODE} PRIVATE
        F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
        F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
        F203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM}
        F203_AHAT16_BATCH_SHAKE=${F203_AHAT16_BATCH_SHAKE}
        F203_ALG7_XOF_504=${F203_ALG7_XOF_504}
        F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
        F203_SE_VECTOR_V3=1
    )
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

target_include_directories(ascendc_kernels_decrypt_${RUN_MODE} PRIVATE ${_DECRYPT_INCLUDES})
target_include_directories(ascendc_kernels_encrypt_${RUN_MODE} PRIVATE ${_ENCRYPT_INCLUDES})

set(_DECAPS_HOST_SOURCES ${DEC_ROOT}/main_kem_decaps.cpp ${DEC_ROOT}/main_kem_dec_g5_run.cpp)
if(NOT "${RUN_MODE}" STREQUAL "cpu")
    list(APPEND _DECAPS_HOST_SOURCES ${ENC_VENDOR}/main_encrypt_g5_run.cpp)
endif()
add_executable(ascendc_kem_decaps_bbit ${_DECAPS_HOST_SOURCES})

set(_HOST_INCLUDES ${DEC_ROOT} ${DEC_VENDOR} ${ENC_VENDOR} ${KEM_DIR} ${KECCAK_INC}
    ${G4_FULL_INC} ${PREP_DECODE_DK_INC} ${NTT_U_INC}
    ${PREP_AHAT_INC} ${PREP_RE_INC} ${NTT_R_INC} ${INTT_INC} ${G4_INC} ${PACK_INC} ${AT_R5_INC}
    ${SHAKE_XOF_INC} ${CMAKE_INSTALL_PREFIX}/include)

target_include_directories(ascendc_kem_decaps_bbit PRIVATE ${_HOST_INCLUDES})

target_compile_options(ascendc_kem_decaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kem_decaps_bbit PRIVATE
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

target_link_libraries(ascendc_kem_decaps_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_decrypt_${RUN_MODE}
    ascendc_kernels_encrypt_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kem_decaps_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kem_decaps_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kem_decaps_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
