#pragma once

#include "innerproduct_tiling.h"

/**
 * Alg13 / KeyGen Â 矩阵唯一 GM 布局（与 a_hat.bin、hat_inner_product_ref 一致）：
 *
 *   K×K 行连续，元素 A[p,j] 为长度 N 的 int32 多项式
 *   flat(p,j,c) = (p * K + j) * N + c
 *
 *   [[a00,a01,a02,a03],
 *    [a10,a11,a12,a13],
 *    ...]
 *
 * ŝ[j]：s_hat[j*N + c]
 * t̂[p]：t_hat[p*N + c]
 */
namespace innerproduct_layout {

constexpr int32_t kK = innerproduct_tiling::kSVec;

__aicore__ inline uint32_t a_hat_offset(int32_t p, int32_t j)
{
    return (static_cast<uint32_t>(p) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(j)) *
           static_cast<uint32_t>(innerproduct_tiling::kN);
}

__aicore__ inline uint32_t s_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(innerproduct_tiling::kN);
}

} // namespace innerproduct_layout
