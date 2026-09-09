/**
 * @file tiling.h
 * @brief RB-D05 Decaps Re-Encrypt：ek + m' + r' → c'[1568]（双 launch）。
 *
 * Launch1 prep（AIV）：r'/coins→OFF_COINS；完整 ek→OFF_EK；ρ←ek 尾。
 * Launch2 mix（MIX）：μ←Decompress₁(m') → Decode₁₂→CBD→Â/ŷ→Mul/INTT→pack→c'。
 * Host 仅预装原始 m'；禁预喂最终 μ/t̂/y/e/Â/ŷ/u/v/c'。
 *
 * Flag（永禁 5/7）：1/3 复用 + 4=GATE；BLOCK_DIM=1。
 *
 * 时序（mix）：
 *   AIC: Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV: Set(1)→Wait(3) → μ←m' → Decode₁₂ → CBD → Â+ŷ → Mul → Set(4)
 *        → Set(1)→Wait(3) → INTT+噪 → pack
 */
#ifndef RB_D05_DECAPS_REENC_TILING_H
#define RB_D05_DECAPS_REENC_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024 常量。 */
constexpr int32_t kKem = 4;
constexpr int32_t kK = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kInttScale = 3303; /**< FIPS Alg.10：128^{-1} mod q */
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;
constexpr int32_t kEta = 2;
constexpr int32_t kNoisePolys = 9; // y[4]+e1[4]+e2[1]

/** ByteDecode₁₂：每 poly 384B；ek = 体[1536]‖ρ[32]。 */
constexpr size_t kPolyPackedBytes = 384; // 12*256/8
constexpr size_t kEkBodyBytes = static_cast<size_t>(kKem) * kPolyPackedBytes; // 1536
constexpr size_t kEkBytes = 1568;
constexpr size_t kCoinsBytes = 32;
constexpr size_t kRhoBytes = 32;
constexpr size_t kMsgBytes = 32; /**< Decaps m' ∈ B^32；μ 源 */
/** PRF_η 输出 / poly：η·N/4 = 128；9 poly 总 1152。 */
constexpr size_t kPrfBytesPerPoly = static_cast<size_t>(kEta) * kPolyN / 4U;
constexpr size_t kPrfTotalBytes = static_cast<size_t>(kNoisePolys) * kPrfBytesPerPoly;
/** 设备 CBD 写出：y(4)+e1(4)+e2(1)。 */
constexpr size_t kYe1e2Bytes = 9 * kPolyN * sizeof(int32_t); // 9216
constexpr size_t kYBytes = static_cast<size_t>(kKem) * kPolyN * sizeof(int32_t); // 4096

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kAHatBytes = static_cast<size_t>(kKem) * kKem * kPolyBytes; // 16384
constexpr size_t kYHatBytes = static_cast<size_t>(kKem) * kPolyBytes;       // 4096
constexpr size_t kTHatCoeffs = static_cast<size_t>(kKem) * static_cast<size_t>(kPolyN); // 1024
constexpr size_t kTHatBytes = kYHatBytes;
constexpr size_t kE1Bytes = kYHatBytes;
constexpr size_t kE2Bytes = kPolyBytes;
constexpr size_t kMuBytes = kPolyBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t);
constexpr size_t kUHatBytes = kYHatBytes;
constexpr size_t kVHatBytes = kPolyBytes;
constexpr size_t kUBytes = kYHatBytes;
constexpr size_t kVBytes = kPolyBytes;
constexpr size_t kCBytes = 1568;

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 29;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace（两 launch 共用）：
 *   COINS → Host H2D（禁预喂最终 y/e/Â/ŷ/u/v/c/t̂/μ）
 *   RHO / EK / PREP_MARK → Launch1 写（EK=完整 1568 供 Decode₁₂）
 *   MAT_* 紧随 prep（低偏移；CPU 孪生上远偏移 Fixpipe 曾见静默空写）
 *   PRF → Launch2 CBD 中间区
 *   Y_E1_E2 → 设备 CBD 写出；E1/E2 为区内别名（加噪读端）
 *   A_HAT/Y_HAT/T_HAT → **设备写**（T_HAT=Decode₁₂）
 *   M → Host 预装**原始消息**；MU → **设备** Decompress₁ 写
 *   ZETAS/GAMMAS → Host 预装
 *   U_HAT/V_HAT/U/V/C/TRACE → Launch2
 */
constexpr size_t OFF_COINS = 0;
constexpr size_t OFF_RHO = OFF_COINS + kCoinsBytes;
constexpr size_t OFF_PREP_MARK = OFF_RHO + kRhoBytes;
constexpr size_t OFF_EK = OFF_PREP_MARK + 64;
constexpr size_t OFF_MAT_A = OFF_EK + kEkBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C_NTT = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_MAT_C_INTT = OFF_MAT_C_NTT + kMatCBytes;
constexpr size_t OFF_PRF = OFF_MAT_C_INTT + kMatCBytes;
constexpr size_t OFF_Y_E1_E2 = OFF_PRF + kPrfTotalBytes;
constexpr size_t OFF_Y = OFF_Y_E1_E2; /**< NTT(y) 读端 = CBD 前 4 poly */
constexpr size_t OFF_E1 = OFF_Y_E1_E2 + kYBytes; /**< CBD e1（加噪） */
constexpr size_t OFF_E2 = OFF_E1 + kE1Bytes;     /**< CBD e2（加噪） */
constexpr size_t OFF_A_HAT = OFF_Y_E1_E2 + kYe1e2Bytes;
constexpr size_t OFF_Y_HAT = OFF_A_HAT + kAHatBytes;
constexpr size_t OFF_T_HAT = OFF_Y_HAT + kYHatBytes;
constexpr size_t OFF_M = OFF_T_HAT + kTHatBytes; /**< Host：原始 m[32]；非最终 μ */
constexpr size_t OFF_MU = OFF_M + kMsgBytes;     /**< 设备 μ←Decompress₁(m) */
constexpr size_t OFF_ZETAS = OFF_MU + kMuBytes;
constexpr size_t OFF_GAMMAS = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_U_HAT = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_V_HAT = OFF_U_HAT + kUHatBytes;
constexpr size_t OFF_U = OFF_V_HAT + kVHatBytes;
constexpr size_t OFF_V = OFF_U + kUBytes;
constexpr size_t OFF_C = OFF_V + kVBytes;
constexpr size_t OFF_TRACE = OFF_C + kCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;
constexpr uint16_t kFlagGate = 4;

/** TRACE 槽（相对 T20 增 MU_DONE）。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_PREP_DONE = 1;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 2;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 3;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 4;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 5;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 6;
constexpr uint32_t SLOT_AIV0_MUL_DONE = 7;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 8;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 9;
constexpr uint32_t SLOT_HOST_MID_SYNC = 10;
constexpr uint32_t SLOT_AIV0_PRE_SET1_INTT = 11;
constexpr uint32_t SLOT_AIC_POST_WAIT1_INTT = 12;
constexpr uint32_t SLOT_AIC_PRE_SET3_INTT = 13;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_INTT = 14;
constexpr uint32_t SLOT_AIV0_UV_DONE = 15;
constexpr uint32_t SLOT_AIV0_PACK_DONE = 16;
constexpr uint32_t SLOT_HOST_POST_SYNC = 17;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_NTT = 18;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 19;
constexpr uint32_t SLOT_AIV1_PRE_SET1_INTT = 20;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_INTT = 21;
constexpr uint32_t SLOT_AIV0_AHAT_DONE = 22;
constexpr uint32_t SLOT_AIV0_YHAT_DONE = 23;
constexpr uint32_t SLOT_AIV0_CBD_DONE = 24;
constexpr uint32_t SLOT_AIV0_BD12_DONE = 25;
constexpr uint32_t SLOT_AIV0_MU_DONE = 26;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_PREP_DONE = 0x50524550u; // "PREP"
constexpr uint32_t MAGIC_HOST_MID = 0x484F4D49u;  // "HOMI"
constexpr uint32_t MAGIC_HOST_POST = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_MUL_DONE = 0x4D554C44u; // "MULD"
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV0_UV_DONE = 0x5556444Eu;  // "UVDN"
constexpr uint32_t MAGIC_AIV0_PACK_DONE = 0x5041434Bu; // "PACK"
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_AIV0_AHAT_DONE = 0x41484154u; // "AHAT"
constexpr uint32_t MAGIC_AIV0_YHAT_DONE = 0x59484154u; // "YHAT"
constexpr uint32_t MAGIC_AIV0_CBD_DONE = 0x43424439u;  // "CBD9"
constexpr uint32_t MAGIC_AIV0_BD12_DONE = 0x42443132u; // "BD12"
constexpr uint32_t MAGIC_AIV0_MU_DONE = 0x4D553031u;  // "MU01"
constexpr uint32_t MAGIC_OUT_OK = 0x44050021u;         // T21
constexpr uint32_t MAGIC_PREP_MARK = 0x44055052u;      // T21P

} // namespace tiling

#endif
