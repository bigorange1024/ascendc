/**
 * @file f203_encrypt_at_jp_tiling.h
 * @brief 行 18 内积 / 行 2 decode 的 UB scratch 偏移与常量（EncryptAtJpHalfRowsVec）。
 *
 * 与 f203_l18_l19_tiling.h（workspace GM）分工：本头只管 AIV 内积管道内的 int32 分区。
 */
#pragma once

#include <cstdint>
#include "byte_decode12_config.hpp"

namespace encrypt_at_jp_tiling {

constexpr int32_t kN = 256;           /**< 多项式长度 */
constexpr int32_t kK = 3;             /**< Â / r̂ 维数 */
constexpr int32_t kPPerAiv = 2;       /**< 每 AIV 最大驻留行数：AIV0=u0/u1，AIV1=u2/tr̂ */
constexpr int32_t kRomPairCount = kN / 2;
constexpr int32_t kVecWsInts = 8 * kRomPairCount; /**< MultiplyNTTs UB 工作区 */
constexpr int32_t kHatQ = 3329;

constexpr int32_t kOffAcc = 0;        /**< 累加器 */
constexpr int32_t kOffRow = kN;       /**< 当前行临时 */
constexpr int32_t kOffModT2 = 2 * kN; /**< mod 中间 */
constexpr int32_t kOffOutLine = 3 * kN;
/** outLine: û 输出 [kPPerAiv,kN]；trLine: k3 的 tr̂ 行（最终放入 AIV1 第 2 个 INTT slot） */
constexpr int32_t kOffTrLine = kOffOutLine + kPPerAiv * kN;
/** tHatUb: 行 2 decode 驻留，避免 fused 内写 GM */
constexpr int32_t kOffTHatUb = kOffTrLine + kN;
#if F203_BYTE_DECODE12_IMPL >= 1
// decode12_ws: Alg7 两段式备用路径（byteTile + c0/c1/c2/t0/t1）
constexpr int32_t kDecode12WsInts = 736;
constexpr int32_t kOffDecode12Ws = kOffTHatUb + kK * kN;
constexpr int32_t kScratchInts = kOffDecode12Ws + kDecode12WsInts;
#else
constexpr int32_t kScratchInts = kOffTHatUb + kK * kN;
#endif

} // namespace encrypt_at_jp_tiling
