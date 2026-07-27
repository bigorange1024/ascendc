/**
 * @file innerproduct_tiling.h
 * @brief ML-KEM-512（k=2）内积探针形状：全量 2×2 GM，双 AIV 写 1+1 行。
 *
 * 背景：ML-KEM-512 的行 18 内积只输出 2 条 t̂ 行，P_OUT 与 S_VEC 必须同为 2。
 * 结论：AIV0 → t̂[{0}]，AIV1 → t̂[{1}]；scratch 按每核 1 行分配。
 * 未采用：沿用 k=3 的 P_OUT/S_VEC=3，或写成依赖 P_OUT/2 的隐式分片。
 */
#pragma once

#include <cstdint>

namespace innerproduct_tiling {

#ifndef INNERPRODUCT_P_OUT
#define INNERPRODUCT_P_OUT 2
#endif
#ifndef INNERPRODUCT_S_VEC
#define INNERPRODUCT_S_VEC 2
#endif

constexpr int32_t kN = 256;
constexpr int32_t kPOut = INNERPRODUCT_P_OUT;
constexpr int32_t kSVec = INNERPRODUCT_S_VEC;
/** scratch / 缓冲上限：k=2 时每个 AIV 只负责 1 行。 */
constexpr int32_t kPPerAivMax = 1;
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
 * AIV0 → t̂ 行 [0,1)；AIV1 → [1,2)。这是 k=2 锁定分片，不由 P_OUT/2 临时推导。
 */

} // namespace innerproduct_tiling
