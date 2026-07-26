/**
 * @file innerproduct_tiling.h
 * @brief ML-KEM-768（k=3）内积探针形状：全量 3×3 GM，双 AIV 写 2+1 行。
 *
 * 背景：k4 halfrows 用 P_OUT/2 均分；k=3 不可整除。
 * 结论（参数卡 §3.1）：AIV0 → t̂[{0,1}]，AIV1 → t̂[{2}]；scratch 按 max=2 行分配。
 * 未采用：P_OUT/2（会丢弃第 2 行）。
 */
#pragma once

#include <cstdint>

namespace innerproduct_tiling {

#ifndef INNERPRODUCT_P_OUT
#define INNERPRODUCT_P_OUT 3
#endif
#ifndef INNERPRODUCT_S_VEC
#define INNERPRODUCT_S_VEC 3
#endif

constexpr int32_t kN = 256;
constexpr int32_t kPOut = INNERPRODUCT_P_OUT;
constexpr int32_t kSVec = INNERPRODUCT_S_VEC;
/** scratch / 缓冲上限：AIV0 最多 2 行。 */
constexpr int32_t kPPerAivMax = 2;
/** 双 AIV。 */
constexpr int32_t kBlockDim = 2;
constexpr int32_t kRomPairCount = kN / 2;
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kHatQ = 3329;

constexpr int32_t kAHatBytes = kPOut * kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kSHatBytes = kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kTHatBytes = kPOut * kN * static_cast<int32_t>(sizeof(int32_t));

constexpr int32_t kOffAcc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kScratchInts = kOffOutLine + kPPerAivMax * kN;

/**
 * 分核约定（实现见 kernel Init，勿在本头文件放 __aicore__/host 双用函数）：
 * AIV0 → t̂ 行 [0,2)；AIV1 → [2,3)。k=3 不可用 P_OUT/2。
 */

} // namespace innerproduct_tiling
