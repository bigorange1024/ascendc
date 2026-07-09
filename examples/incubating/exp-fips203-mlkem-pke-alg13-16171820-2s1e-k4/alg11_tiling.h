/**
 * @file alg11_tiling.h
 * @brief Alg.11 向量 basemul 的静态尺寸：N、pairCount、VecWs 槽位数。
 *
 * 用途：为 multiply_ntts_vec / hat_alg11 提供编译期常量；ALG11_MEM_OPS 决定 ws 是否含内嵌索引 LUT。
 *
 * 调用方：multiply_ntts_ub.hpp、multiply_ntts_vec.hpp、hat_dot_ub_tiling.hpp（kVecWsInts 与之对齐）。
 *
 * 不变量：kN=256、kPairCount=128；kBlockDim=1（本探针单核 MIX）；MEM_OPS=1 时 kVecWsInts=8*128。
 *
 * Golden：无直接对拍；经行 18 t_hat 间接验收。
 *
 * CMake：ALG11_MEM_OPS（multiply_ntts_config.hpp）。
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
/** ROM 索引独立 Init UB；ws 仅 a0..t2 */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
