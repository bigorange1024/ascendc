#pragma once

/**
 * @file f203_encrypt_at_jp_scalar.hpp
 * @brief Alg.14 行 19 NTT 域部分：û[p] ← Σ_j MultiplyNTTs(A[j,p], ŷ[j])。
 *
 * GM 布局与 prep / alg13 一致：Â 元素 A[p,j] 存于 flat(p,j,c)=(p*K+j)*N+c。
 * 故 A[j,p] 偏移 a_hat_offset(j,p)=(j*K+p)*N。
 * 双 AIV halfrows：subBlockID=0 → p∈{0,1}；subBlockID=1 → p∈{2,3}。
 */
#include "alg11_gammas.h"
#include "kernel_operator.h"

namespace encrypt_at_jp {

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

/** Alg.11 标量 basemul（可行性探针；SIM 上直接读 __gm__）。 */
#if defined(ASCENDC_CPU_DEBUG)
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g, int32_t n)
#else
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const __gm__ int32_t *f, const __gm__ int32_t *g, int32_t n)
#endif
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

__aicore__ inline uint32_t a_hat_offset_jp(int32_t j, int32_t p)
{
    return (static_cast<uint32_t>(j) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(p)) *
           static_cast<uint32_t>(kN);
}

/**
 * halfrows 内积：读 GM 全量 ŷ[0..K-1]，写 û[pBegin..pEnd)。
 * 前置：行 18 后双 AIV PipeBarrier，ŷ GM 完整。
 */
__aicore__ inline void innerproduct_halfrows_scalar(GM_ADDR aHat, GM_ADDR yHat, GM_ADDR uNtt, int32_t pBegin,
                                                    int32_t pEnd)
{
    const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHat);
    const auto *yGm = reinterpret_cast<const __gm__ int32_t *>(yHat);
    auto *uGm = reinterpret_cast<__gm__ int32_t *>(uNtt);

    int32_t prod[kN];
    int64_t acc[kN];

    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            const __gm__ int32_t *aPoly = aGm + a_hat_offset_jp(j, p);
            const __gm__ int32_t *yPoly = yGm + static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
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
 * 单 launch 融合：û 驻留 UB，供 INTT split 直连（对齐 2s1e UB 融合不变量）。
 * @param uUb 本核 halfrows，布局 [(p-pBegin)*N + c]。
 */
__aicore__ inline void innerproduct_halfrows_to_ub(GM_ADDR aHat, GM_ADDR yHat, AscendC::LocalTensor<int32_t> &uUb,
                                                   int32_t pBegin, int32_t pEnd)
{
    const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHat);
    const auto *yGm = reinterpret_cast<const __gm__ int32_t *>(yHat);

    int32_t prod[kN];
    int64_t acc[kN];

    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            const __gm__ int32_t *aPoly = aGm + a_hat_offset_jp(j, p);
            const __gm__ int32_t *yPoly = yGm + static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
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

/** 内积 UB 落盘：DataCopy(UB→GM) 供 host 对拍 u_ntt；须在 PipeBarrier<PIPE_ALL> 后调用。 */
__aicore__ inline void dump_u_ntt_halfrows_ub(GM_ADDR uNtt, AscendC::LocalTensor<int32_t> &uUb, int32_t pBegin,
                                              int32_t pEnd)
{
    AscendC::GlobalTensor<int32_t> uGm;
    uGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uNtt));
    const uint32_t dstOff = static_cast<uint32_t>(pBegin) * static_cast<uint32_t>(kN);
    const uint32_t elemCount = static_cast<uint32_t>(pEnd - pBegin) * static_cast<uint32_t>(kN);
    AscendC::DataCopy(uGm[dstOff], uUb, elemCount);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace encrypt_at_jp
