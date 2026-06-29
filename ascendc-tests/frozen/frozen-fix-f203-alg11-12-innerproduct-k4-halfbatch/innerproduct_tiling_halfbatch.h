#pragma once

#include <cstdint>

namespace innerproduct_tiling {

#ifndef INNERPRODUCT_HALF_BATCH
#define INNERPRODUCT_HALF_BATCH 1
#endif

constexpr int32_t kN = 256;
/** 2×2×1：2 行输出，ŝ 长度 2，无 ê。 */
constexpr int32_t kPOut = 2;
constexpr int32_t kSVec = 2;
constexpr int32_t kBlockDim = 1;
constexpr int32_t kHalfLen = kN / 2;
constexpr int32_t kHalfPairCount = kHalfLen / 2;
constexpr int32_t kRomPairCount = kN / 2;
constexpr int32_t kInterleaveReorderCount = kN;
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kHalfWsInts = 8 * kHalfPairCount;
constexpr int32_t kHatQ = 3329;

constexpr int32_t kAColBytes = kPOut * kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kSHatBytes = kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kTHatBytes = kPOut * kN * static_cast<int32_t>(sizeof(int32_t));

/** 一期全 poly：acc + row + modT2 + outLine(2×N) = 5×N */
constexpr int32_t kOffAcc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kScratchIntsPhase1 = 5 * kN;

/** 二期 half 批处理 scratch（见 INNERPRODUCT_K4_PLAN.md §2.5） */
constexpr int32_t kOffHalfOutLine = 0;
constexpr int32_t kOffHalfRow = kPOut * kN;
constexpr int32_t kOffSB0 = kOffHalfRow + kHalfLen;
constexpr int32_t kOffSB1 = kOffSB0 + kHalfPairCount;
constexpr int32_t kOffAPair = kOffSB1 + kHalfPairCount;
constexpr int32_t kOffSHalf = kOffAPair + kHalfLen;
constexpr int32_t kOffGammaSlice = kOffSHalf + kHalfLen;
constexpr int32_t kOffHalfModT2 = kOffGammaSlice + kHalfPairCount;
constexpr int32_t kScratchIntsHalf = kOffHalfModT2 + kHalfLen;

/** 当前 kernel scratch 大小（随 INNERPRODUCT_HALF_BATCH 在编译期选择） */
#if defined(INNERPRODUCT_HALF_BATCH) && INNERPRODUCT_HALF_BATCH == 0
constexpr int32_t kScratchInts = kScratchIntsPhase1;
#else
constexpr int32_t kScratchInts = kScratchIntsHalf;
#endif

} // namespace innerproduct_tiling
