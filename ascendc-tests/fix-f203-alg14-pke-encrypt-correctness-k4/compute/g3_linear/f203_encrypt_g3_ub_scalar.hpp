#pragma once

#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "f203_encrypt_at_r_layout.h"
#include "f203_encrypt_t_dot_r_layout.h"
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
        h[i * 2] = barrett_red(a0 * b0 + a1b1 * gamma);
        h[i * 2 + 1] = barrett_red(a0 * b1 + a1 * b0);
    }
}

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
            multiply_ntts_scalar(prod, aGm + f203_at_r_layout::a_hat_offset_at(p, j),
                                 rGm + f203_at_r_layout::r_hat_offset(j), n);
            for (int32_t c = 0; c < n; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        for (int32_t c = 0; c < n; ++c) {
            uGm[p * n + c] = mod_q_i64(acc[c]);
        }
    }
}

__aicore__ inline void t_dot_r_scalar(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat)
{
    const auto *tGm = reinterpret_cast<const __gm__ int32_t *>(tHat);
    const auto *rGm = reinterpret_cast<const __gm__ int32_t *>(rHat);
    auto *outGm = reinterpret_cast<__gm__ int32_t *>(trHat);
    constexpr int32_t n = innerproduct_tiling::kN;
    int32_t prod[innerproduct_tiling::kN];
    int64_t acc[innerproduct_tiling::kN];
    for (int32_t c = 0; c < n; ++c) {
        acc[c] = 0;
    }
    for (int32_t j = 0; j < f203_t_dot_r_layout::kK; ++j) {
        multiply_ntts_scalar(prod, tGm + f203_t_dot_r_layout::polyvec_offset(j),
                             rGm + f203_t_dot_r_layout::polyvec_offset(j), n);
        for (int32_t c = 0; c < n; ++c) {
            acc[c] += static_cast<int64_t>(prod[c]);
        }
    }
    for (int32_t c = 0; c < n; ++c) {
        outGm[c] = mod_q_i64(acc[c]);
    }
}

__aicore__ inline void g3_linear_scalar(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uHat, GM_ADDR trHat)
{
    innerproduct_scalar_at_r(aHat, rHat, uHat);
    t_dot_r_scalar(tHat, rHat, trHat);
}

} // namespace hat_ip

#endif
