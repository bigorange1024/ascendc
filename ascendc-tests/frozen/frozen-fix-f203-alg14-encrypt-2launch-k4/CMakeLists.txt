# stable-fips203-mlkem-pke-encrypt-k4 — Alg.14 Encrypt 设备拼装（G1–G4）
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT "${TEST_ROOT}/../..")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(PREP_AHAT_INC "${TEST_ROOT}/prep/a_hat")
set(PREP_ALG7_INC "${TEST_ROOT}/prep/a_hat/alg7")
set(PREP_RE_INC "${TEST_ROOT}/prep/re")
set(PREP_CBD_INC "${TEST_ROOT}/prep/re/cbd")
set(NTT_R_INC "${TEST_ROOT}/compute/ntt_r")
set(ALG11_INC "${TEST_ROOT}/compute/alg11")
set(G3_LINEAR_INC "${TEST_ROOT}/compute/g3_linear")
set(INTT_INC "${TEST_ROOT}/compute/intt")
set(G4_INC "${TEST_ROOT}/compute/g4")
set(PACK_INC "${TEST_ROOT}/pack")
set(PREP_DECODE_INC "${TEST_ROOT}/prep/decode_ek")
set(AT_R_INC "${TEST_ROOT}/compute/at_r")
set(T_DOT_R_INC "${TEST_ROOT}/compute/t_dot_r")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej: 1=vec_mins (default)")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "0=scalar deinterleave (default)")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "2=dual AIV 8+8 (default)")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "0=sequential SHAKE (default)")
set(F203_ALG7_XOF_504 "0" CACHE STRING "0=672B XOF (default)")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "Encrypt re CBD P1b-single=1")
set(F203_STAGE1_SPLIT "1" CACHE STRING "G2 NTT Stage1 split: 1=vec (default)")
set(F203_STAGE3_MOD "0" CACHE STRING "G2 NTT Stage3 mod: 0=Barrett (default)")
set(ALG11_IMPL "1" CACHE STRING "G3 innerproduct: 1=vector (default)")
set(ALG11_VEC_VARIANT "2" CACHE STRING "G3 deinterleave: 2=gather B2 (default)")
set(ALG11_VEC_OPTS "1" CACHE STRING "G3 vec opts: 1=sec9 (default)")
set(ALG11_MEM_OPS "1" CACHE STRING "G3 UB load: 1=GM ROM DataCopy (default)")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "install prefix" FORCE)
endif()

set(KERNEL_FILES
    ${TEST_ROOT}/f203_encrypt_marker_custom.cpp
    ${PREP_AHAT_INC}/f203_encrypt_prep_a_hat_entry.cpp
    ${PREP_RE_INC}/f203_encrypt_prep_re_entry.cpp
    ${PREP_ALG7_INC}/f203_alg7_rej_scalar.c
    ${NTT_R_INC}/f203_encrypt_ntt_r_entry.cpp
    ${PREP_DECODE_INC}/f203_encrypt_decode_t_hat_entry.cpp
    ${INTT_INC}/f203_encrypt_intt_entry.cpp
    ${G4_INC}/f203_encrypt_g4_noise_entry.cpp
    ${G4_INC}/f203_encrypt_g4_noise_ws_entry.cpp
    ${PACK_INC}/f203_encrypt_pack_entry.cpp
)
# G3 线性层：CPU 与 SIM/NPU 统一编 g3_linear.cpp（全 AIV_ONLY，唯一 include alg11 ROM 的 TU）。
#   提供已验证的 merged 核 g3_linear4（uTrOut=[û|tr̂]）；全文件 AIV_ONLY → 仅 device_aiv.o，
#   ROM 单份定义，无 dup symbol、无 auto_gen「多 MIX 降级」。弃用 at_r_mix.cpp（假 MIX，产出全 0）。
list(APPEND KERNEL_FILES ${G3_LINEAR_INC}/f203_encrypt_g3_linear.cpp)

set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kernels_bbit ${TEST_ROOT}/main_encrypt.cpp ${TEST_ROOT}/main_encrypt_g5_run.cpp)

set(_PREP_INCLUDES ${TEST_ROOT} ${PREP_AHAT_INC} ${PREP_ALG7_INC} ${PREP_RE_INC} ${PREP_CBD_INC} ${PREP_DECODE_INC}
    ${NTT_R_INC} ${ALG11_INC} ${G3_LINEAR_INC} ${INTT_INC} ${G4_INC} ${PACK_INC} ${AT_R_INC} ${T_DOT_R_INC}
    ${SHAKE_XOF_INC} ${KECCAK_INC} ${CMAKE_INSTALL_PREFIX}/include)

target_include_directories(ascendc_kernels_bbit PRIVATE ${_PREP_INCLUDES})

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    SOC_VERSION="${SOC_VERSION}"
    F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL}
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

target_link_libraries(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    tiling_api register platform ascendalog unified_dlog dl
)

if("${RUN_MODE}" STREQUAL "sim")
    set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
    target_link_directories(ascendc_kernels_bbit PRIVATE ${_ASCENDC_SIM_LIB})
    target_link_options(ascendc_kernels_bbit PRIVATE
        "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
endif()

install(TARGETS ascendc_kernels_bbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
