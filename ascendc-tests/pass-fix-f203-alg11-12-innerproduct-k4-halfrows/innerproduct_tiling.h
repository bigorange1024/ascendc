#pragma once

#include <cstdint>

namespace innerproduct_tiling {

#ifndef INNERPRODUCT_P_OUT
#define INNERPRODUCT_P_OUT 4
#endif
#ifndef INNERPRODUCT_S_VEC
#define INNERPRODUCT_S_VEC 4
#endif

constexpr int32_t kN = 256;
/** 4×4×1 全量 a_hat GM；每 AIV 只写 P_PER_AIV=2 行（对齐 KeyGen 双 AIV 行 18）。 */
constexpr int32_t kPOut = INNERPRODUCT_P_OUT;
constexpr int32_t kSVec = INNERPRODUCT_S_VEC;
constexpr int32_t kPPerAiv = kPOut / 2;
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
constexpr int32_t kScratchInts = kOffOutLine + kPPerAiv * kN;

} // namespace innerproduct_tiling
