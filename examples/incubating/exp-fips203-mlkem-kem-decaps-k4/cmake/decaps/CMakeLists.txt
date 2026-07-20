# Alg.21 KEM Decaps incubating — 自包含 vendored Decrypt + Encrypt + kem
#
# 规格：exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.*
# 禁止：编译期依赖 ascendc-tests/ 或其它 examples/ 源码路径。
#
cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

get_filename_component(KEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(REPO_ROOT "${KEM_ROOT}/../../.." ABSOLUTE)

# Encrypt：本目录 prep/compute（vendored）
set(STABLE_ENCRYPT_ROOT "${KEM_ROOT}")
# Decrypt：本目录 decrypt/（vendored）
set(STABLE_DECRYPT_ROOT "${KEM_ROOT}/decrypt")
set(DEC_SHIM_ROOT "${KEM_ROOT}/shim/pke_decrypt")
set(TEST_ROOT "${STABLE_ENCRYPT_ROOT}")
set(COMPUTE_DIR "${KEM_ROOT}/compute")
set(PREP_AHAT "${TEST_ROOT}/prep/ahat")
set(PREP_ALG7 "${TEST_ROOT}/prep/alg7")
set(PREP_ALG8 "${TEST_ROOT}/prep/alg8")
set(PREP_PRESAMPLE "${TEST_ROOT}/prep/presample")
set(KEM_DIR "${KEM_ROOT}/kem")
set(SHAKE_XOF_INC "${REPO_ROOT}/library/shared/shake_xof_kernel")
set(KECCAK_INC "${REPO_ROOT}/library/shared/keccak_f1600_kernel")
set(SHARED_INC "${REPO_ROOT}/library/shared")

set(G4_FULL_INC "${STABLE_DECRYPT_ROOT}/compute/g4_full")
set(UNPACK_INC "${STABLE_DECRYPT_ROOT}/unpack")
set(PREP_DECODE_INC "${STABLE_DECRYPT_ROOT}/prep/decode_dk")
set(NTT_U_INC "${STABLE_DECRYPT_ROOT}/compute/ntt_u")
set(INTT_W_INC "${STABLE_DECRYPT_ROOT}/compute/intt_w")
set(ALG11_DEC_INC "${STABLE_DECRYPT_ROOT}/compute/alg11")
set(SU_DOT_INC "${STABLE_DECRYPT_ROOT}/compute/su_dot")
set(FINAL_INC "${STABLE_DECRYPT_ROOT}/compute/final")
set(TAIL_C1_INC "${STABLE_DECRYPT_ROOT}/compute/compress1_byteencode1")

set(SHIM_G4_FULL "${DEC_SHIM_ROOT}/compute/g4_full")
set(SHIM_UNPACK "${DEC_SHIM_ROOT}/unpack")
set(SHIM_PREP_DECODE "${DEC_SHIM_ROOT}/prep/decode_dk")
set(SHIM_NTT_U "${DEC_SHIM_ROOT}/compute/ntt_u")
set(SHIM_ALG11 "${DEC_SHIM_ROOT}/compute/alg11")
set(SHIM_SU_DOT "${DEC_SHIM_ROOT}/compute/su_dot")
set(SHIM_TAIL_C1 "${DEC_SHIM_ROOT}/compute/compress1_byteencode1")

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B4" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest" CACHE STRING "CANN path")
set(F203_STAGE1_SPLIT "1" CACHE STRING "Stage1 split")
set(F203_STAGE3_MOD "0" CACHE STRING "Stage3 mod")
set(ALG11_IMPL "1" CACHE STRING "Alg11")
set(ALG11_VEC_VARIANT "2" CACHE STRING "Alg11 variant")
set(ALG11_VEC_OPTS "1" CACHE STRING "Alg11 opts")
set(ALG11_MEM_OPS "1" CACHE STRING "Alg11 mem")
set(F203_BYTE_DECODE12_IMPL "0" CACHE STRING "ByteDecode12")
set(F203_ALG7_REJ_IMPL "1" CACHE STRING "rej")
set(F203_ALG7_D12_GATHER "0" CACHE STRING "d12")
set(F203_AHAT16_BLOCK_DIM "2" CACHE STRING "dual AIV")
set(F203_AHAT16_BATCH_SHAKE "0" CACHE STRING "shake")
set(F203_ALG7_XOF_504 "0" CACHE STRING "xof")
set(F203_CBD_BLOCK_DIM "1" CACHE STRING "cbd")
set(COMPRESS_1_VEC "1" CACHE STRING "Decrypt Compress1 vec")
set(DECOMPRESS_D_VEC "1" CACHE STRING "Decrypt Decompress vec")
set(KEM_DECAPS_PHASEE_ONLY "0" CACHE STRING "1=only build phase_e host binary")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${KEM_ROOT}/out" CACHE STRING "install prefix" FORCE)
endif()

# T19i：l18_l19 用 kem/ 本地覆盖（pack 尾同核 FO）；勿编 compute/ 同名 TU
set(ENCRYPT_KERNEL_FILES
    ${KEM_DIR}/f203_kem_dec_phase_e_prep_entry.cpp
    ${KEM_DIR}/f203_kem_dec_pack_fo_entry.cpp
    ${COMPUTE_DIR}/f203_encrypt_ntt_y_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_at_jp_kernel.cpp
    ${COMPUTE_DIR}/f203_encrypt_intt_e1_kernel.cpp
    ${KEM_DIR}/f203_encrypt_l18_l19_kernel.cpp
)

set(CUSTOM_ASCEND310P_LIST "Ascend310P1" "Ascend310P3")

set(_DECRYPT_INCLUDES_STABLE
    ${STABLE_DECRYPT_ROOT} ${G4_FULL_INC} ${UNPACK_INC} ${PREP_DECODE_INC}
    ${NTT_U_INC} ${INTT_W_INC} ${ALG11_DEC_INC} ${SU_DOT_INC} ${FINAL_INC} ${TAIL_C1_INC}
    ${KEM_DIR} ${KEM_ROOT} ${KECCAK_INC} ${SHARED_INC}
)
set(_DECRYPT_INCLUDES_SHIM
    ${DEC_SHIM_ROOT} ${SHIM_G4_FULL} ${SHIM_UNPACK} ${SHIM_PREP_DECODE}
    ${SHIM_NTT_U} ${SHIM_ALG11} ${SHIM_SU_DOT} ${SHIM_TAIL_C1}
    ${KEM_DIR} ${KEM_ROOT} ${KECCAK_INC} ${SHARED_INC}
)
set(_ENCRYPT_INCLUDES
    ${TEST_ROOT} ${COMPUTE_DIR} ${PREP_AHAT} ${PREP_ALG7} ${PREP_ALG8} ${PREP_PRESAMPLE}
    ${KEM_DIR} ${KEM_ROOT} ${SHAKE_XOF_INC} ${KECCAK_INC} ${SHARED_INC}
)

set(_COMMON_KERNEL_DEFS
    $<$<BOOL:$<IN_LIST:${SOC_VERSION},${CUSTOM_ASCEND310P_LIST}>>:CUSTOM_ASCEND310P>
    $<$<STREQUAL:${RUN_MODE},cpu>:ASCENDC_CPU_DEBUG>
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
    COMPRESS_1_VEC=${COMPRESS_1_VEC}
    DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC}
)

function(_kem_set_src_includes srcs incs)
    set(_flags "")
    foreach(_d ${incs})
        list(APPEND _flags "-I${_d}")
    endforeach()
    foreach(_s ${srcs})
        set_property(SOURCE ${_s} PROPERTY INCLUDE_DIRECTORIES "${incs}")
        set_property(SOURCE ${_s} APPEND PROPERTY COMPILE_OPTIONS ${_flags})
    endforeach()
endfunction()

if("${RUN_MODE}" STREQUAL "cpu")
    set(DECRYPT_KERNEL_FILES ${G4_FULL_INC}/f203_decrypt_device_fused_entry.cpp)
    set(KERNEL_FILES ${DECRYPT_KERNEL_FILES} ${ENCRYPT_KERNEL_FILES})

    if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
        set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/tools/tikicpulib/lib/cmake)
    endif()
    find_package(tikicpulib REQUIRED)
    add_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
    _kem_set_src_includes("${DECRYPT_KERNEL_FILES}" "${_DECRYPT_INCLUDES_STABLE}")
    _kem_set_src_includes("${ENCRYPT_KERNEL_FILES}" "${_ENCRYPT_INCLUDES}")
    target_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE ${_COMMON_KERNEL_DEFS})
    target_compile_options(ascendc_kernels_${RUN_MODE} PRIVATE -g -O0 -std=c++17)
    target_link_libraries(ascendc_kernels_${RUN_MODE} PUBLIC tikicpulib::${SOC_VERSION})
    install(TARGETS ascendc_kernels_${RUN_MODE} DESTINATION ${CMAKE_INSTALL_LIBDIR})

    set(_HOST_DECRYPT_INCS ${_DECRYPT_INCLUDES_STABLE})
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    if(NOT EXISTS "${SHIM_G4_FULL}/f203_decrypt_device_fused_entry.cpp")
        message(FATAL_ERROR
            "Decrypt shim missing: ${SHIM_G4_FULL}/f203_decrypt_device_fused_entry.cpp\n"
            "Run: bash ${KEM_ROOT}/scripts/prepare_dec_shim.sh")
    endif()
    if(NOT EXISTS "${SHIM_NTT_U}/dec_aiv_func.hpp")
        message(FATAL_ERROR
            "Decrypt shim incomplete (need dec_aiv_func.hpp). Re-run prepare_dec_shim.sh")
    endif()

    set(DECRYPT_KERNEL_FILES ${SHIM_G4_FULL}/f203_decrypt_device_fused_entry.cpp)
    set(KERNEL_FILES ${DECRYPT_KERNEL_FILES} ${ENCRYPT_KERNEL_FILES})

    if(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
        set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
    elseif(EXISTS ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
        set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/tikcpp/ascendc_kernel_cmake)
    else()
        message(FATAL_ERROR "ascendc_kernel_cmake not found")
    endif()
    include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

    set(_ALL_INCLUDES ${_DECRYPT_INCLUDES_SHIM} ${_ENCRYPT_INCLUDES})

    ascendc_library(ascendc_kernels_${RUN_MODE} SHARED ${KERNEL_FILES})
    ascendc_include_directories(ascendc_kernels_${RUN_MODE} PRIVATE ${_ALL_INCLUDES})
    ascendc_compile_definitions(ascendc_kernels_${RUN_MODE} PRIVATE ${_COMMON_KERNEL_DEFS})

    set(_HOST_DECRYPT_INCS ${_DECRYPT_INCLUDES_STABLE})
else()
    message(FATAL_ERROR "invalid RUN_MODE: ${RUN_MODE}")
endif()

add_executable(ascendc_kem_decaps_bbit
    ${KEM_ROOT}/main_kem_decaps.cpp
    ${KEM_ROOT}/main_kem_decaps_phase_d_run.cpp
    ${KEM_ROOT}/main_kem_decaps_phase_e_run.cpp
    ${TEST_ROOT}/f203_encrypt_tiling.cpp
)

add_executable(ascendc_kem_decaps_phase_e_bbit
    ${KEM_ROOT}/main_kem_decaps_phase_e.cpp
    ${KEM_ROOT}/main_kem_decaps_phase_e_run.cpp
    ${TEST_ROOT}/f203_encrypt_tiling.cpp
)

set(_HOST_COMMON_OPTS
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)
set(_HOST_COMMON_DEFS
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
    COMPRESS_1_VEC=${COMPRESS_1_VEC}
    DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC}
)

set(_HOST_KERNEL_LIBS ascendc_kernels_${RUN_MODE})
set(_HOST_ACL_INCS ${CMAKE_BINARY_DIR}/include/ascendc_kernels_${RUN_MODE})

set(_HOST_LINK
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ${_HOST_KERNEL_LIBS}
    tiling_api register platform ascendalog unified_dlog dl pthread
)

set_property(SOURCE ${KEM_ROOT}/main_kem_decaps_phase_d_run.cpp PROPERTY INCLUDE_DIRECTORIES "${_HOST_DECRYPT_INCS}")
set_property(SOURCE ${KEM_ROOT}/main_kem_decaps_phase_e_run.cpp PROPERTY INCLUDE_DIRECTORIES "${_ENCRYPT_INCLUDES}")
set_property(SOURCE ${KEM_ROOT}/main_kem_decaps_phase_e.cpp PROPERTY INCLUDE_DIRECTORIES "${_ENCRYPT_INCLUDES}")
set_property(SOURCE ${TEST_ROOT}/f203_encrypt_tiling.cpp PROPERTY INCLUDE_DIRECTORIES "${_ENCRYPT_INCLUDES}")

foreach(_exe ascendc_kem_decaps_bbit ascendc_kem_decaps_phase_e_bbit)
    target_include_directories(${_exe} PRIVATE
        ${KEM_ROOT} ${KEM_DIR} ${SHARED_INC} ${KECCAK_INC}
        $<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:${_HOST_ACL_INCS}>
    )
    target_include_directories(${_exe} PRIVATE ${_ENCRYPT_INCLUDES})
    target_compile_options(${_exe} PRIVATE ${_HOST_COMMON_OPTS})
    target_compile_definitions(${_exe} PRIVATE ${_HOST_COMMON_DEFS})
    target_link_libraries(${_exe} PRIVATE ${_HOST_LINK})
    if("${RUN_MODE}" STREQUAL "sim")
        set(_ASCENDC_SIM_LIB "${ASCEND_CANN_PACKAGE_PATH}/tools/simulator/${SOC_VERSION}/lib")
        target_link_directories(${_exe} PRIVATE ${_ASCENDC_SIM_LIB})
        target_link_options(${_exe} PRIVATE
            "-Wl,-rpath-link,${_ASCENDC_SIM_LIB}" "-Wl,-rpath,${_ASCENDC_SIM_LIB}")
    endif()
endforeach()

set_property(SOURCE ${KEM_ROOT}/main_kem_decaps.cpp PROPERTY INCLUDE_DIRECTORIES
    "${KEM_ROOT};${KEM_DIR};${SHARED_INC};${KECCAK_INC}")

install(TARGETS ascendc_kem_decaps_bbit ascendc_kem_decaps_phase_e_bbit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
