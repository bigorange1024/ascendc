/**
 * @file f203_decrypt_g4_prep_entry.cpp
 * @brief 历史 Launch-1：unpack(c→u',v') + decode(dk→ŝ)；中间态留 GM。
 *
 * 流水线位置：多 launch 调试路径；生产已并入 f203_decrypt_device_fused。
 * Alg.15：行 3–4 ByteDecode+Decompress；行 5 ByteDecode₁₂(ŝ)。
 * 与 golden：门控 G1/G2 的 u/v/s_hat。
 *
 * 核类型：CPU debug 为 AIV-only；SIM/NPU 为 MIX，但 AIC 立即 return，
 * 仅 block0 的 AIV 执行（与 fused 中 prep 仅 AIV0 一致）。
 */
#include "f203_decrypt_decode_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_unpack_impl.hpp"
#include "kernel_operator.h"

/**
 * prep 入口。
 * @param dkGm/cGm 生产输入
 * @param uGm/vGm  写出 u'/v'（int32 平面）
 * @param sHatGm   写出 ŝ
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
    // AIC 子块不参与 prep（无 MMAD）
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#endif

    // 行 3–4：密文 → u' / v'
    decrypt_g4::unpack_c_impl(cGm, uGm, vGm);
    AscendC::PipeBarrier<PIPE_ALL>();
    // 行 5：私钥 → ŝ
    decrypt_g4::decode_s_hat_impl(dkGm, sHatGm);
}
