// @probe exp-fips203-mlkem-pke-keygen-k2
// @file compute/hat_line18_2s1e.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_line18_2s1e.hpp` 为该子模块组件。 / Component: hat_line18_2s1e.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 hat 点积（Â∘ŝ）与相关 UB/tiling。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/hat_line18_2s1e.hpp
 */
/**
 * @file hat_line18_2s1e.hpp
 * @brief k2 行 18 布局：每 AIV 读取完整 ŝ[0..1]、ê 局部行号、p 区间与 Â 偏移辅助。
 *
 * 用途：twos1e::s_row / e_local_lp / p_begin / p_end — 接在 tiling::kS 条 ŝ 后的 ê 分片。
 *
 * 调用方：byte_encode12_pair.hpp、`2s1e_post_ntt_ub.hpp`（poly-batch 语义，非 limbsplit）。
 *
 * 不变量：
 *   - AIV0 负责 p={0}，AIV1 负责 p={1}（参数卡 §3.2 / D13），禁止补到 3 行；
 *   - ŝ 行号即 j∈[0,1]，由 compute launch 内 GM 暂存 + SyncAll 后共享读取。
 *
 * Golden：src.bin 行 0..1 为 ŝ、2..3 为 ê；t_hat [2,256]。
 *
 * CMake：HAT_LINE18_FULLPOLY（integration_config.hpp，默认 1=j→p 全 poly）。
 */
#ifndef HAT_LINE18_2S1E_HPP
#define HAT_LINE18_2S1E_HPP

#include "tiling.h"

namespace twos1e {

/** 每 AIV 读取完整 ŝ[0..1]：行号即 j。 */
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
    return (subCoreIdx == 0) ? 0U : 1U;
}

/** 开区间上界：p_end - 1 为本核最后一个 p */
__aicore__ inline uint16_t p_end(int32_t subCoreIdx)
{
    return (subCoreIdx == 0) ? 1U : 2U;
}

} // namespace twos1e

#endif
