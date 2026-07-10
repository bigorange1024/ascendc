#pragma once

/**
 * @file f203_encrypt_at_jp.hpp
 * @brief Alg.14 行 18 NTT 域内积统一入口：CPU 标量 / SIM·NPU 向量（û 可驻留 UB）。
 *
 * 流水线：独立 at_jp / l19 写 GM；融合 l18_l19 写 UB 供 INTT ProcessFromLocal。
 * Golden：u_ntt / u_tr 与 gen_data 对拍；本头只做分发，算法在 scalar/vec。
 */
#include "f203_encrypt_at_jp_tiling.h"
#include "alg11_gammas.h"
#include "kernel_operator.h"

#if defined(ASCENDC_CPU_DEBUG)
#include "f203_encrypt_at_jp_scalar.hpp"
#else
#include "f203_encrypt_at_jp_vec.hpp"
#endif

namespace encrypt_at_jp {

constexpr int32_t kN = encrypt_at_jp_tiling::kN;
constexpr int32_t kK = encrypt_at_jp_tiling::kK;
constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;

/**
 * 3 launch / MIX 段：û 写 GM。
 * @param aHat Â [kK,kK,N]；@param yHat ŷ [kK,N]；@param uNtt 输出 û [kK,N]
 * @param pBegin,pEnd 本 AIV 半开行区间（0..2 或 2..4）
 * CPU→scalar；SIM→EncryptAtJpHalfRowsVec::ProcessToGm（DataCopy，非标量写 GM）
 */
__aicore__ inline void innerproduct_halfrows_to_gm(GM_ADDR aHat, GM_ADDR yHat, GM_ADDR uNtt, int32_t pBegin,
                                                   int32_t pEnd)
{
#if defined(ASCENDC_CPU_DEBUG)
    innerproduct_halfrows_scalar(aHat, yHat, uNtt, pBegin, pEnd);
#else
    EncryptAtJpHalfRowsVec op;
    op.Init(aHat, yHat, pBegin, pEnd);
    op.ProcessToGm(uNtt);
#endif
}

/**
 * 单 launch 融合：û 驻留 UB，供 INTT ProcessFromLocal。
 * @param uUb 本核 halfrows 输出缓冲 [pEnd-pBegin, N]
 */
__aicore__ inline void innerproduct_halfrows_to_ub(GM_ADDR aHat, GM_ADDR yHat, AscendC::LocalTensor<int32_t> &uUb,
                                                   int32_t pBegin, int32_t pEnd)
{
#if defined(ASCENDC_CPU_DEBUG)
    innerproduct_halfrows_scalar_to_ub(aHat, yHat, uUb, pBegin, pEnd);
#else
    EncryptAtJpHalfRowsVec op;
    op.Init(aHat, yHat, pBegin, pEnd);
    op.ProcessToUb(uUb);
#endif
}

/**
 * halfrows 内积（û）+ 可选 tr̂（kP=5）→ 统一 pad-8 UB。
 *
 * @param doTrHat 仅 AIV0 为 true 时算 tr̂；@param tHatUbOpt 行 2 decode 驻留（优先于 tHat GM）
 * @param unifiedUTrPad8 true：dst 布局 [û0,û1,tr̂,0] / [û2,û3,0,0]
 * 设计：复用同一 EncryptAtJpHalfRowsVec 的 TPipe，避免 fused 内第二套 TPipe 冲 UB。
 */
__aicore__ inline void innerproduct_halfrows_to_ub_maybe_trhat(GM_ADDR aHat, GM_ADDR yHat, AscendC::LocalTensor<int32_t> &uUb,
                                                              int32_t pBegin, int32_t pEnd, GM_ADDR tHat,
                                                              GM_ADDR trHatNtt, bool doTrHat,
                                                              const AscendC::LocalTensor<int32_t> *tHatUbOpt = nullptr,
                                                              bool unifiedUTrPad8 = false)
{
#if defined(ASCENDC_CPU_DEBUG)
    // CPU 孪生仅写 û；tr̂/pad8 由 SIM 融合路径覆盖
    innerproduct_halfrows_scalar_to_ub(aHat, yHat, uUb, pBegin, pEnd);
    (void)doTrHat;
    (void)tHat;
    (void)trHatNtt;
    (void)tHatUbOpt;
    (void)unifiedUTrPad8;
#else
    EncryptAtJpHalfRowsVec op;
    op.Init(aHat, yHat, pBegin, pEnd);
    op.ProcessToUbMaybeTrHat(uUb, tHat, trHatNtt, doTrHat, tHatUbOpt, unifiedUTrPad8);
#endif
}

/**
 * 内积 UB→GM 落盘：供 host 对拍 u_ntt（仅 û 行，不含 tr̂）。
 * @param uUb 局部行相对 pBegin；写到 GM 绝对行 pBegin..pEnd
 */
__aicore__ inline void dump_u_ntt_halfrows_ub(GM_ADDR uNtt, AscendC::LocalTensor<int32_t> &uUb, int32_t pBegin,
                                              int32_t pEnd)
{
    AscendC::GlobalTensor<int32_t> uGm;
    uGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uNtt));
    const uint32_t dstOff = static_cast<uint32_t>(pBegin) * static_cast<uint32_t>(encrypt_at_jp_tiling::kN);
    const uint32_t elemCount = static_cast<uint32_t>(pEnd - pBegin) * static_cast<uint32_t>(encrypt_at_jp_tiling::kN);
    AscendC::DataCopy(uGm[dstOff], uUb, elemCount);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 统一 uTr[5] 对拍：从 pad-8 UB scatter 到 GM u_tr[5,256]。
 * AIV0: ub[0,1,2]→u_tr[0,1,4]；AIV1: ub[0,1]→u_tr[2,3]（pad 零行不写）。
 */
__aicore__ inline void dump_u_tr_pad8_ub(GM_ADDR uTrGm, AscendC::LocalTensor<int32_t> &uUb, int32_t subBlockID)
{
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    AscendC::GlobalTensor<int32_t> uTrG;
    uTrG.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uTrGm));
    const uint32_t lineLen = static_cast<uint32_t>(kN);
    if (subBlockID == 0) {
        AscendC::DataCopy(uTrG[0], uUb[0], lineLen);
        AscendC::DataCopy(uTrG[lineLen], uUb[lineLen], lineLen);
        AscendC::DataCopy(uTrG[4U * lineLen], uUb[2U * lineLen], lineLen);
    } else {
        AscendC::DataCopy(uTrG[2U * lineLen], uUb[0], lineLen);
        AscendC::DataCopy(uTrG[3U * lineLen], uUb[lineLen], lineLen);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * kP=5 预备：tr̂ = Σ_j MultiplyNTTs(t̂[j], ŷ[j]) mod q，写 GM 单行。
 * CPU：scalar；SIM：标量 GM 循环（避免 fused 内再开 TPipe）。
 */
__aicore__ inline void innerproduct_tr_hat_to_gm(GM_ADDR tHat, GM_ADDR yHat, GM_ADDR trHatNtt)
{
#if defined(ASCENDC_CPU_DEBUG)
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    constexpr int32_t kK = encrypt_at_jp_tiling::kK;
    const auto *tGm = reinterpret_cast<const int32_t *>(tHat);
    const auto *yGm = reinterpret_cast<const int32_t *>(yHat);
    auto *outGm = reinterpret_cast<int32_t *>(trHatNtt);
    int32_t prod[kN];
    int64_t acc[kN];
    for (int32_t c = 0; c < kN; ++c) {
        acc[c] = 0;
    }
    for (int32_t j = 0; j < kK; ++j) {
        const int32_t *tPoly = tGm + static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
        const int32_t *yPoly = yGm + encrypt_at_jp_layout::y_hat_offset(j);
        multiply_ntts_scalar(prod, tPoly, yPoly, kN);
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] += static_cast<int64_t>(prod[c]);
        }
    }
    for (int32_t c = 0; c < kN; ++c) {
        outGm[static_cast<uint32_t>(c)] = mod_q_i64(acc[c]);
    }
#else
    // SIM/NPU：避免额外 TPipe（fused UB 紧张），直接标量 basemul 累加
    constexpr int32_t kN = encrypt_at_jp_tiling::kN;
    constexpr int32_t kK = encrypt_at_jp_tiling::kK;
    constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;
    const auto *tGm = reinterpret_cast<const __gm__ int32_t *>(tHat);
    const auto *yGm = reinterpret_cast<const __gm__ int32_t *>(yHat);
    auto *outGm = reinterpret_cast<__gm__ int32_t *>(trHatNtt);

    auto barrett_red = [](int32_t x) -> int32_t {
        constexpr int32_t q = kQ;
        int32_t t = x + (q & (x >> 31));
        int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
        x = t - t1 * q;
        int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
        x = x - t2 * q;
        x = x - (q & ~((x - q) >> 31));
        return x;
    };

    int64_t acc[kN];
    for (int32_t c = 0; c < kN; ++c) {
        acc[c] = 0;
    }

    for (int32_t j = 0; j < kK; ++j) {
        const __gm__ int32_t *f = tGm + static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
        const __gm__ int32_t *g = yGm + encrypt_at_jp_layout::y_hat_offset(j);
        for (int32_t i = 0; i < kN / 2; ++i) {
            const int32_t gamma = kAlg11Gammas[i];
            const int32_t a0 = f[2 * i];
            const int32_t a1 = f[2 * i + 1];
            const int32_t b0 = g[2 * i];
            const int32_t b1 = g[2 * i + 1];
            const int32_t a1b1 = barrett_red(a1 * b1);
            const int32_t c0 = barrett_red(a0 * b0 + a1b1 * gamma);
            const int32_t c1 = barrett_red(a0 * b1 + a1 * b0);
            acc[2 * i] += static_cast<int64_t>(c0);
            acc[2 * i + 1] += static_cast<int64_t>(c1);
        }
    }

    const int64_t q64 = static_cast<int64_t>(kQ);
    for (int32_t c = 0; c < kN; ++c) {
        int64_t rem = acc[c] % q64;
        if (rem < 0) {
            rem += q64;
        }
        outGm[static_cast<uint32_t>(c)] = static_cast<int32_t>(rem);
    }
#endif
}

} // namespace encrypt_at_jp
