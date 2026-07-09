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
/** ROM 索引独立 Init UB；ws 仅 a0..t2 */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
