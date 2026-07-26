#pragma once

/**
 * @file f203_mu_embed.hpp
 * @brief Alg.14 行 20：m[32] → μ_embed[256]（⌊(q+1)/2⌋·bit），**不写 v**。
 *
 * 流水线位置：pack 探针独立对拍 μ；合并 compute 时与 g4_noise 嵌入语义一致。
 * golden I/O：输入 m uint8[32]；输出 mu_embed int32[256]，元素 ∈ {0, HALF_Q}。
 * 本文件仅展开消息比特，不加到 v（行 21 由上游负责）。
 */
#include "f203_encrypt_tail_layout.h"
#include "kernel_operator.h"

namespace f203_tail {

/**
 * 设备侧：Decompress_1 / 消息比特展开为 per-coeff 嵌入量。
 * @param mUb   UB uint8[32]（已从 GM 拷入）
 * @param muOut UB int32[256] 输出；muOut[c] ∈ {0, HALF_Q}
 *
 * 索引约定：系数 c 对应字节 i=c/8、位 j=c%8（小端比特序，与 FIPS 消息编码一致）。
 */
__aicore__ inline void mu_embed_from_message_ub(AscendC::LocalTensor<uint8_t> &mUb,
                                                AscendC::LocalTensor<int32_t> &muOut)
{
    constexpr int32_t kN = F203_TAIL_N;
    constexpr int32_t kHalfQ = F203_TAIL_HALF_Q;
    // 逐系数取消息比特：bit=1 → HALF_Q，bit=0 → 0
    for (int32_t c = 0; c < kN; ++c) {
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        const uint8_t mb = mUb.GetValue(i);
        const int32_t bit = (static_cast<int32_t>(mb) >> j) & 1;
        muOut.SetValue(c, bit != 0 ? kHalfQ : 0);
    }
}

} // namespace f203_tail
