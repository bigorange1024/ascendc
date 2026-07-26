/**
 * @file innerproduct_layout.h
 * @brief Alg.13 / KeyGen Â、ŝ 的 GM 线性偏移（与 a_hat.bin、C ref 一致）。
 *
 * Â 矩阵唯一布局：K×K 行连续，元素 A[p,j] 为长度 N 的 int32 多项式
 *   flat(p,j,c) = (p * K + j) * N + c
 *
 *   [[a00,a01,a02,a02],
 *    [a10,a11,a12,a12],
 *    ...]
 *
 * ŝ[j]：s_hat[j*N + c]；t̂[p]：t_hat[p*N + c]
 */
#pragma once

#include "innerproduct_tiling.h"

namespace innerproduct_layout {

/** 列维 K，与 S_VEC 相同（KeyGen 方阵）。 */
constexpr int32_t kK = innerproduct_tiling::kSVec;

/**
 * Â[p,j] 在 a_hat GM 中的 int32 起点下标（不含系数 c）。
 * @param p 输出行 0..P_OUT-1
 * @param j 列 / ŝ 下标 0..S_VEC-1
 */
__aicore__ inline uint32_t a_hat_offset(int32_t p, int32_t j)
{
    return (static_cast<uint32_t>(p) * static_cast<uint32_t>(kK) + static_cast<uint32_t>(j)) *
           static_cast<uint32_t>(innerproduct_tiling::kN);
}

/**
 * ŝ[j] 在 s_hat GM 中的 int32 起点下标。
 * @param j 向量分量下标
 */
__aicore__ inline uint32_t s_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(innerproduct_tiling::kN);
}

} // namespace innerproduct_layout
