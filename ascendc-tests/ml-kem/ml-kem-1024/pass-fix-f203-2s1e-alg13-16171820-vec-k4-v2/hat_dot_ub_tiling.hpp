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

constexpr int32_t kN = 256;              /**< 单 poly 系数数 */
constexpr int32_t kHatQ = 3329;          /**< ML-KEM 模数 q */
constexpr int32_t kRomPairCount = kN / 2; /**< Alg.11 对数 = 128 */
constexpr int32_t kVecWsInts = 8 * kRomPairCount; /**< Alg11 向量工作区 int32 槽 */
constexpr int32_t kPPerAiv = 2;          /**< 每 AIV 负责 2 个 p */

/* dotScratch 内相对偏移（int32 元素）：fLoc | row累加 | modT2 | outLine×kPPerAiv */
constexpr int32_t kOffFLoc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kDotScratchInts = kOffOutLine + kPPerAiv * kN;

} // namespace hat_dot_ub
