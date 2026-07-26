/**
 * @file f203_decrypt_extract_impl.hpp
 * @brief 历史 G4 尾段：v' − w_time → Compress₁ → m（标量 UB 路径）。
 *
 * 流水线位置：旧 g4_full 内联；生产 fused 改用
 * decrypt_device::extract_m_compress1_byteencode1（Barrett，对齐 liboqs）。
 * 本文件 Compress₁ 用 (Q+1)/2 公式，与 golden_m 在 u=832 可能差 1 bit——
 * 仅保留作对照，非生产验收路径。
 */
#ifndef F203_DECRYPT_EXTRACT_IMPL_HPP
#define F203_DECRYPT_EXTRACT_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

namespace decrypt_g4 {

constexpr int32_t kExtractN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kExtractQ = static_cast<int32_t>(F203_DECRYPT_Q);

/** (a−b) mod q，结果 ∈[0,q)。 */
__aicore__ inline int32_t mod_q_sub(int32_t a, int32_t b)
{
    int32_t x = a - b;
    x %= kExtractQ;
    if (x < 0) {
        x += kExtractQ;
    }
    return x;
}

/**
 * 旧 Compress₁：((2x + (q+1)/2) / q) & 1。
 * 生产路径请用 Barrett 常数 1290168（见 tail_compress1）。
 */
__aicore__ inline uint32_t compress_1_u32(int32_t x)
{
    x = mod_q_sub(x, 0);
    const int32_t half = (kExtractQ + 1) / 2;
    const int32_t t = (x << 1) + half;
    return static_cast<uint32_t>((t / kExtractQ) & 1);
}

/**
 * 标量尾段：读 v/w_time，写 m[32]。
 * @param vGm / wTimeGm int32[n]；mGm uint8[32]
 */
__aicore__ inline void extract_m_impl(GM_ADDR vGm, GM_ADDR wTimeGm, GM_ADDR mGm)
{
    const auto *vIn = reinterpret_cast<const __gm__ int32_t *>(vGm);
    const auto *wIn = reinterpret_cast<const __gm__ int32_t *>(wTimeGm);
    auto *mOut = reinterpret_cast<__gm__ uint8_t *>(mGm);

    // GM → 局部：避免热路径反复 GM 标量读
    int32_t vLocal[kExtractN];
    int32_t wLocal[kExtractN];
    for (int32_t i = 0; i < kExtractN; ++i) {
        vLocal[i] = vIn[i];
        wLocal[i] = wIn[i];
    }

    uint8_t msg[F203_MSG_BYTES];
    for (uint32_t i = 0; i < F203_MSG_BYTES; ++i) {
        msg[i] = 0U;
    }
    // 逐系数：差 → Compress₁ → 拼 bit
    for (int32_t i = 0; i < kExtractN; ++i) {
        const int32_t w = mod_q_sub(vLocal[i], wLocal[i]);
        const uint32_t bit = compress_1_u32(w);
        const uint32_t byteIdx = static_cast<uint32_t>(i >> 3);
        const uint32_t bitIdx = static_cast<uint32_t>(i & 7);
        if (bit != 0U) {
            msg[byteIdx] |= static_cast<uint8_t>(1U << bitIdx);
        }
    }
    for (uint32_t i = 0; i < F203_MSG_BYTES; ++i) {
        mOut[i] = msg[i];
    }
}

} // namespace decrypt_g4

#endif
