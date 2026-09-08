/**
 * @file tiling.h
 * @brief RB-T13：设备 Â←SampleNTT(ρ) — Host 预喂 ρ[32]，MIX AIV0 做 16×Alg.7。
 *
 * Flag 表（对齐 T12；本刀仅 SampleNTT 握手段）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 永禁 5 / 7；本刀无 GATE(4)。
 *
 * 时序：
 *   AIC: Wait(1) → CubeSample → Set(3)
 *   AIV: Set(1) → Wait(3) → Â←SampleNTT(ρ‖j‖i)（AIV0；k=4→16 poly）
 *
 * 背景：T07 Host ρ←ek 尾；T12 证明 MIX 半链可上板；本刀关 Encrypt 行 3–7 设备侧。
 * 结论：主验收 Â 对拍 + 不挂；禁抄 alg14/encrypt/frozen；SampleNTT 非三段式 NTT。
 */
#ifndef RB_T13_DEVICE_SAMPLE_NTT_A_TILING_H
#define RB_T13_DEVICE_SAMPLE_NTT_A_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4；Â 为 k×k=16 个 NTT 域 poly。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kAHatPolys = kKem * kKem; // 16
constexpr int32_t kQ = 3329;

constexpr size_t kRhoBytes = 32;
constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kPolyBytes;  // 16384

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
 *   RHO → Host H2D（禁预喂最终 Â）
 *   A_HAT → AIV0 SampleNTT 写出（亦镜像到独立 aHatOut）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_RHO = 0;
constexpr size_t OFF_A_HAT = OFF_RHO + kRhoBytes;
constexpr size_t OFF_MAT_A = OFF_A_HAT + kAHatBytes;
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
/** AIV0：Â 写完（软）。 */
constexpr uint32_t SLOT_AIV0_AHAT_DONE = 8;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_AHAT_DONE = 0x41484154u; // "AHAT"
constexpr uint32_t MAGIC_OUT_OK = 0x543A003Du;         // T13

} // namespace tiling

#endif
