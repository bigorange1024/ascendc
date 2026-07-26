/**
 * @file innerproduct_ub_scalar.hpp
 * @brief CPU 孪生（ASCENDC_CPU_DEBUG）下的标量内积与 MultiplyNTTs。
 *
 * 仅在 CPU 调试路径编译；设备/SIM 不包含本文件逻辑。
 * 语义对齐 hat_inner_product_ref.c：逐对 (a0,a1)×(b0,b1) + gamma，lazy ∑ 后 mod q。
 */
#pragma once

#if defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"
#include "innerproduct_layout.h"
#include "innerproduct_tiling.h"
#include "kernel_operator.h"

namespace hat_ip {

constexpr int32_t kQ = 3329;

/**
 * 非负剩余：x mod q，结果落在 [0, q)。
 * @param x 任意 int64 累加值
 */
__aicore__ inline int32_t mod_q_i64(int64_t x)
{
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

/**
 * 标量 Barrett 约减（与 C ref barrett_red_coeff 同系数 78/18、5039/24）。
 * @param x 乘积或线性组合，可能为负
 * @return 约减后落在 [0,q) 的系数
 */
__aicore__ inline int32_t barrett_red(int32_t x)
{
    const int32_t q = kQ;
    // 先把负值抬到非负：x + (q & (x>>31))
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    // 末步 wrap：若仍 ≥q 则减 q
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * 标量 MultiplyNTTs：N/2 个 (偶,奇) 对，h[2i]=c0, h[2i+1]=c1。
 * @param h 输出多项式 [n]
 * @param f 左多项式 Â[p,j]
 * @param g 右多项式 ŝ[j]
 * @param n 系数个数（本探针固定 256）
 */
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g, int32_t n)
{
    const int32_t pairCount = n / 2;
    for (int32_t i = 0; i < pairCount; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[i * 2];
        const int32_t a1 = f[i * 2 + 1];
        const int32_t b0 = g[i * 2];
        const int32_t b1 = g[i * 2 + 1];
        // c0 = a0*b0 + a1*b1*γ；c1 = a0*b1 + a1*b0（均经 Barrett）
        const int32_t a1b1 = barrett_red(a1 * b1);
        const int32_t c0 = barrett_red(a0 * b0 + a1b1 * gamma);
        const int32_t c1 = barrett_red(a0 * b1 + a1 * b0);
        h[i * 2] = c0;
        h[i * 2 + 1] = c1;
    }
}

/**
 * CPU 孪生全量内积：对每个输出行 p，Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) 后 mod_q。
 * a_hat 行主序 flat(p,j)=(p*K+j)*N。
 * @param aHat Â GM
 * @param sHat ŝ GM
 * @param tHat t̂ GM（写出）
 */
__aicore__ inline void innerproduct_scalar_a_hat(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat)
{
    const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHat);
    const auto *sGm = reinterpret_cast<const __gm__ int32_t *>(sHat);
    auto *tGm = reinterpret_cast<__gm__ int32_t *>(tHat);

    constexpr int32_t n = innerproduct_tiling::kN;
    constexpr int32_t pOut = innerproduct_tiling::kPOut;
    constexpr int32_t sVec = innerproduct_tiling::kSVec;

    int32_t prod[innerproduct_tiling::kN];
    int64_t acc[innerproduct_tiling::kN];

    for (int32_t p = 0; p < pOut; ++p) {
        // 清零 lazy 累加器
        for (int32_t c = 0; c < n; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < sVec; ++j) {
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
