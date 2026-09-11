/**
 * @file tiling.h
 * @brief RB-T28-dec：Alg.15 Decrypt — prep AIV → 融合 NTT+INTT MIX（同核串行两段）。
 *
 * Flag（融合 MIX 内复用，各段完整一轮；对齐 D08）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 可选 4 = GATE（本刀不用）
 * 永禁 5 / 7；Wait 环内禁 SyncAll / SoftSync。
 *
 * 数学（FIPS 203 Alg.15，k=4）：
 *   ŝ←BD₁₂(dk)；u←Decomp₁₁(BD₁₁(c₁))；v←Decomp₅(BD₅(c₂))
 *   û←NTT(u)；ŵ←Σⱼ MultiplyNTTs(ŝⱼ,ûⱼ)；w←INTT(ŵ)
 *   m←BE₁(Compress₁(v−w))
 *
 * BLOCK_DIM=1。禁抄 alg15/decrypt/encrypt/encaps/decaps/l18_l19/frozen。
 */
#ifndef RB_T28_DEC_TILING_H
#define RB_T28_DEC_TILING_H

#include <cstddef>
#include <cstdint>

struct DecTilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace dec_tiling {

constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;
constexpr int32_t kInttScale = 3303; // 128^{-1} mod q
constexpr int32_t kDu = 11;
constexpr int32_t kDv = 5;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t);
constexpr size_t kSHatBytes = static_cast<size_t>(kKem) * kPolyBytes; // ŝ
constexpr size_t kUBytes = kSHatBytes;
constexpr size_t kVBytes = kPolyBytes;
constexpr size_t kUHatBytes = kUBytes;
constexpr size_t kWHatBytes = kPolyBytes;
constexpr size_t kWBytes = kPolyBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t);

constexpr size_t kDkBytes = 1536;  // ByteEncode₁₂(ŝ)，k=4
constexpr size_t kCBytes = 1568;   // c₁‖c₂
constexpr size_t kC1Bytes = 32 * static_cast<size_t>(kDu) * static_cast<size_t>(kKem); // 1408
constexpr size_t kC2Bytes = 32 * static_cast<size_t>(kDv);                             // 160
constexpr size_t kMBytes = 32;

/** 极轻 Cube：握手段有界真算。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 24;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * workspace 布局（Host 预喂 LUT/mat/TRACE；prep 写 ŝ/u/v；NTT 写 û/ŵ；INTT 写 w/m）：
 *   DK / C 镜像 → ŝ / U / V → ZETAS / GAMMAS → U_HAT / W_HAT / W → MAT_* → TRACE / M
 */
constexpr size_t OFF_DK = 0;
constexpr size_t OFF_C = OFF_DK + kDkBytes;
constexpr size_t OFF_S_HAT = OFF_C + kCBytes;
constexpr size_t OFF_U = OFF_S_HAT + kSHatBytes;
constexpr size_t OFF_V = OFF_U + kUBytes;
constexpr size_t OFF_ZETAS = OFF_V + kVBytes;
constexpr size_t OFF_GAMMAS = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_U_HAT = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_W_HAT = OFF_U_HAT + kUHatBytes;
constexpr size_t OFF_W = OFF_W_HAT + kWHatBytes;
constexpr size_t OFF_MAT_A = OFF_W + kWBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C_NTT = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_MAT_C_INTT = OFF_MAT_C_NTT + kMatCBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C_INTT + kMatCBytes;
constexpr size_t OFF_M = OFF_TRACE + kTraceBytes;
constexpr size_t OFF_PREP_MARK = OFF_M + kMBytes;
constexpr size_t wssize = OFF_PREP_MARK + 64;

constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;
// kFlagGate=4 本刀不用（融合核内复用 1/3，对齐 D08 已证路径）

constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_PREP_DONE = 1;
constexpr uint32_t SLOT_HOST_MID1 = 2;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 3;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 4;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 5;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 6;
constexpr uint32_t SLOT_AIV0_NTT_DOT_DONE = 7;
constexpr uint32_t SLOT_HOST_MID2 = 8;
constexpr uint32_t SLOT_AIV0_PRE_SET1_INTT = 9;
constexpr uint32_t SLOT_AIC_POST_WAIT1_INTT = 10;
constexpr uint32_t SLOT_AIC_PRE_SET3_INTT = 11;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_INTT = 12;
constexpr uint32_t SLOT_AIV0_EXTRACT_DONE = 13;
constexpr uint32_t SLOT_HOST_POST = 14;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 15;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_NTT = 16;
constexpr uint32_t SLOT_AIV1_PRE_SET1_INTT = 17;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_INTT = 18;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_PREP_DONE = 0x50524550u; // "PREP"
constexpr uint32_t MAGIC_HOST_MID1 = 0x484F4D31u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_NTT_DOT_DONE = 0x4E545444u; // "NTTD"
constexpr uint32_t MAGIC_HOST_MID2 = 0x484F4D32u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV0_EXTRACT_DONE = 0x4D455854u; // "MEXT"
constexpr uint32_t MAGIC_HOST_POST = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_PREP_MARK = 0x50524B31u;
constexpr uint32_t MAGIC_OUT_OK = 0x543F001Du; // T29 decrypt prep+NTT+INTT fused MIX

} // namespace dec_tiling

#endif
