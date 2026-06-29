// @probe exp-mlkem-f203-pke-keygen-k4
// @file compute/hat_dot_layout.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_dot_layout.hpp` 为该子模块组件。 / Component: hat_dot_layout.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

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

__aicore__ inline uint32_t a_hat_offset(uint16_t p, uint16_t j)
{
    return (static_cast<uint32_t>(p) * static_cast<uint32_t>(tiling::kHatK) + static_cast<uint32_t>(j)) * kN;
}

} // namespace hat_dot_layout
