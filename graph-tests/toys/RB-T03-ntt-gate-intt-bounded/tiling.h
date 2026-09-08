/**
 * @file tiling.h
 * @brief RB-T03：单 launch MIX — NTT(1/3) → GATE(4) → INTT(复用 1/3) + 两段极轻 Cube。
 *
 * Flag 表（永禁 5/7）：
 *   1 = AIV→AIC 就绪（NTT 段与 INTT 段复用；前段 Wait(3) 完成后才复用）
 *   3 = AIC→AIV 完成（同上复用）
 *   4 = AIV→AIC GATE
 *
 * 时序：
 *   AIC: Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV: Set(1)→Wait(3) → Set(4) → Set(1)→Wait(3)
 */
#ifndef RB_T03_NTT_GATE_INTT_BOUNDED_TILING_H
#define RB_T03_NTT_GATE_INTT_BOUNDED_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（NTT/INTT 各跑一趟）。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kK = 32;
constexpr uint16_t kN = 32;

constexpr size_t kMatABytes = static_cast<size_t>(kM) * kK;                   // 512
constexpr size_t kMatBBytes = static_cast<size_t>(kK) * kN;                   // 1024
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t); // 2048
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 20;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/** GM workspace：两份 mat_c 证明两段 Cube 均写出。 */
constexpr size_t MAT_A = 0;
constexpr size_t MAT_B = MAT_A + kMatABytes;
constexpr size_t MAT_C_NTT = MAT_B + kMatBBytes;
constexpr size_t MAT_C_INTT = MAT_C_NTT + kMatCBytes;
constexpr size_t TRACE = MAT_C_INTT + kMatCBytes;
constexpr size_t wssize = TRACE + kTraceBytes;

/**
 * CrossCore flagId：
 * 1/3 = NTT 与 INTT 握手（复用；GATE 插在两段之间）；4 = GATE；永禁 5/7。
 */
constexpr uint16_t kFlagAivReady = 1; /**< AIV→AIC：NTT/INTT 段就绪 */
constexpr uint16_t kFlagAicDone = 3;  /**< AIC→AIV：该段 Cube 完成 */
constexpr uint16_t kFlagGate = 4;     /**< AIV→AIC：GATE */

/** TRACE 槽位（与 trace_map.md 一致）。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 5;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 6;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 7;
constexpr uint32_t SLOT_HOST_POST_SYNC = 8;
constexpr uint32_t SLOT_AIV0_PRE_SET1_INTT = 9;
constexpr uint32_t SLOT_AIC_POST_WAIT1_INTT = 10;
constexpr uint32_t SLOT_AIC_PRE_SET3_INTT = 11;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_INTT = 12;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_INTT = 13;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 14;
constexpr uint32_t SLOT_AIV1_PRE_SET1_INTT = 15;
/** AIV1 NTT WAIT(3) 后；与槽 5 成对，供 NPU TRACE 落 AIV1 时验收（勿硬绑 AIV0）。 */
constexpr uint32_t SLOT_AIV1_POST_WAIT3_NTT = 16;

/** TRACE / out 魔数。 */
constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_OUT_OK = 0x54F30033u;

} // namespace tiling

#endif
