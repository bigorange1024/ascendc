/**
 * @file f203_encrypt_at_r5_layout.h
 * @brief at_r5 输入/输出 GM 偏移（host 拼装 matM 与 device 读写须一致）。
 *
 * matM 物理布局（kK=4 行 × kP=5 列 × kN 系数，行主序）：
 *   matM[(j*kP + p) * kN + c]，j ∈ [0..kK-1], p ∈ [0..kP-1], c ∈ [0..kN-1]
 *
 * Host 拼装规则（INTEGRATION_PLAN §2.3）：
 *   - p ∈ [0..3]：matM[j, p, ·] = Â[j, p, ·]，即 aHatHost[(j*4 + p)*N + c]
 *   - p = 4   ：matM[j, 4, ·] = t̂[j, ·]，即 tHatHost[j*N + c]
 *
 * 数学等价（device 算法见 kernel.cpp）：
 *   uTr[p, c] = Σ_{j=0..kK-1} matM[j, p, c] *_NTT rHat[j, c]
 *     p ∈ [0..3]：uTr[p] = Σ_j Â[j,p] *_NTT r̂[j] = û[p] = (Âᵀ·r̂)[p]（FIPS 203 Alg.14 §18）
 *     p = 4   ：uTr[4] = Σ_j t̂[j] *_NTT r̂[j] = tr̂（FIPS 203 Alg.14 §19 前置）
 */
#pragma once

#include "f203_encrypt_at_r5_tiling.h"

namespace at_r5_layout {

/** matM[(j*kP + p) * kN] 的起始 int32 下标。 */
__aicore__ inline uint32_t mat_offset(int32_t j, int32_t p)
{
    return (static_cast<uint32_t>(j) * static_cast<uint32_t>(at_r5_tiling::kP) +
            static_cast<uint32_t>(p)) *
           static_cast<uint32_t>(at_r5_tiling::kN);
}

/** r̂[j*N] 的起始 int32 下标。 */
__aicore__ inline uint32_t r_hat_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(at_r5_tiling::kN);
}

/** uTr[p*N] 的起始 int32 下标（输出连续 [kP, kN]）。 */
__aicore__ inline uint32_t u_tr_offset(int32_t p)
{
    return static_cast<uint32_t>(p) * static_cast<uint32_t>(at_r5_tiling::kN);
}

} // namespace at_r5_layout
