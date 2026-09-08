/**
 * @file tiling.h
 * @brief RB-T16：设备 CBD coins→(y,e₁,e₂) — Host 预喂 coins[32]，MIX AIV0 做 PRF+Alg.8。
 *
 * Flag 表（对齐 T12/T13；本刀仅 CBD 握手段）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 永禁 5 / 7；本刀无 GATE(4)。
 *
 * 时序：
 *   AIC: Wait(1) → CubeSample → Set(3)
 *   AIV: Set(1) → Wait(3) → PRF(coins,N=0..8)→CBD_η₂ → y‖e1‖e2（AIV0）
 *
 * 背景：T07 Host 已定义 coins→(y,e₁,e₂) 契约；本刀把 CBD 搬到设备。
 * 结论：主验收 y/e1/e2 对拍；禁抄 encrypt/alg14/frozen；积木只 -I 活跃 alg8 + shake_xof。
 */
#ifndef RB_T16_DEVICE_CBD_Y_E_TILING_H
#define RB_T16_DEVICE_CBD_Y_E_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4；Encrypt 噪声 9 poly = y[4]+e1[4]+e2[1]。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kEta = 2;
constexpr int32_t kNoisePolys = 9; // y0..3 + e1_0..3 + e2
constexpr int32_t kQ = 3329;

constexpr size_t kCoinsBytes = 32;
/** PRF_η 输出 / poly：η·N/4 = 128。 */
constexpr size_t kPrfBytesPerPoly = static_cast<size_t>(kEta) * kPolyN / 4U; // 128
constexpr size_t kPrfTotalBytes = static_cast<size_t>(kNoisePolys) * kPrfBytesPerPoly; // 1152
constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kYeeBytes = static_cast<size_t>(kNoisePolys) * kPolyBytes;  // 9216
constexpr size_t kYBytes = static_cast<size_t>(kKem) * kPolyBytes;           // 4096
constexpr size_t kE1Bytes = kYBytes;
constexpr size_t kE2Bytes = kPolyBytes;

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（握手段有界真算）。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 12;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace：
 *   COINS → Host H2D（禁预喂最终 y/e1/e2）
 *   PRF   → AIV0 SHAKE256 写出（中间）
 *   YEE   → AIV0 CBD 镜像（可选；主结果在独立 yeeOut）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_COINS = 0;
constexpr size_t OFF_PRF = OFF_COINS + kCoinsBytes;
constexpr size_t OFF_YEE = OFF_PRF + kPrfTotalBytes;
constexpr size_t OFF_MAT_A = OFF_YEE + kYeeBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C + kMatCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：仅 1/3；永禁 5/7。 */
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;

/** TRACE 槽。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET1 = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET1 = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT1 = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3 = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3 = 5;
constexpr uint32_t SLOT_HOST_POST_SYNC = 6;
constexpr uint32_t SLOT_AIV1_POST_WAIT3 = 7;
/** AIV0：y/e1/e2 写完（软）。 */
constexpr uint32_t SLOT_AIV0_CBD_DONE = 8;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_CBD_DONE = 0x43424439u; // "CBD9"
constexpr uint32_t MAGIC_OUT_OK = 0x543A0010u;        // T16

} // namespace tiling

#endif
