/**
 * @file tiling.h
 * @brief Alg.11 MultiplyNTTs 尺寸与向量工作区常量（Decrypt su_dot）。
 *
 * N=256、pairCount=128；ALG11_MEM_OPS=1 时 ws 仅 8×pair（ROM 索引独立 UB）。
 */
#pragma once

#include <cstdint>

#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif

namespace alg11_tiling {
constexpr int32_t kN = 256;
constexpr int32_t kBlockDim = 1;
constexpr int32_t kPairCount = kN / 2;
constexpr int32_t kInterleaveReorderCount = kN;
#if ALG11_MEM_OPS == 1
/** ROM 索引独立 Init UB；ws 仅 a0..t2 共 8 条 lane */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
/** legacy：索引也挤在 ws 内，共 10 条 lane */
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
