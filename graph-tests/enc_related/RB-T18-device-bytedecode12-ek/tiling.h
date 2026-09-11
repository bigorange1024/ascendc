/**
 * @file tiling.h
 * @brief RB-T18：设备 ByteDecode₁₂(ek)→t̂ — Host 预喂 ek，MIX AIV0 解出 t̂[4×256]。
 *
 * Flag 表（对齐 T12/T16 半链；本刀仅 Decode 握手段）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 永禁 5 / 7；本刀无 GATE(4)。
 *
 * 时序：
 *   AIC: Wait(1) → CubeSample → Set(3)
 *   AIV: Set(1) → Wait(3) → ByteDecode₁₂(ek[0:1536]) → t̂（AIV0）
 *
 * I/O 契约（对齐 T05 / FIPS Alg.14 行 2，勿大段抄 T05 实现）：
 *   ek[1568] = BE₁₂(t̂)[1536] ‖ ρ[32]；本刀只解前 1536B，ρ 仅占位接 prep。
 *   输出 t̂[1024] int32，按 poly 连续。
 *
 * 背景：T05 已关独立 AIV Decode₁₂；本刀把同契约放进 Encrypt 外形 MIX。
 * 结论：主验收 t̂ 对拍；禁抄 encrypt/alg14/frozen；积木只 #include shared 头。
 */
#ifndef RB_T18_DEVICE_BYTEDECODE12_EK_TILING_H
#define RB_T18_DEVICE_BYTEDECODE12_EK_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4；ByteDecode₁₂ 每 poly 384B → 256 系数。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr size_t kPolyPackedBytes = 384; // 12*256/8
constexpr size_t kEkBodyBytes = static_cast<size_t>(kKem) * kPolyPackedBytes; // 1536
constexpr size_t kRhoBytes = 32;
/** 完整 ek 外形（接 Encrypt prep）：体 ‖ ρ。 */
constexpr size_t kEkBytes = kEkBodyBytes + kRhoBytes; // 1568
constexpr size_t kTHatCoeffs = static_cast<size_t>(kKem) * static_cast<size_t>(kPolyN); // 1024
constexpr size_t kTHatBytes = kTHatCoeffs * sizeof(int32_t); // 4096

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
 *   EK    → Host H2D（禁预喂最终 t̂）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_EK = 0;
constexpr size_t OFF_MAT_A = OFF_EK + kEkBytes;
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
/** AIV0：t̂ 写完（软）。 */
constexpr uint32_t SLOT_AIV0_BD12_DONE = 8;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_BD12_DONE = 0x42443132u; // "BD12"
constexpr uint32_t MAGIC_OUT_OK = 0x543A0012u;         // T18

} // namespace tiling

#endif
