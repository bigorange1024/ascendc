#pragma once

/**
 * @file f203_encrypt_at_jp_scalar.hpp
 * @brief CPU 孪生：Alg.14 行 18 NTT 域内积标量参考（仅 ASCENDC_CPU_DEBUG）。
 *
 * 流水线位置：tikicpu 下替代向量 basemul；与 gen_data / mlkem_ref MultiplyNTTs 同数学。
 * 不参与 SIM 生产路径；golden 对拍仍以 I/O 为准。
 */
#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "f203_encrypt_at_jp_layout.hpp"
#include "f203_encrypt_at_jp_tiling.h"
#include "kernel_operator.h"

namespace encrypt_at_jp {

/**
 * 将 int64 约化到 [0,q)。
 * @param x 任意有符号累加；@return int32 ∈ [0,3329)
 */
__aicore__ inline int32_t mod_q_i64(int64_t x)
{
    constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

/**
 * Barrett 风格约化（与设备 basemul 标量对照一致）。
 * @param x 乘积中间量；@return ∈ [0,q)
 */
__aicore__ inline int32_t barrett_red(int32_t x)
{
    constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;
    const int32_t q = kQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * FIPS MultiplyNTTs：128 对 (a0,a1)×(b0,b1) 带 γ。
 * @param h 输出；@param f,g 输入 poly；@param n 须为 256
 */
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g, int32_t n)
{
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    const int32_t pairCount = n / 2;
    for (int32_t i = 0; i < pairCount; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[i * 2];
        const int32_t a1 = f[i * 2 + 1];
        const int32_t b0 = g[i * 2];
        const int32_t b1 = g[i * 2 + 1];
        const int32_t a1b1 = barrett_red(a1 * b1);
        const int32_t c0 = barrett_red(a0 * b0 + a1b1 * gamma);
        const int32_t c1 = barrett_red(a0 * b1 + a1 * b0);
        h[i * 2] = c0;
        h[i * 2 + 1] = c1;
    }
}

/**
 * 半行内积写 GM：û[p] = Σ_j MultiplyNTTs(A[j,p], ŷ[j]) mod q。
 * @param pBegin,pEnd 本 AIV 负责的输出行半开区间（通常 0..2 或 2..4）
 */
__aicore__ inline void innerproduct_halfrows_scalar(GM_ADDR aHat, GM_ADDR yHat, GM_ADDR uNtt, int32_t pBegin,
                                                    int32_t pEnd)
{
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    constexpr int32_t kK = encrypt_at_jp_tiling::kK;
    const auto *aGm = reinterpret_cast<const int32_t *>(aHat);
    const auto *yGm = reinterpret_cast<const int32_t *>(yHat);
    auto *uGm = reinterpret_cast<int32_t *>(uNtt);

    int32_t prod[kN];
    int64_t acc[kN];

    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            const int32_t *aPoly = aGm + encrypt_at_jp_layout::a_hat_offset_jp(j, p);
            const int32_t *yPoly = yGm + encrypt_at_jp_layout::y_hat_offset(j);
            multiply_ntts_scalar(prod, aPoly, yPoly, kN);
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        for (int32_t c = 0; c < kN; ++c) {
            uGm[static_cast<uint32_t>(p) * static_cast<uint32_t>(kN) + static_cast<uint32_t>(c)] = mod_q_i64(acc[c]);
        }
    }
}

/**
 * CPU：û 写 UB（标量 SetValue，仅 tikicpu）。
 * 局部行索引相对 pBegin，供融合核 ProcessFromLocal。
 */
__aicore__ inline void innerproduct_halfrows_scalar_to_ub(GM_ADDR aHat, GM_ADDR yHat,
                                                          AscendC::LocalTensor<int32_t> &uUb, int32_t pBegin,
                                                          int32_t pEnd)
{
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    constexpr int32_t kK = encrypt_at_jp_tiling::kK;
    const auto *aGm = reinterpret_cast<const int32_t *>(aHat);
    const auto *yGm = reinterpret_cast<const int32_t *>(yHat);

    int32_t prod[kN];
    int64_t acc[kN];

    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            const int32_t *aPoly = aGm + encrypt_at_jp_layout::a_hat_offset_jp(j, p);
            const int32_t *yPoly = yGm + encrypt_at_jp_layout::y_hat_offset(j);
            multiply_ntts_scalar(prod, aPoly, yPoly, kN);
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        const uint32_t localBase = static_cast<uint32_t>(p - pBegin) * static_cast<uint32_t>(kN);
        for (int32_t c = 0; c < kN; ++c) {
            uUb.SetValue(localBase + static_cast<uint32_t>(c), mod_q_i64(acc[c]));
        }
    }
}

} // namespace encrypt_at_jp

#endif
