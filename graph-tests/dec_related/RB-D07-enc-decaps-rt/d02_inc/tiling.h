/**
 * @file tiling.h
 * @brief RB-D02：Decrypt L2a — NTT(u)+su_dot → û,ŵ。
 *
 * Flag 表（S0A / KB §B2）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 *   4 = GATE（可选收口；本刀启用）
 * 永禁 5 / 7；AIC Wait 环内禁 SyncAll。
 *
 * 时序（本核内，勿跨 launch 依赖 flag）：
 *   AIV: Set(4) → Set(1) → Wait(3) → NTT(u)→û → ΣMultiplyNTTs(ŝ,û)→ŵ [→ 业务写出]
 *   AIC: Wait(4) → Wait(1) → Cube → Set(3)
 *
 * 背景：接 D01 mid-sync 后的 u/ŝ；本刀独立 gen_data 造合法输入。
 * 结论：主验收 û/ŵ 对拍；poly-batch 整 poly；禁 Gather / limbsplit。
 */
#ifndef RB_D02_DECRYPT_NTT_DOT_TILING_H
#define RB_D02_DECRYPT_NTT_DOT_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4 polyvec；N=256。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kPolyvecBytes = static_cast<size_t>(kKem) * kPolyBytes;     // 4096
constexpr size_t kUBytes = kPolyvecBytes;
constexpr size_t kSHatBytes = kPolyvecBytes;
constexpr size_t kUHatBytes = kPolyvecBytes;
constexpr size_t kWHatBytes = kPolyBytes; // 单 poly ŵ
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t);

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
 *   U / S_HAT / ZETAS / GAMMAS → Host H2D
 *   U_HAT / W_HAT → AIV0 写出（亦镜像独立 out 缓冲）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_U = 0;
constexpr size_t OFF_S_HAT = OFF_U + kUBytes;
constexpr size_t OFF_ZETAS = OFF_S_HAT + kSHatBytes;
constexpr size_t OFF_GAMMAS = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_U_HAT = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_W_HAT = OFF_U_HAT + kUHatBytes;
constexpr size_t OFF_MAT_A = OFF_W_HAT + kWHatBytes;
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
constexpr uint32_t MAGIC_AIV0_DONE = 0x4430324Fu; // "D02O"
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_OUT_OK = 0x4430323Au; // "D02:"

} // namespace tiling

#endif
