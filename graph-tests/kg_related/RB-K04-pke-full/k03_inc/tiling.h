/** K04 拼装：自 RB-K0x 契约头复制，仅改 include 路径；算法未改。 */
/**
 * @file tiling.h
 * @brief RB-K03：KeyGen L2b — Â∘ŝ̂+ê̂ → ByteEncode₁₂ → ek_pke‖ρ / dk_pke。
 *
 * Flag 表（KB §B2 · MIX · 1/3(+4)）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 *   4 = GATE（本刀启用）
 * 永禁 5 / 7；AIC Wait 环内禁 SyncAll。
 *
 * 时序（本核内）：
 *   AIV: Set(4) → Set(1) → Wait(3) → 点积+BE+拼 ek/dk [AIV0]
 *   AIC: Wait(4) → Wait(1) → Cube → Set(3)
 *
 * 背景：接 K01 Â/ρ 与 K02 NTT(ŝ/ê)；本刀 gen_data 可自洽造同 SEED_D。
 * 结论：主验收 ek_pke(1568)/dk_pke(1536)；禁融 prep/NTT；禁抄 KeyGen 整核。
 */
#ifndef RB_K03_KG_DOT_ENCODE_TILING_H
#define RB_K03_KG_DOT_ENCODE_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4 polyvec；N=256；ByteEncode₁₂。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kGammaN = 128;
constexpr int32_t kAHatPolys = kKem * kKem; // 16

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kPolyvecBytes = static_cast<size_t>(kKem) * kPolyBytes;     // 4096
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kPolyBytes; // 16384
constexpr size_t kSNttBytes = kPolyvecBytes;
constexpr size_t kENttBytes = kPolyvecBytes;
constexpr size_t kTHatBytes = kPolyvecBytes;
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t); // 512
constexpr size_t kRhoBytes = 32;
/** 单 poly ByteEncode₁₂：256×12bit = 384B。 */
constexpr size_t kBe12PolyBytes = 384;
constexpr size_t kBe12VecBytes = static_cast<size_t>(kKem) * kBe12PolyBytes; // 1536
constexpr size_t kEkBytes = kBe12VecBytes + kRhoBytes;                        // 1568
constexpr size_t kDkBytes = kBe12VecBytes;                                    // 1536

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 14;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace 布局（字节偏移）：
 *   A_HAT / S_NTT / E_NTT / GAMMAS / RHO → Host H2D
 *   T_HAT / EK / DK → AIV0 写出（亦镜像独立 out 缓冲）
 *   MAT_* → 极轻 Cube；TRACE → 握手诊断
 *
 * Â 行主序：flat(p,j,c)=(p·k+j)·N+c（与 F203-innerproduct 契约一致）。
 */
constexpr size_t OFF_A_HAT = 0;
constexpr size_t OFF_S_NTT = OFF_A_HAT + kAHatBytes;
constexpr size_t OFF_E_NTT = OFF_S_NTT + kSNttBytes;
constexpr size_t OFF_GAMMAS = OFF_E_NTT + kENttBytes;
constexpr size_t OFF_RHO = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_T_HAT = OFF_RHO + kRhoBytes;
constexpr size_t OFF_EK = OFF_T_HAT + kTHatBytes;
constexpr size_t OFF_DK = OFF_EK + kEkBytes;
constexpr size_t OFF_MAT_A = OFF_DK + kDkBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C + kMatCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：1/3 + GATE(4)；永禁 5/7。 */
constexpr uint16_t kFlagGate = 4;
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;

/** TRACE 槽。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 2;
constexpr uint32_t SLOT_AIV0_PRE_SET1 = 3;
constexpr uint32_t SLOT_AIV1_PRE_SET1 = 4;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 5;
constexpr uint32_t SLOT_AIC_POST_WAIT1 = 6;
constexpr uint32_t SLOT_AIC_PRE_SET3 = 7;
constexpr uint32_t SLOT_AIV0_POST_WAIT3 = 8;
constexpr uint32_t SLOT_AIV1_POST_WAIT3 = 9;
constexpr uint32_t SLOT_AIV0_DONE = 10;
constexpr uint32_t SLOT_HOST_POST_SYNC = 11;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_DONE = 0x4B30334Fu; // "K03O"
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_OUT_OK = 0x4B30333Au; // "K03:"

} // namespace tiling

#endif
