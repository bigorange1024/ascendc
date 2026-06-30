#pragma once

/**
 * @file f203_encrypt_g4_host_scalar.hpp
 * @brief G4 噪声/μ Host 标量（**已冻结**：frozen-gates/frozen-g4-host-scalar-tail/；G4 SIM 507000 绕行）。
 * G5 生产路径不得使用；仅 ENCRYPT_GATE=4 过渡回放。
 */
#include "f203_encrypt_layout.h"

#include <cstdint>

namespace f203_g4_host {

constexpr int32_t kQ = F203_ENCRYPT_Q;
constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kK = F203_ENCRYPT_K;

inline int32_t mod_q_i64(int64_t x)
{
    const int64_t q = kQ;
    int64_t rem = x % q;
    if (rem < 0) {
        rem += q;
    }
    return static_cast<int32_t>(rem);
}

/** Alg.14 行 19–21：u ← u+e₁；v ← tr+e₂+Decompress₁(μ)（tr 为 INTT 后首 poly[256]）。 */
inline void add_noise_embed(uint8_t *u, const uint8_t *e1, const uint8_t *tr, const uint8_t *e2, const uint8_t *m,
                            uint8_t *v)
{
    auto *u32 = reinterpret_cast<int32_t *>(u);
    const auto *e1_32 = reinterpret_cast<const int32_t *>(e1);
    const auto *tr32 = reinterpret_cast<const int32_t *>(tr);
    const auto *e2_32 = reinterpret_cast<const int32_t *>(e2);
    const auto *m8 = reinterpret_cast<const uint8_t *>(m);
    auto *v32 = reinterpret_cast<int32_t *>(v);

    for (int32_t p = 0; p < kK; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const int64_t sum = static_cast<int64_t>(u32[p * kN + c]) + static_cast<int64_t>(e1_32[p * kN + c]);
            u32[p * kN + c] = mod_q_i64(sum);
        }
    }

    const int32_t halfQ = (kQ + 1) / 2;
    for (int32_t c = 0; c < kN; ++c) {
        int64_t val = static_cast<int64_t>(tr32[c]) + static_cast<int64_t>(e2_32[c]);
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        if (i < 32) {
            const int32_t bit = (static_cast<int32_t>(m8[i]) >> j) & 1;
            val += static_cast<int64_t>(halfQ) * static_cast<int64_t>(bit);
        }
        v32[c] = mod_q_i64(val);
    }
}

} // namespace f203_g4_host
