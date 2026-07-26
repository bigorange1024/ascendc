/**
 * @file f203_decrypt_extract_impl.hpp
 * @brief Decrypt 流水线尾段（G4 标量参考）：v − w_time → Compress₁ → ByteEncode₁ → m。
 *
 * 对齐 FIPS 203 Alg.15 行 6–7。本实现为全标量；生产融合路径优先用
 * decrypt_device::extract_m_compress1_byteencode1（向量 Compress + 标量 Encode）。
 * golden I/O：输出 m[32B]，与 output/golden_m.bin 对拍。
 */
#ifndef F203_DECRYPT_EXTRACT_IMPL_HPP
#define F203_DECRYPT_EXTRACT_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

namespace decrypt_g4 {

constexpr int32_t kExtractN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kExtractQ = static_cast<int32_t>(F203_DECRYPT_Q);

/** (a−b) mod q，结果 ∈ [0,q)。 */
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
 * FIPS 203 Compress₁：系数 → {0,1}。
 * 先约化到 [0,q)，再 ⌊(2x + ⌊(q+1)/2⌋)/q⌋ & 1。
 */
__aicore__ inline uint32_t compress_1_u32(int32_t x)
{
    x = mod_q_sub(x, 0);
    const int32_t half = (kExtractQ + 1) / 2;
    const int32_t t = (x << 1) + half;
    return static_cast<uint32_t>((t / kExtractQ) & 1);
}

/**
 * Alg.15 尾：m ← ByteEncode₁(Compress₁(v'−w))。
 * @param vGm     v' [N] int32；@param wTimeGm 时域 w [N]；@param mGm 输出 m[32] uint8
 * 编码：LSB-first，每 8 个 bit 打成一字节。
 */
__aicore__ inline void extract_m_impl(GM_ADDR vGm, GM_ADDR wTimeGm, GM_ADDR mGm)
{
    const auto *vIn = reinterpret_cast<const __gm__ int32_t *>(vGm);
    const auto *wIn = reinterpret_cast<const __gm__ int32_t *>(wTimeGm);
    auto *mOut = reinterpret_cast<__gm__ uint8_t *>(mGm);

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
