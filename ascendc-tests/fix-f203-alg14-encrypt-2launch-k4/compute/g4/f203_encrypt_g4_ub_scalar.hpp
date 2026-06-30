#pragma once

/**
 * @file f203_encrypt_g4_ub_scalar.hpp
 * @brief G4 CPU 标量回退：时域 u+e₁、v=tr+e₂+μ 嵌入（mod q）。
 */
#if defined(ASCENDC_CPU_DEBUG)

#include "kernel_operator.h"

namespace f203_g4 {

constexpr int32_t kQ = 3329;
constexpr int32_t kN = 256;
constexpr int32_t kK = 4;

__aicore__ inline int32_t mod_q_i64(int64_t x)
{
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

/** Alg.14 行 19–21：u ← u+e₁；v ← tr+e₂+Decompress_1(μ) 分量嵌入。 */
__aicore__ inline void add_noise_embed_scalar(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm, GM_ADDR mGm,
                                              GM_ADDR vGm)
{
    auto *u = reinterpret_cast<__gm__ int32_t *>(uGm);
    auto *e1 = reinterpret_cast<__gm__ int32_t *>(e1Gm);
    auto *tr = reinterpret_cast<__gm__ int32_t *>(trGm);
    auto *e2 = reinterpret_cast<__gm__ int32_t *>(e2Gm);
    const auto *m = reinterpret_cast<const __gm__ uint8_t *>(mGm);
    auto *v = reinterpret_cast<__gm__ int32_t *>(vGm);

    for (int32_t p = 0; p < kK; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const int64_t sum = static_cast<int64_t>(u[p * kN + c]) + static_cast<int64_t>(e1[p * kN + c]);
            u[p * kN + c] = mod_q_i64(sum);
        }
    }

    const int32_t halfQ = (kQ + 1) / 2;
    for (int32_t c = 0; c < kN; ++c) {
        int64_t val = static_cast<int64_t>(tr[c]) + static_cast<int64_t>(e2[c]);
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        if (i < 32) {
            const int32_t bit = (static_cast<int32_t>(m[i]) >> j) & 1;
            val += static_cast<int64_t>(halfQ) * static_cast<int64_t>(bit);
        }
        v[c] = mod_q_i64(val);
    }
}

} // namespace f203_g4

#endif
