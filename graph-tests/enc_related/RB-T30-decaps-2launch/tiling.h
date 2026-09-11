/**
 * @file tiling.h
 * @brief RB-T30：ML-KEM-1024 Alg.21 Decaps — 2 Host launch 融合实验。
 *
 * Host 布局（已锁）：
 *   L1 MIX `t30_dec_fused_custom`：Decrypt 全链（prep→SyncAll→NTT+dot→INTT+extract）→ m'
 *   L2 MIX `t30_enc_fused_custom`：Re-Encrypt/Encaps 全链（prep 前缀 G/CBD/Â…→SyncAll→NTT 路径）→ c'/K'
 *   Host FO：c'≡c ? K' : J(z‖c)
 *
 * Flag：∈{1,3}；L2 可加 4=GATE；永禁 5/7；禁 SoftSync；Wait 环内禁 SyncAll。
 * BLOCK_DIM=1；Compress₁ 常数路径 C=41285357。
 * magic out：0x54333032 ("T302")。
 *
 * 禁抄 RB-T28/T29/D08/D09/stable/frozen 核源码；本头仅常量契约。
 */
#ifndef RB_T30_TILING_H
#define RB_T30_TILING_H

#include <cstddef>
#include <cstdint>

/** Decrypt / Encaps 共用占位 tiling（≤64B）。 */
struct T30DecTilingData {
    int32_t reserved0;
    int32_t reserved1;
};

struct T30EncTilingData {
    int32_t reserved0;
    int32_t reserved1;
};

/** Decrypt 侧 workspace 与 TRACE（Alg.15）。 */
namespace t30_dec {

constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;
constexpr int32_t kInttScale = 3303; // 128^{-1} mod q
constexpr int32_t kDu = 11;
constexpr int32_t kDv = 5;
/** Compress₁：(C·u + 2^{35}) >> 36 & 1。 */
constexpr int64_t kCompressC = 41285357LL;
constexpr int32_t kCompress1Shift = 36;
constexpr int64_t kCompress1Bias = 1LL << 35;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t);
constexpr size_t kSHatBytes = static_cast<size_t>(kKem) * kPolyBytes;
constexpr size_t kUBytes = kSHatBytes;
constexpr size_t kVBytes = kPolyBytes;
constexpr size_t kUHatBytes = kUBytes;
constexpr size_t kWHatBytes = kPolyBytes;
constexpr size_t kWBytes = kPolyBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t);

constexpr size_t kDkBytes = 1536;
constexpr size_t kCBytes = 1568;
constexpr size_t kC1Bytes = 32 * static_cast<size_t>(kDu) * static_cast<size_t>(kKem); // 1408
constexpr size_t kC2Bytes = 32 * static_cast<size_t>(kDv);                             // 160
constexpr size_t kMBytes = 32;

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
 * Decrypt ws（dk/c 不镜像；prep 直读 H2D）：
 *   S_HAT → U → V → ZETAS → GAMMAS → U_HAT → W_HAT → W →
 *   MAT_A → MAT_B → MAT_C_NTT → MAT_C_INTT → TRACE → M → PREP_MARK
 */
constexpr size_t OFF_S_HAT = 0;
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

constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_PREP_DONE = 1;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 3;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 4;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 5;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 6;
constexpr uint32_t SLOT_AIV0_NTT_DOT_DONE = 7;
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
constexpr uint32_t MAGIC_PREP_DONE = 0x50524550u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_NTT_DOT_DONE = 0x4E545444u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV0_EXTRACT_DONE = 0x4D455854u;
constexpr uint32_t MAGIC_HOST_POST = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_PREP_MARK = 0x54335031u; // "T0P1"
constexpr uint32_t MAGIC_OUT_OK = 0x54333032u;    // "T302"

} // namespace t30_dec

/** Re-Encrypt / Encaps 侧 workspace 与 TRACE（Alg.14/16/17 外形）。 */
namespace t30_enc {

constexpr int32_t kKem = 4;
constexpr int32_t kK = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kInttScale = 3303;
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;
constexpr int32_t kEta = 2;
constexpr int32_t kNoisePolys = 9;

constexpr size_t kPolyPackedBytes = 384;
constexpr size_t kEkBodyBytes = static_cast<size_t>(kKem) * kPolyPackedBytes;
constexpr size_t kEkBytes = 1568;
constexpr size_t kCoinsBytes = 32;
constexpr size_t kRhoBytes = 32;
constexpr size_t kMsgBytes = 32;
constexpr size_t kHashBytes = 32;
constexpr size_t kSharedKeyBytes = 32;
constexpr size_t kGOutBytes = 64;
constexpr size_t kPrfBytesPerPoly = static_cast<size_t>(kEta) * kPolyN / 4U;
constexpr size_t kPrfTotalBytes = static_cast<size_t>(kNoisePolys) * kPrfBytesPerPoly;
constexpr size_t kYe1e2Bytes = 9 * kPolyN * sizeof(int32_t);
constexpr size_t kYBytes = static_cast<size_t>(kKem) * kPolyN * sizeof(int32_t);

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t);
constexpr size_t kAHatBytes = static_cast<size_t>(kKem) * kKem * kPolyBytes;
constexpr size_t kYHatBytes = static_cast<size_t>(kKem) * kPolyBytes;
constexpr size_t kTHatCoeffs = static_cast<size_t>(kKem) * static_cast<size_t>(kPolyN);
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

constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 30;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * Encaps ws（两段同核共用）：
 *   COINS/RHO/PREP_MARK/EK/MAT_A|B|C/PRF/Y_E1_E2/A_HAT/Y_HAT/T_HAT/
 *   M/MU/ZETAS/GAMMAS/U_HAT/V_HAT/U/V/C/K/H/TRACE
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
constexpr size_t OFF_Y = OFF_Y_E1_E2;
constexpr size_t OFF_E1 = OFF_Y_E1_E2 + kYBytes;
constexpr size_t OFF_E2 = OFF_E1 + kE1Bytes;
constexpr size_t OFF_A_HAT = OFF_Y_E1_E2 + kYe1e2Bytes;
constexpr size_t OFF_Y_HAT = OFF_A_HAT + kAHatBytes;
constexpr size_t OFF_T_HAT = OFF_Y_HAT + kYHatBytes;
constexpr size_t OFF_M = OFF_T_HAT + kTHatBytes;
constexpr size_t OFF_MU = OFF_M + kMsgBytes;
constexpr size_t OFF_ZETAS = OFF_MU + kMuBytes;
constexpr size_t OFF_GAMMAS = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_U_HAT = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_V_HAT = OFF_U_HAT + kUHatBytes;
constexpr size_t OFF_U = OFF_V_HAT + kVHatBytes;
constexpr size_t OFF_V = OFF_U + kUBytes;
constexpr size_t OFF_C = OFF_V + kVBytes;
constexpr size_t OFF_K = OFF_C + kCBytes;
constexpr size_t OFF_H = OFF_K + kSharedKeyBytes;
constexpr size_t OFF_TRACE = OFF_H + kHashBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;
constexpr uint16_t kFlagGate = 4;

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
constexpr uint32_t SLOT_AIV0_G_DONE = 27;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_PREP_DONE = 0x50524550u;
constexpr uint32_t MAGIC_HOST_POST = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_MUL_DONE = 0x4D554C44u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV0_UV_DONE = 0x5556444Eu;
constexpr uint32_t MAGIC_AIV0_PACK_DONE = 0x5041434Bu;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_AIV0_AHAT_DONE = 0x41484154u;
constexpr uint32_t MAGIC_AIV0_YHAT_DONE = 0x59484154u;
constexpr uint32_t MAGIC_AIV0_CBD_DONE = 0x43424439u;
constexpr uint32_t MAGIC_AIV0_BD12_DONE = 0x42443132u;
constexpr uint32_t MAGIC_AIV0_MU_DONE = 0x4D553031u;
constexpr uint32_t MAGIC_AIV0_G_DONE = 0x47303132u;
constexpr uint32_t MAGIC_OUT_OK = 0x54333032u;  // "T302"
constexpr uint32_t MAGIC_PREP_MARK = 0x54335032u; // "T0P2"

} // namespace t30_enc

#endif
