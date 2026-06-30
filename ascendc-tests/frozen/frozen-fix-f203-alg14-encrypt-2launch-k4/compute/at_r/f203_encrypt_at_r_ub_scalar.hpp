#pragma once

#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "f203_encrypt_at_r_layout.h"
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

/** CPU 孪生：Âᵀ·r̂，读 A[j,p] = a_hat_offset_at(p,j)。 */
__aicore__ inline void innerproduct_scalar_at_r(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR uHat)
{
    const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHat);
    const auto *rGm = reinterpret_cast<const __gm__ int32_t *>(rHat);
    auto *uGm = reinterpret_cast<__gm__ int32_t *>(uHat);

    constexpr int32_t n = innerproduct_tiling::kN;
    constexpr int32_t pOut = innerproduct_tiling::kPOut;
    constexpr int32_t sVec = innerproduct_tiling::kSVec;

    int32_t prod[innerproduct_tiling::kN];
    int64_t acc[innerproduct_tiling::kN];

    for (int32_t p = 0; p < pOut; ++p) {
        for (int32_t c = 0; c < n; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < sVec; ++j) {
            const __gm__ int32_t *aPoly = aGm + f203_at_r_layout::a_hat_offset_at(p, j);
            const __gm__ int32_t *rPoly = rGm + f203_at_r_layout::r_hat_offset(j);
            multiply_ntts_scalar(prod, aPoly, rPoly, n);
            for (int32_t c = 0; c < n; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        for (int32_t c = 0; c < n; ++c) {
            uGm[p * n + c] = mod_q_i64(acc[c]);
        }
    }
}

} // namespace hat_ip

#endif
