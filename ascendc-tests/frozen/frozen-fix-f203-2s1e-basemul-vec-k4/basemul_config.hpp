#ifndef BASEMUL_CONFIG_HPP
#define BASEMUL_CONFIG_HPP

/**
 * 行 18 MultiplyNTTs basemul 变体（CMake -DHAT_BASEMUL_VARIANT / 环境变量覆盖）。
 *
 * HAT_BASEMUL_VARIANT:
 *   0 — multiply_ntts_half_scalar（基线，与 byteencode 探针一致）
 *   1 — B1：标量 deinterleave + 向量 Mul/Add + 向量 Barrett + γ 广播
 *   2 — B2：Gather deinterleave + 向量 Mul/Add（复活冻结 vec 路径，post-NTT 允许 Gather）
 */
#ifndef HAT_BASEMUL_VARIANT
#define HAT_BASEMUL_VARIANT 0
#endif

#if HAT_BASEMUL_VARIANT >= 1
constexpr uint32_t kHatBasemulExtraInt32Slots = 11U; // a0..gammaV,idx,idx2 × pairCount
#else
constexpr uint32_t kHatBasemulExtraInt32Slots = 0U;
#endif

#endif
