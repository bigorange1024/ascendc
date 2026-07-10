/**
 * @file hat_dot_layout.hpp
 * @brief Alg.13 行 18：Â_hat 在 GM/UB 中的行主序偏移公式。
 *
 * 用途：将 (p, j) 二维索引映射为 Â[p,j] 多项式在扁平 int32 缓冲中的起始下标。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`（全 poly j→p 内积）、`hat_dot_halfrows_ub.hpp`（legacy half 探针）。
 *
 * 不变量：
 *   - flat(p,j,c) = (p*K + j)*N + c，K=tiling::kHatK=4，N=256；
 *   - 与 innerproduct-k4 / gen_data.py 中 a_hat 布局一致。
 *
 * Golden：`scripts/gen_data.py` 生成 a_hat；`verify_result.py` 对拍 t_hat / golden_t_hat_*.bin。
 *
 * CMake：无专用宏；依赖 `tiling.h` 中 kHatK。
 */
#pragma once

#include "tiling.h"

/** Alg13 Â 行主序：flat(p,j,c) = (p*K+j)*N + c（与 innerproduct-k4 一致）。 */
namespace hat_dot_layout {

constexpr uint32_t kN = 256U;

/**
 * Â[p,j] 多项式在扁平 a_hat GM/UB 中的起始元素下标。
 * @param p 行（ê/t̂ 索引 0..3）；@param j 列（ŝ 索引 0..3）
 * @return (p*K+j)*256，dtype int32 元素偏移
 */
__aicore__ inline uint32_t a_hat_offset(uint16_t p, uint16_t j)
{
    return (static_cast<uint32_t>(p) * static_cast<uint32_t>(tiling::kHatK) + static_cast<uint32_t>(j)) * kN;
}

} // namespace hat_dot_layout
