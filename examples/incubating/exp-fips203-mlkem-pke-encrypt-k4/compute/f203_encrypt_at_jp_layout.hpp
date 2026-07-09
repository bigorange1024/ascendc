#pragma once

#include "f203_encrypt_at_jp_tiling.h"

/**
 * Encrypt compute Âᵀ∘ŷ 的 GM 布局（与 gen_data.py / golden 一致）：
 *
 *   a_hat.bin 按 flat(j,p,c) = (j*K + p)*N + c 存储 A[j,p]
 *   y_hat[j*N + c] = ŷ[j]
 *
 * 注意：与 KeyGen innerproduct-k4-halfrows 的 flat(p,j) 不同，不可混用 offset。
 */
namespace encrypt_at_jp_layout {

constexpr int32_t kK = encrypt_at_jp_tiling::kK;
constexpr int32_t kN = encrypt_at_jp_tiling::kN;

__aicore__ inline uint32_t a_hat_offset_jp(int32_t j, int32_t p)
{
    return (static_cast<uint32_t>(j) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(p)) *
           static_cast<uint32_t>(kN);
}

__aicore__ inline uint32_t y_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
}

} // namespace encrypt_at_jp_layout
