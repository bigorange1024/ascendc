#pragma once

/**
 * @file f203_mu_embed.hpp
 * @brief Alg.14 行 20：m[32] → μ_embed[256]（⌊(q+1)/2⌋·bit），供 e₂+=μ 前缀折叠。
 *
 * 语义：FIPS 203 Decompress_1(m) 展开为 per-coeff 嵌入量；本文件只生成 μ，**不写 v**。
 * 融合核在 INTT 前对 e₂ GM 做 mod_q 加，使行 21 的 v←INTT(tr̂)+e₂' 与 tail 纯 pack 兼容。
 */
#include "f203_encrypt_tail_layout.h"
#include "kernel_operator.h"

namespace f203_tail {

/**
 * 设备侧：Decompress_1 / 消息比特展开为 per-coeff 嵌入量。
 * @param mUb   UB uint8[32]（已从 GM 拷入）
 * @param muOut UB int32[256] 输出；muOut[c] ∈ {0, HALF_Q}
 */
__aicore__ inline void mu_embed_from_message_ub(AscendC::LocalTensor<uint8_t> &mUb,
                                                AscendC::LocalTensor<int32_t> &muOut)
{
    constexpr int32_t kN = F203_TAIL_N;
    constexpr int32_t kHalfQ = F203_TAIL_HALF_Q;
    // 按 FIPS 203 消息比特序：coeff c 对应 m[c//8] 的第 (c%8) 位
    for (int32_t c = 0; c < kN; ++c) {
        const int32_t i = c / 8;   // 消息字节下标
        const int32_t j = c % 8;   // 字节内 bit 序（LSB-first）
        const uint8_t mb = mUb.GetValue(i);
        const int32_t bit = (static_cast<int32_t>(mb) >> j) & 1;
        // bit=1 → HALF_Q=(q+1)/2；bit=0 → 0（与 golden mu_embed_from_m 一致）
        muOut.SetValue(c, bit != 0 ? kHalfQ : 0);
    }
}

} // namespace f203_tail
