/**
 * @file f203_decrypt_g4_chain_ntt_entry.cpp
 * @brief 历史 G4 Launch-2：NTT(u') → su_dot → pad ŵ（与 INTT 分 launch）。
 *
 * 流水线位置：多 launch 调试；生产已并入 1-kernel fused。
 * 背景：早期 MIX CrossCore flag 与 INTT 冲突，故 NTT+su_dot 与 INTT 拆开。
 * 与 golden：门控 G2 û、G3 ŵ。
 */
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_impl.hpp"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_su_dot_impl.hpp"
#include "kernel_operator.h"

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_g4_chain_ntt_mix_pass = 3;
#endif

/**
 * NTT 链入口。
 * @param uGm       输入 u'
 * @param sHatGm    输入 ŝ
 * @param uHatGm    输出 û
 * @param wHatGm    输出 ŵ
 * @param wPaddedGm 输出 pad 后的 polyvec（供下一 launch INTT）
 * @param nttWsGm   NTT workspace（含 LUT）
 */
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(GM_ADDR uGm, GM_ADDR sHatGm, GM_ADDR uHatGm,
                                                                 GM_ADDR wHatGm, GM_ADDR wPaddedGm,
                                                                 GM_ADDR nttWsGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());

    // 锁定与 tiling 头一致的几何（禁止运行时改参绕过）
    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_ntt_mix_pass;
#else
    tiling.mixPass = 3; /* 生产全量 mixPass */
#endif

    // 行 5'：û ← NTT(u')（双 AIV + AIC）
    decrypt_g4::ntt_u_impl(uHatGm, uGm, nttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    // su_dot + pad 仅 AIV0（与 fused softSync 语义一致）
    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_g4::su_dot_impl(sHatGm, uHatGm, wHatGm);
        AscendC::PipeBarrier<PIPE_ALL>();
        decrypt_g4::pad_w_hat_for_intt(wPaddedGm, wHatGm);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}
