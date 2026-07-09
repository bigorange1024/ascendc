/**
 * @file innerproduct_tiling.h
 * @brief 内积 / su_dot 相关尺寸常量（自 KeyGen 行 18 探针；Decrypt 用 N/q/ROM 子集）。
 *
 * Decrypt ⟨ŝ,û⟩ 实际 k=4、单输出 poly；P_OUT/S_VEC 保留供对照宏覆写。
 */
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
/** ML-KEM K=4 KeyGen 行 18：P_OUT=4, S_VEC=4（可用 CMake 覆写 2×2 对照）。 */
constexpr int32_t kPOut = INNERPRODUCT_P_OUT;
constexpr int32_t kSVec = INNERPRODUCT_S_VEC;
constexpr int32_t kBlockDim = 1;
constexpr int32_t kRomPairCount = kN / 2;
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kHatQ = 3329;

/** a_hat [P_OUT*S_VEC, N] 行主序；s_hat [S_VEC, N]；t_hat [P_OUT, N] */
constexpr int32_t kAHatBytes = kPOut * kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kSHatBytes = kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kTHatBytes = kPOut * kN * static_cast<int32_t>(sizeof(int32_t));

/** scratch 分区偏移（int32）：acc / row / modT2 / outLine… */
constexpr int32_t kOffAcc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kScratchInts = kOffOutLine + kPOut * kN;

} // namespace innerproduct_tiling
