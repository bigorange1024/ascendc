/**
 * @file f203_encrypt_at_r_layout.h
 * @brief G3 L4：Âᵀ·r̂ GM 偏移（Encrypt 读 A[j,p] 即 a_hat_offset(j,p)）。
 *
 * 背景：KeyGen 行 18 读 A[p,j]；Encrypt 行 18 读 A[j,p]（同 GM，索引对调，见 INTEGRATION_PLAN §2.1）。
 */
#pragma once

#include "innerproduct_tiling.h"

namespace f203_at_r_layout {

constexpr int32_t kK = innerproduct_tiling::kSVec;

/** KeyGen 布局 flat(p,j,c) = (p*K+j)*N + c；Encrypt Âᵀ 读 A[j,p] → (j*K+p)*N。 */
__aicore__ inline uint32_t a_hat_offset_at(int32_t p, int32_t j)
{
    return (static_cast<uint32_t>(j) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(p)) *
           static_cast<uint32_t>(innerproduct_tiling::kN);
}

__aicore__ inline uint32_t r_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(innerproduct_tiling::kN);
}

} // namespace f203_at_r_layout
