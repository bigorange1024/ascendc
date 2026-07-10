/**
 * @file f203_decrypt_g4_chain_intt_entry.cpp
 * @brief 历史 device-k4 Launch-3：INTT(ŵ) → Compress₁+ByteEncode₁ → m。
 *
 * 流水线位置：多 launch 调试；生产已并入 fused。
 * 尾段用 decrypt_device::extract_m_compress1_byteencode1（Barrett Compress₁），
 * 相对旧 G4 extract 的 (Q+1)/2 公式已对齐 liboqs / golden_m。
 * 与 golden：output/m.bin。
 */
#include "f203_decrypt_intt_w_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_tail_compress1_byteencode1.hpp"
#include "kernel_operator.h"

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_g4_chain_intt_mix_pass = 3;
#endif

/**
 * INTT + 尾段入口。
 * @param vGm       输入 v'（与 w_time 做差）
 * @param wPaddedGm 输入 pad 后的 ŵ polyvec
 * @param wTimeGm   输出时域 w
 * @param mGm       输出 m[32]
 * @param inttWsGm  INTT workspace
 */
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_intt(GM_ADDR vGm, GM_ADDR wPaddedGm, GM_ADDR wTimeGm,
                                                                   GM_ADDR mGm, GM_ADDR inttWsGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());

    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_intt_mix_pass;
#else
    tiling.mixPass = 3;
#endif

    // 行 6'：w ← INTT(ŵ_padded)
    decrypt_g4::intt_w_impl(wTimeGm, wPaddedGm, inttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    // 行 6–7：仅 AIV0 做 v−w → Compress₁ → Encode₁
    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_device::extract_m_compress1_byteencode1(vGm, wTimeGm, mGm);
    }
}
