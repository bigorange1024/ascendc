/**
 * @file f203_encrypt_at_r5_ub_scalar.hpp
 * @brief at_r5 CPU 孪生：标量参考实现（与 device 同公式，仅用于 ASCENDC_CPU_DEBUG）。
 *
 * 数学：uTr[p, c] = mod_q(Σ_{j=0..kK-1} barrett(matM[j,p] *_NTT rHat[j]) at c)
 *   *_NTT = FIPS 203 Alg.11 MultiplyNTTs（pair-wise，γ_i = ζ^{(2 BitRev7(i)+1)})
 *
 * 与 at_r 孪生的区别：
 *   - 输入 aHat → matM；偏移函数 a_hat_offset_at(p,j) → mat_offset(j,p)
 *   - kPOut 4 → 5（外层 p 循环到 kP-1=4）
 *
 * gamma 表与 at_r 同：alg11_gammas.h（INNERPRODUCT_GAMMAS_F203 与 at_r5 共享）。
 */
#pragma once

#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "f203_encrypt_at_r5_layout.h"
#include "f203_encrypt_at_r5_tiling.h"
#include "kernel_operator.h"

namespace at_r5_scalar {

constexpr int32_t kQ = at_r5_tiling::kHatQ;
constexpr int32_t kN = at_r5_tiling::kN;
constexpr int32_t kK = at_r5_tiling::kK;
constexpr int32_t kP = at_r5_tiling::kP;

/** 64 位 → [0, q) 模约简（与 at_r 同）。 */
__aicore__ inline int32_t mod_q_i64(int64_t x)
{
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

/** Barrett 折半约简（与 at_r 同；输入 32 位、输出 [0, q)）。 */
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

/** Alg.11 MultiplyNTTs pair-wise（与 at_r 同）。 */
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

/**
 * CPU 孪生总入口：uTr[kP, kN] ← Σ_j matM[j, p] *_NTT rHat[j]，p ∈ [0..kP-1]。
 *
 * 设备 kernel 在 #ifdef ASCENDC_CPU_DEBUG 分支调用本函数，
 * 保证 CPU build 与 SIM/NPU 数值口径一致。
 */
__aicore__ inline void at_r5_scalar_compute(GM_ADDR matMGm, GM_ADDR rHatGm, GM_ADDR uTrGm)
{
    const auto *mGm = reinterpret_cast<const __gm__ int32_t *>(matMGm);
    const auto *rGm = reinterpret_cast<const __gm__ int32_t *>(rHatGm);
    auto *uGm = reinterpret_cast<__gm__ int32_t *>(uTrGm);

    int32_t prod[kN];
    int64_t acc[kN];

    for (int32_t p = 0; p < kP; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            const __gm__ int32_t *fPoly = mGm + at_r5_layout::mat_offset(j, p);
            const __gm__ int32_t *gPoly = rGm + at_r5_layout::r_hat_offset(j);
            multiply_ntts_scalar(prod, fPoly, gPoly, kN);
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        for (int32_t c = 0; c < kN; ++c) {
            uGm[at_r5_layout::u_tr_offset(p) + c] = mod_q_i64(acc[c]);
        }
    }
}

} // namespace at_r5_scalar

#endif // ASCENDC_CPU_DEBUG
