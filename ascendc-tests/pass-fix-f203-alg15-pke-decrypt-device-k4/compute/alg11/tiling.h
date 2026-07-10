/**
 * @file tiling.h
 * @brief Alg.11 设备侧 tiling 常量（n=256、pair、workspace 槽位数）。
 *
 * 流水线位置：MultiplyNTTs / su_dot 分配 UB workspace。
 * ALG11_MEM_OPS=1（生产默认）：索引在独立 Init UB，ws 仅 a0..t2 共 8×pair；
 * =0 时 ws 含索引共 10×pair（遗留）。
 */
#pragma once

#include <cstdint>

#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif

namespace alg11_tiling {
constexpr int32_t kN = 256;
constexpr int32_t kBlockDim = 1;
constexpr int32_t kPairCount = kN / 2;                 /* 128 个 (偶,奇) 系数对 */
constexpr int32_t kInterleaveReorderCount = kN;       /* interleave 重排索引长度 */
#if ALG11_MEM_OPS == 1
/** ROM 索引独立 Init UB；ws 仅 a0..t2 */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
