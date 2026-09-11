/**
 * @file tiling.h
 * @brief RB-T02 GATE 时序玩具：AIC 先 Wait(4) + 极轻 Cube + NTT 段 flag 1/3。
 *
 * 对齐任务书：生产 Encrypt 常见 GATE 外形 —— AIC 侧先进入 Wait(4)（由 AIV Set(4) 唤醒），
 * 再极轻计算；随后仍用 1/3 做最短 NTT 握手。永禁 flag 5/7。
 */
#ifndef RB_T02_GATE_TIMING_WAIT4_TILING_H
#define RB_T02_GATE_TIMING_WAIT4_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kK = 32;
constexpr uint16_t kN = 32;

constexpr size_t kMatABytes = static_cast<size_t>(kM) * kK;                   // 512
constexpr size_t kMatBBytes = static_cast<size_t>(kK) * kN;                   // 1024
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t); // 2048
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 16;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/** GM workspace 线性布局（字节偏移，自 ws 基址）。 */
constexpr size_t MAT_A = 0;
constexpr size_t MAT_B = MAT_A + kMatABytes;
constexpr size_t MAT_C = MAT_B + kMatBBytes;
constexpr size_t TRACE = MAT_C + kMatCBytes;
constexpr size_t wssize = TRACE + kTraceBytes;

/**
 * CrossCore flagId：
 * 4 = GATE（AIV→AIC，IP_DONE 同构外形）；1/3 = NTT 最短握手；永禁 5/7。
 */
constexpr uint16_t kFlagGate = 4;     /**< AIV→AIC：向量侧 GATE 就绪 */
constexpr uint16_t kFlagAivReady = 1; /**< AIV→AIC：NTT 段就绪 */
constexpr uint16_t kFlagAicDone = 3;  /**< AIC→AIV：Cube+握手完成 */

/** TRACE 槽位（与 trace_map.md 一致）。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3 = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3 = 5;
constexpr uint32_t SLOT_AIV1_POST_WAIT3 = 6;
constexpr uint32_t SLOT_HOST_POST_SYNC = 7;
constexpr uint32_t SLOT_AIV0_PRE_SET1 = 8;
constexpr uint32_t SLOT_AIV1_PRE_SET1 = 9;
constexpr uint32_t SLOT_AIC_POST_WAIT1 = 10;

/** TRACE / out 魔数。 */
constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_OUT_OK = 0x54F20024u;

} // namespace tiling

#endif
