/**
 * @file f203_encrypt_g4_scalar.hpp
 * @brief G4 标量设备核：u+e₁、v=tr+e₂+μ（mod q）；正确性优先，SIM 可 launch。
 */
#pragma once

#include "f203_encrypt_layout.h"
#include "kernel_operator.h"

namespace f203_g4 {

constexpr int32_t kQ = F203_ENCRYPT_Q;
constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kK = F203_ENCRYPT_K;

/**
 * 非负值条件减取模：x ∈ [0, 多个 q) → [0, q)。
 *
 * 背景（CPU↔SIM 差异，2026-06-30 定位）：旧实现用 `int64_t % q`（64 位整数取模/除法），
 * CPU 孪生跑 x86 没问题（CPU 全链 max=0），但 AI Core 标量单元 / CAModel 不支持 64 位整数
 * 除法，导致本 kernel SIM 注册失败（RegisterAscendBinary 107000）→ launch 507000。
 * 这里改用条件减（与 pack 的乘加移位同属“无除法”），调用方须保证 x ≥ 0。
 *
 * @param x 非负输入；本文件用途下上界 < 3q，循环至多减 2 次。
 */
__aicore__ inline int32_t reduce_nonneg_modq(int32_t x)
{
    while (x >= kQ) {
        x -= kQ;
    }
    return x;
}

/**
 * Alg.14 行 19–21：u ← u+e₁（mod q）；v ← tr+e₂+Decompress₁(μ)（mod q）。
 *
 * 全程 int32 + 条件减，禁止除法/取模（见 reduce_nonneg_modq 背景）。
 * 取值范围说明：u/e₁/tr/e₂ 均 ∈ [0,q)（INTT 输出与 CBD 噪声），halfQ·bit ∈ {0,1665}，
 * 故 u+e₁ < 2q、tr+e₂+μ < 2q+1665 < 3q，条件减安全。
 *
 * @param uGm  in/out：u polyvec [K*N] int32，原地写回 u+e₁
 * @param e1Gm in：噪声 e₁ [K*N] int32
 * @param trGm in：tr poly [N] int32（trGm 实际指向 INTT(tr_pad) 的 poly0）
 * @param e2Gm in：噪声 e₂ [N] int32
 * @param mGm  in：32B 明文，逐 bit 嵌入（halfQ = ⌈q/2⌉）
 * @param vGm  out：v poly [N] int32
 */
__aicore__ inline void add_noise_embed_scalar(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm, GM_ADDR mGm,
                                              GM_ADDR vGm)
{
    auto *u = reinterpret_cast<__gm__ int32_t *>(uGm);
    auto *e1 = reinterpret_cast<__gm__ int32_t *>(e1Gm);
    auto *tr = reinterpret_cast<__gm__ int32_t *>(trGm);
    auto *e2 = reinterpret_cast<__gm__ int32_t *>(e2Gm);
    const auto *m = reinterpret_cast<const __gm__ uint8_t *>(mGm);
    auto *v = reinterpret_cast<__gm__ int32_t *>(vGm);

    // u ← u + e₁（每个系数 < 2q，一次条件减即可）
    for (int32_t p = 0; p < kK; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const int32_t sum = u[p * kN + c] + e1[p * kN + c];
            u[p * kN + c] = reduce_nonneg_modq(sum);
        }
    }

    // v ← tr + e₂ + μ·halfQ（μ 为明文逐 bit；和 < 3q，条件减至多 2 次）
    const int32_t halfQ = (kQ + 1) / 2;
    for (int32_t c = 0; c < kN; ++c) {
        int32_t val = tr[c] + e2[c];
        const int32_t i = c >> 3;   // 第 i 字节（c/8，位运算避免除法）
        const int32_t j = c & 7;    // 字节内第 j bit（c%8）
        if (i < 32) {
            const int32_t bit = (static_cast<int32_t>(m[i]) >> j) & 1;
            val += halfQ * bit;
        }
        v[c] = reduce_nonneg_modq(val);
    }
}

} // namespace f203_g4
