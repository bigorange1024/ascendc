#pragma once

#include "f203_encrypt_at_jp_tiling.h"

/**
 * @file f203_encrypt_at_jp_layout.hpp
 * @brief Encrypt compute Âᵀ∘ŷ 的 GM 布局偏移（与 gen_data / golden 一致）。
 *
 * 流水线位置：Alg.14 行 18 内积；FIPS 203 / ML-KEM-1024。
 *
 *   a_hat 按 flat(j,p,c) = (j*K + p)*N + c 存储 A[j,p]
 *   y_hat[j*N + c] = ŷ[j]
 *
 * 注意：与 KeyGen innerproduct 的 flat(p,j) 不同，不可混用 offset。
 */
namespace encrypt_at_jp_layout {

constexpr int32_t kK = encrypt_at_jp_tiling::kK;
constexpr int32_t kN = encrypt_at_jp_tiling::kN;

/** Â 中 poly A[j,p] 的元素起点（相对 a_hat 基址）。 */
__aicore__ inline uint32_t a_hat_offset_jp(int32_t j, int32_t p)
{
    return (static_cast<uint32_t>(j) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(p)) *
           static_cast<uint32_t>(kN);
}

/** ŷ 中第 j 个 poly 的元素起点。 */
__aicore__ inline uint32_t y_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
}

} // namespace encrypt_at_jp_layout
