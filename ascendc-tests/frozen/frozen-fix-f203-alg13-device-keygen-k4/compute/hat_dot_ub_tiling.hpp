// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/hat_dot_ub_tiling.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_dot_ub_tiling.hpp` 为该子模块组件。 / Component: hat_dot_ub_tiling.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file hat_dot_ub_tiling.hpp
 * @brief 行 18 UB 驻留内积路径的 scratch / ROM / 分核常量（与 hat_dot_halfrows_ub 及 2s1e 管线共用）。
 *
 * 用途：定义 q=3329、pairCount=128、每 AIV 2 个 p 槽，以及 dot scratch 内 fLoc/row/modT2/outLine 偏移。
 *
 * 调用方：`hat_dot_halfrows_ub.hpp`、`2s1e_post_ntt_ub.hpp`（通过 hat_dot_ub_tiling 与 innerproduct 块）。
 *
 * 不变量：
 *   - kPPerAiv=2（4 个 ê 多项式 / 2 AIV）；
 *   - kDotScratchInts = 3*N + kPPerAiv*N（fLoc + 行累加 + mod 临时 + 输出行）；
 *   - kVecWsInts = 8*128（Alg11 向量 ws，与 alg11_tiling 对齐）。
 *
 * Golden：行 18 对拍 `output/t_hat.bin` vs `golden_t_hat_*.bin`（gen_data + hat_inner_product_ref.c）。
 *
 * CMake：无；与 `integration_config.hpp` 中 HAT_LINE18_* 组合决定走全 poly 或 halfrows 探针。
 */
#pragma once

#include <cstdint>

namespace hat_dot_ub {

constexpr int32_t kN = 256;
constexpr int32_t kHatQ = 3329;
constexpr int32_t kRomPairCount = kN / 2;
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kPPerAiv = 2;

constexpr int32_t kOffFLoc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kDotScratchInts = kOffOutLine + kPPerAiv * kN;

} // namespace hat_dot_ub
