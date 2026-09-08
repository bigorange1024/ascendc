/**
 * @file tiling.h
 * @brief RB-T14：设备 Â+ŷ 半链 — Host 预喂 ρ/y/ζ，单 launch MIX 同核顺序产出。
 *
 * Flag 表（对齐 T12/T13；本刀仅一段握手）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 永禁 5 / 7；本刀无 GATE(4)。
 *
 * 时序：
 *   AIC: Wait(1) → Cube → Set(3)
 *   AIV: Set(1) → Wait(3) → Â←SampleNTT(ρ) → ŷ←NTT(y)（仅 AIV0）
 *
 * 背景：T12 关行 16、T13 关行 3–7；本刀拼装两路设备半链。
 * 结论：双对拍 Â 与 ŷ；禁抄 alg14/encrypt/frozen；禁 Host 预喂最终 Â/ŷ。
 */
#ifndef RB_T14_DEVICE_AHAT_YHAT_TILING_H
#define RB_T14_DEVICE_AHAT_YHAT_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4；Â 为 16 poly；ŷ 为 4 poly。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kAHatPolys = kKem * kKem; // 16
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;

constexpr size_t kRhoBytes = 32;
constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kYBytes = static_cast<size_t>(kKem) * kPolyBytes;           // 4096
constexpr size_t kYHatBytes = kYBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t); // 512
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kPolyBytes;   // 16384

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
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
 *   RHO / Y / ZETAS → Host H2D（禁预喂最终 Â/ŷ）
 *   A_HAT / Y_HAT → AIV0 写出镜像（Â 亦可只写独立 aHatOut）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_RHO = 0;
constexpr size_t OFF_Y = OFF_RHO + kRhoBytes;
constexpr size_t OFF_ZETAS = OFF_Y + kYBytes;
constexpr size_t OFF_A_HAT = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_Y_HAT = OFF_A_HAT + kAHatBytes;
constexpr size_t OFF_MAT_A = OFF_Y_HAT + kYHatBytes;
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
constexpr uint32_t SLOT_AIV0_AHAT_DONE = 8;
constexpr uint32_t SLOT_AIV0_YHAT_DONE = 9;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_AHAT_DONE = 0x41484154u; // "AHAT"
constexpr uint32_t MAGIC_AIV0_YHAT_DONE = 0x59484154u; // "YHAT"
constexpr uint32_t MAGIC_OUT_OK = 0x543A003Eu;         // T14

} // namespace tiling

#endif
