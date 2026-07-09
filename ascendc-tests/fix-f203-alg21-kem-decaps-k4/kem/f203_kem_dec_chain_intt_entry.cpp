/**
 * @file f203_kem_dec_chain_intt_entry.cpp
 * @brief Alg.21 Decaps Phase-D 尾：INTT(ŵ)→w_time，再 extract m'。
 *
 * G(m'‖h) 在独立 launch f203_kem_dec_g 中完成（跨 launch 保证 mGm 可见）。
 * 复用 vendor Decrypt g4 的 intt_w_impl / extract_m_impl；本 entry 为 KEM 探针注册符号。
 */
#include "f203_decrypt_extract_impl.hpp"
#include "f203_decrypt_intt_w_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "kernel_operator.h"

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_g4_chain_intt_mix_pass = 3;
#endif

extern "C" __global__ __aicore__ void f203_kem_dec_chain_intt(GM_ADDR vGm, GM_ADDR wPaddedGm, GM_ADDR wTimeGm,
                                                                GM_ADDR mGm, GM_ADDR inttWsGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());

    // 与 Decrypt G4 INTT 路径一致的 tiling 字段
    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_intt_mix_pass;
#else
    tiling.mixPass = 3;
#endif

    // ① INTT(ŵ) → w_time（MIX：AIC+AIV 协同）
    decrypt_g4::intt_w_impl(wTimeGm, wPaddedGm, inttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ② 仅 AIV block0：v−w → m'（Compress₁ 打包 32B）
    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_g4::extract_m_impl(vGm, wTimeGm, mGm);
    }
}
