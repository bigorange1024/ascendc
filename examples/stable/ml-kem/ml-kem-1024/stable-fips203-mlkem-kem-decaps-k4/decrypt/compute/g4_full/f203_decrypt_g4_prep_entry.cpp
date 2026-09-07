/**
 * @file f203_decrypt_g4_prep_entry.cpp
 * @brief Decrypt 多 launch 调试路径 Launch-1：unpack c + decode dk→ŝ。
 *
 * 对齐 FIPS 203 Alg.15 行 3–5 prep；中间态留设备 GM。
 * 2026-09-03：Decaps Phase-D **默认** Host 编排的第 1 launch（纯 AIV；无 Cube）；
 * 旧 fused：`F203_DECRYPT_FUSED=1`。
 * golden I/O：本段不写 m；仅准备 u'/v'/ŝ 供后续 NTT/su_dot。
 */
#include "f203_decrypt_decode_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_unpack_impl.hpp"
#include "kernel_operator.h"

/**
 * prep kernel：c→u'/v'，dk→ŝ。
 * @param dkGm/cGm 生产输入；@param uGm/vGm/sHatGm 中间输出
 * 前置：仅 AIV0（blockIdx==0）；AIC 空返回。
 */
extern "C" __global__ __aicore__ void f203_decrypt_g4_prep(GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm,
                                                             GM_ADDR sHatGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#endif

    /* 行 3–4：unpack；行 5：decode ŝ */
    decrypt_g4::unpack_c_impl(cGm, uGm, vGm);
    AscendC::PipeBarrier<PIPE_ALL>();
    decrypt_g4::decode_s_hat_impl(dkGm, sHatGm);
}
