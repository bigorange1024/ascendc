#pragma once

/**
 * @file f203_encrypt_at_jp_scalar.hpp
 * @brief CPU 孪生：Alg.14 行 18 NTT 域内积标量参考（仅 ASCENDC_CPU_DEBUG）。
 *
 * 流水线位置：Encrypt CPU 五 launch 中 `f203_encrypt_at_jp` 的标量路径；
 * SIM/向量路径用 `f203_encrypt_at_jp_vec.hpp`，本文件不参与生产 SIM。
 * 与 golden：仅辅助孪生正确性；最终密文仍对拍 `c.bin`。
 */
#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "f203_encrypt_at_jp_layout.hpp"
#include "f203_encrypt_at_jp_tiling.h"
#include "kernel_operator.h"

namespace encrypt_at_jp {

/** 将 int64 累加结果归约到 [0,q)。 */
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
 * Barrett 风格二次约减（与设备 basemul 标量参考一致）。
 * @param x 乘积或中间 int32
 * @return x mod q，落在 [0,q)
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
 * Alg.11 MultiplyNTTs 标量：h ← f ⊙ g（按对，用 γ[i]）。
 * @param h/f/g 长度 n 的 int32 缓冲；n 须为偶数（通常 256）
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
 * 半行内积写 GM：对 p∈[pBegin,pEnd) 计算 û_p = Σ_j Â_{j,p} ⊙ ŷ_j。
 * @param aHat/yHat/uNtt  GM 基址；布局见 encrypt_at_jp_layout
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
 * CPU：û 写 UB（标量 SetValue，仅 tikicpu）；localBase 相对 pBegin。
 * 用于融合路径把内积结果驻留 UB，避免再经 uNtt GM。
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
