/**
 * @file f203_kem_dec_extract_g.hpp
 * @brief Alg.21 Decaps K1 辅助：从 v−w_time 抽出 m' 到 UB，并可写 mGm。
 *
 * 背景：SIM 上 extract 写 GM 后同核标量读 mGm 算 G 可能读到脏数据（CPU 孪生无此问题）；
 * 故推荐 ExtractMLocal 把 msg 留在 UB，再直接 KemDecGFromUb。
 * Compress₁ 语义与 vendor Decrypt extract 一致（q=3329，N=256）。
 */
#pragma once

#include "f203_decrypt_layout.h"
#include "f203_kem_dec_g.hpp"
#include "kernel_operator.h"

namespace F203KemDec {

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

/** Compress₁：系数 → 1 bit（与 FIPS 203 / vendor extract 同式）。 */
__aicore__ inline uint32_t compress_1_u32(int32_t x)
{
    x = mod_q_sub(x, 0);
    const int32_t half = (kExtractQ + 1) / 2;
    const int32_t t = (x << 1) + half;
    return static_cast<uint32_t>((t / kExtractQ) & 1);
}

/**
 * v − w_time → m'：msg 留在 UB；同时写 mGm 供后续 Re-Encrypt g4_noise 使用。
 *
 * @param vGm     Decrypt 解码的 v 多项式 [256] int32
 * @param wTimeGm INTT(ŵ) 时域 [256] int32
 * @param mGm     输出 32B 明文（供 Phase-E）
 * @param msg     输出 UB 副本（供同核 G）
 */
__aicore__ inline void ExtractMLocal(GM_ADDR vGm, GM_ADDR wTimeGm, GM_ADDR mGm, uint8_t msg[kHashBytes])
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

    for (uint32_t i = 0; i < kHashBytes; ++i) {
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
    for (uint32_t i = 0; i < kHashBytes; ++i) {
        mOut[i] = msg[i];
    }
}

}  // namespace F203KemDec
