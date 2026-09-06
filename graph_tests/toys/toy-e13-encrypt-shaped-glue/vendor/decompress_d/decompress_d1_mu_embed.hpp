/**
 * @file decompress_d1_mu_embed.hpp
 * @brief FIPS 203 Decompress_1：m[32] → μ_embed[256]（per-coeff {0, HALF_Q}）。
 *
 * 本文件在流水线中的位置：E11 L2 INTT 后、Compress 前；由 decompress_l2_ub.hpp 调用。
 * 对齐规范：与 stable Encrypt `f203_mu_embed.hpp` 同语义（LSB-first 比特序）。
 * 与 golden 的关系：须与 decompress_d1_ref.c 中 embed_message_ref 逐系数一致。
 */
#ifndef DECOMPRESS_D1_MU_EMBED_HPP
#define DECOMPRESS_D1_MU_EMBED_HPP

#include "decompress_d1_config.hpp"
#include "kernel_operator.h"

namespace decompress_d1 {

/**
 * 设备侧 Decompress_1：消息比特展开为 per-coeff 嵌入量。
 * @param mUb   UB uint8[32]（已从 GM 拷入的 μ）
 * @param muOut UB int32[256] 输出；muOut[c] ∈ {0, F203_DECOMPRESS_D1_HALF_Q}
 * 前置条件：mUb 长度 ≥32；muOut 长度 ≥256。
 */
__aicore__ inline void mu_embed_from_message_ub(AscendC::LocalTensor<uint8_t> &mUb,
                                                AscendC::LocalTensor<int32_t> &muOut)
{
    constexpr int32_t kN = static_cast<int32_t>(F203_MLKEM_N);
    constexpr int32_t kHalfQ = static_cast<int32_t>(F203_DECOMPRESS_D1_HALF_Q);
    for (int32_t c = 0; c < kN; ++c) {
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        const uint8_t mb = mUb.GetValue(i);
        const int32_t bit = (static_cast<int32_t>(mb) >> j) & 1;
        muOut.SetValue(c, bit != 0 ? kHalfQ : 0);
    }
}

/**
 * 对 half poly 做 Decompress_1 片段展开（双 AIV 各处理 128 系数）。
 * @param mUb        UB uint8[32] 完整消息
 * @param muHalfOut  UB int32[halfLen] 输出本 half 的嵌入量
 * @param coeffOffset 本 half 在整 poly 中的起始系数下标（0 或 128）
 * @param halfLen    本 half 系数个数（128）
 */
__aicore__ inline void mu_embed_half_from_message_ub(AscendC::LocalTensor<uint8_t> &mUb,
                                                     AscendC::LocalTensor<int32_t> &muHalfOut,
                                                     uint32_t coeffOffset, uint32_t halfLen)
{
    constexpr int32_t kHalfQ = static_cast<int32_t>(F203_DECOMPRESS_D1_HALF_Q);
    for (uint32_t i = 0; i < halfLen; ++i) {
        const int32_t c = static_cast<int32_t>(coeffOffset + i);
        const int32_t bi = c / 8;
        const int32_t bj = c % 8;
        const uint8_t mb = mUb.GetValue(bi);
        const int32_t bit = (static_cast<int32_t>(mb) >> bj) & 1;
        muHalfOut.SetValue(static_cast<int32_t>(i), bit != 0 ? kHalfQ : 0);
    }
}

} // namespace decompress_d1

#endif
