/**
 * @file f203_decrypt_g4_chain_ntt_entry.cpp
 * @brief Decrypt 多 launch 调试路径 Launch-2：NTT(u) → su_dot → pad ŵ。
 *
 * 与 INTT 分 launch，避免 MIX CrossCore flag 与下一段冲突。
 * 生产路径为 1-kernel fused（同 launch 内 NTT+su_dot+INTT+尾）。
 * golden I/O：本段输出中间 ŵ / wPadded，不写 m。
 */
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_impl.hpp"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_su_dot_impl.hpp"
#include "kernel_operator.h"

#ifdef ASCENDC_CPU_DEBUG
/** CPU 调试可改 mixPass（0/1/3）；生产默认 3=全量 Stage1–3。 */
volatile int g_f203_decrypt_g4_chain_ntt_mix_pass = 3;
#endif

/**
 * @param uGm/sHatGm 输入；@param uHatGm/wHatGm/wPaddedGm 中间输出；@param nttWsGm NTT workspace
 */
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(GM_ADDR uGm, GM_ADDR sHatGm, GM_ADDR uHatGm,
                                                                 GM_ADDR wHatGm, GM_ADDR wPaddedGm,
                                                                 GM_ADDR nttWsGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());

    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_ntt_mix_pass;
#else
    tiling.mixPass = 3;
#endif

    /* û ← NTT(u')：AIC/AIV 均进入 ntt_u_impl */
    decrypt_g4::ntt_u_impl(uHatGm, uGm, nttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    /* ŵ ← ⟨ŝ,û⟩ + pad：仅 AIV0 */
    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_g4::su_dot_impl(sHatGm, uHatGm, wHatGm);
        AscendC::PipeBarrier<PIPE_ALL>();
        decrypt_g4::pad_w_hat_for_intt(wPaddedGm, wHatGm);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}
