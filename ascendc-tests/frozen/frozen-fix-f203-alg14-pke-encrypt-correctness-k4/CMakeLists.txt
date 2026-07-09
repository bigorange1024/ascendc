# fix-f203-alg14-pke-encrypt-correctness-k4 — Alg.14 Encrypt 设备拼装（生产 G5；G0–G4 过渡已冻结）
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
set(INTT_INC "${TEST_ROOT}/compute/intt")
set(G4_INC "${TEST_ROOT}/compute/g4")
set(PACK_INC "${TEST_ROOT}/pack")
set(PREP_DECODE_INC "${TEST_ROOT}/prep/decode_ek")
set(AT_R5_INC "${TEST_ROOT}/compute/at_r5")
set(COMPUTE_FROZEN_INC "${TEST_ROOT}/compute/frozen")
set(FROZEN_GATES_INC "${TEST_ROOT}/frozen-gates/frozen-g4-host-scalar-tail")

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

# funckey 实验开关：ON 时 SIM/NPU 端 KERNEL_FILES 缩到 5 个 AIV-only 核（含 g4_noise），
# 用于验证家里 agent 提出的「CAModel AIV func_key ≥ 5 → 507000」病根：
#   - 默认 OFF（生产路径）：仍含全部 12 个 device kernel；
#   - ON：device_aiv.o 仅含 marker / prep_a_hat / prep_re / decode_t_hat / g4_noise 5 个 AIV-only 核，
#         g4_noise 落到 func_key=4；main_encrypt* 在 SIM 路径试 launch g4_noise，观察 ret。
#   - CPU build 不受影响（CPU 需要 g3_linear/at_r/pack 等 ICPU_RUN_KF），仍编全部。
set(F203_FUNCKEY_EXPERIMENT "0" CACHE STRING "1 = SIM 缩 AIV 核到 5 个验证 func_key 假说")

set(KERNEL_FILES
    ${TEST_ROOT}/f203_encrypt_marker_custom.cpp
    ${PREP_AHAT_INC}/f203_encrypt_prep_a_hat_entry.cpp
    ${PREP_RE_INC}/f203_encrypt_prep_re_entry.cpp
    ${PREP_ALG7_INC}/f203_alg7_rej_scalar.c
    ${NTT_R_INC}/f203_encrypt_ntt_r_entry.cpp
    ${PREP_DECODE_INC}/f203_encrypt_decode_t_hat_entry.cpp
    ${AT_R5_INC}/f203_encrypt_at_r5_kernel.cpp
    ${INTT_INC}/f203_encrypt_intt_entry.cpp
    ${G4_INC}/f203_encrypt_g4_noise_entry.cpp
    ${PACK_INC}/f203_encrypt_pack_entry.cpp
)
# 2026-06-30：G3 旧 4 核已迁入 compute/frozen/frozen-{g3_linear,at_r,t_dot_r}/（见 FROZEN.md），不参与编译。
# g4_add_e1 / g4_make_v 已迁入 frozen-gates/frozen-g4-split-kernels/；G4 噪声由 f203_encrypt_g4_noise 六参一次完成。
# P2（2026-06-30）：decode_t_hat / pack 在 SIM/NPU 走 MIX_AIC_1_2 占位，AIV-only 压到 5（marker/prep_a_hat/prep_re/g4_noise/at_r5）。

# F203_FUNCKEY_EXPERIMENT=1（历史证据；2026-06-30 后已默认走 at_r5 合并核新方案）
#   - 移除 at_r5（让 g4_noise 落到更低 func_key，对照实验用）+ g4_add_e1/make_v/pack；
#   - 结果：SIM device_aiv.o 仅剩 5 个 AIV-only 核（marker/prep_a_hat/prep_re/decode_t_hat/g4_noise），
#     g4_noise 落到 func_key=4，可独立验证 funckey 病根（详 qa/2026-06/2026-06-30-funckey…纪要）。
#   - CPU build 不受影响（CPU 不走此分支）。
if(F203_FUNCKEY_EXPERIMENT STREQUAL "1" AND NOT "${RUN_MODE}" STREQUAL "cpu")
    list(REMOVE_ITEM KERNEL_FILES
        ${AT_R5_INC}/f203_encrypt_at_r5_kernel.cpp
        ${PACK_INC}/f203_encrypt_pack_entry.cpp
    )
    message(STATUS "[funckey-exp] SIM/NPU KERNEL_FILES 缩到 5 个 AIV-only 核：marker/prep_a_hat/prep_re/decode_t_hat/g4_noise")
endif()
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
    ${NTT_R_INC} ${ALG11_INC} ${INTT_INC} ${G4_INC} ${PACK_INC} ${AT_R5_INC} ${COMPUTE_FROZEN_INC} ${FROZEN_GATES_INC}
    ${SHAKE_XOF_INC} ${KECCAK_INC} ${CMAKE_INSTALL_PREFIX}/include)

target_include_directories(ascendc_kernels_bbit PRIVATE ${_PREP_INCLUDES})

target_compile_options(ascendc_kernels_bbit PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

target_compile_definitions(ascendc_kernels_bbit PRIVATE
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
    $<$<STREQUAL:${F203_FUNCKEY_EXPERIMENT},1>:F203_FUNCKEY_EXPERIMENT>
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
