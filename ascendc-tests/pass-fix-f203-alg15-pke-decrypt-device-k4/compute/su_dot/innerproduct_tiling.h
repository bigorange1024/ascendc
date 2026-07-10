/**
 * @file innerproduct_tiling.h
 * @brief 单 AIV 内积探针的形状、字节数与 UB scratch 布局常量。
 *
 * 默认 ML-KEM K=4 KeyGen 行 18：P_OUT=4、S_VEC=4、N=256、blockDim=1。
 * 可用 CMake/宏 INNERPRODUCT_P_OUT、INNERPRODUCT_S_VEC 覆写做 2×2 对照。
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
/** 本探针单核；半行变体见 halfrows 目录 kBlockDim=2。 */
constexpr int32_t kBlockDim = 1;
/** MultiplyNTTs 偶奇对数 = N/2。 */
constexpr int32_t kRomPairCount = kN / 2;
/** 向量 MultiplyNTTs 工作区 int32 个数（8 段 × pair）。 */
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kHatQ = 3329;

/** a_hat [P_OUT*S_VEC, N] 行主序；s_hat [S_VEC, N]；t_hat [P_OUT, N] */
constexpr int32_t kAHatBytes = kPOut * kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kSHatBytes = kSVec * kN * static_cast<int32_t>(sizeof(int32_t));
constexpr int32_t kTHatBytes = kPOut * kN * static_cast<int32_t>(sizeof(int32_t));

/** scratch_ 内 int32 分区：acc/fLoc | row | modT2 | outLine[P_OUT*N] */
constexpr int32_t kOffAcc = 0;
constexpr int32_t kOffRow = kN;
constexpr int32_t kOffModT2 = 2 * kN;
constexpr int32_t kOffOutLine = 3 * kN;
constexpr int32_t kScratchInts = kOffOutLine + kPOut * kN;

} // namespace innerproduct_tiling
