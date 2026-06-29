// @probe exp-mlkem-f203-pke-keygen-k4
// @file compute/hat_line18_2s1e.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_line18_2s1e.hpp` 为该子模块组件。 / Component: hat_line18_2s1e.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file hat_line18_2s1e.hpp
 * @brief 2s1e 行 18 布局：每 AIV 握完整 ŝ[0..3]、ê 局部行号、p 区间与 Â 偏移辅助。
 *
 * 用途：twos1e::s_row / e_local_lp / p_begin / p_end — 接在 tiling::kS 条 ŝ 后的 ê 分片。
 *
 * 调用方：byte_encode12_pair.hpp、`2s1e_post_ntt_ub.hpp`（poly-batch 语义，非 limbsplit）。
 *
 * 不变量：
 *   - 每 AIV kEPerAiv=2 个 p（共 4 ê）；ŝ 行号即 j∈[0,3]；
 *   - 与 docs/notes/F203-2s1e-NTT内积UB融合技术总结.md poly-batch 契约一致。
 *
 * Golden：src.bin 行 0..3 为 ŝ、4..7 为 ê 变体；t_hat [4,256]。
 *
 * CMake：HAT_LINE18_FULLPOLY（integration_config.hpp，默认 1=j→p 全 poly）。
 */
#ifndef HAT_LINE18_2S1E_HPP
#define HAT_LINE18_2S1E_HPP

#include "tiling.h"

namespace twos1e {

/** 每 AIV 握完整 ŝ[0..3]：行号即 j。 */
__aicore__ inline uint32_t s_row(uint16_t j)
{
    return static_cast<uint32_t>(j);
}

/** 本 AIV ê 在 ub_ntt 中的 local 行（接在 kS 条 ŝ 后）。 */
__aicore__ inline uint32_t e_local_lp(uint16_t p, uint16_t pBegin)
{
    return static_cast<uint32_t>(tiling::kS) + (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin));
}

__aicore__ inline uint16_t p_begin(int32_t subCoreIdx)
{
    /* AIV0: p∈[0,1]；AIV1: p∈[2,3] */
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv);
}

/** 开区间上界：p_end - 1 为本核最后一个 p */
__aicore__ inline uint16_t p_end(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv) +
           static_cast<uint16_t>(tiling::kEPerAiv);
}

} // namespace twos1e

#endif
