#pragma once

#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "innerproduct_layout.h"
#include "innerproduct_tiling.h"
#include "kernel_operator.h"

namespace hat_ip {

constexpr int32_t kQ = 3329;

__aicore__ inline int32_t mod_q_i64(int64_t x)
{
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

__aicore__ inline int32_t barrett_red(int32_t x)
{
    const int32_t q = kQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g, int32_t n)
{
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

__aicore__ inline void innerproduct_scalar_halfrows(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat, int32_t pBegin,
                                                    int32_t pEnd)
{
    const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHat);
    const auto *sGm = reinterpret_cast<const __gm__ int32_t *>(sHat);
    auto *tGm = reinterpret_cast<__gm__ int32_t *>(tHat);

    constexpr int32_t n = innerproduct_tiling::kN;

    int32_t prod[innerproduct_tiling::kN];
    int64_t acc[innerproduct_tiling::kN];

    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < n; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < innerproduct_tiling::kSVec; ++j) {
            const __gm__ int32_t *aPoly = aGm + innerproduct_layout::a_hat_offset(p, j);
            const __gm__ int32_t *sPoly = sGm + innerproduct_layout::s_hat_offset(j);
            multiply_ntts_scalar(prod, aPoly, sPoly, n);
            for (int32_t c = 0; c < n; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        for (int32_t c = 0; c < n; ++c) {
            tGm[p * n + c] = mod_q_i64(acc[c]);
        }
    }
}

} // namespace hat_ip

#endif
