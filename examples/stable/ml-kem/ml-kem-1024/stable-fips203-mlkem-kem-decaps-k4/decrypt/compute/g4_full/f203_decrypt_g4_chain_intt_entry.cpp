/**
 * @file f203_decrypt_g4_chain_intt_entry.cpp
 * @brief Decrypt 多 launch 调试路径 Launch-3：INTT(ŵ) → Compress₁/ByteEncode₁ → m。
 *
 * 尾段用 decrypt_device::extract_m_compress1_byteencode1（向量 Compress + 标量 Encode）。
 * 生产路径为 1-kernel fused；本入口供分段对拍。
 * golden I/O：写出 m[32]，与 golden_m.bin 对拍。
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
 * @param vGm v'；@param wPaddedGm 已 pad 的 ŵ；@param wTimeGm INTT 输出；@param mGm m[32]
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

    decrypt_g4::intt_w_impl(wTimeGm, wPaddedGm, inttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_device::extract_m_compress1_byteencode1(vGm, wTimeGm, mGm);
    }
}
