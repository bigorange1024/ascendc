#pragma once

/**
 * @file f203_encrypt_at_jp_tiling.h
 * @brief 内积 / ByteDecode₁₂ 相关 UB scratch 偏移与几何常量（k=4,N=256,q=3329）。
 *
 * 流水线位置：Alg.14 行 2 decode + 行 18 内积；供 at_jp / l18_l19 共用。
 * 与 golden：仅设备 UB 布局，不改变密文 I/O。
 */
#include <cstdint>
#include "byte_decode12_config.hpp"

namespace encrypt_at_jp_tiling {

constexpr int32_t kN = 256;
constexpr int32_t kK = 4;
constexpr int32_t kPPerAiv = kK / 2;           // 每 AIV 2 行 û
constexpr int32_t kRomPairCount = kN / 2;      // Alg.11 128 对
constexpr int32_t kVecWsInts = 8 * kRomPairCount;
constexpr int32_t kHatQ = 3329;

constexpr int32_t kOffAcc = 0;                 // 累加器
constexpr int32_t kOffRow = kN;                // 当前行缓冲
constexpr int32_t kOffModT2 = 2 * kN;          // mod 临时
constexpr int32_t kOffOutLine = 3 * kN;        // halfrows 输出 [kPPerAiv,kN]
// trLine:  追加 1 行（kP=5 融合预备，p=4），仅 AIV0 计算但复用同一 scratch
constexpr int32_t kOffTrLine = kOffOutLine + kPPerAiv * kN;
// tHatUb: 行2 decode_t_hat 的 UB 驻留区（kK*kN int32），避免 fused 内写 GM。
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
