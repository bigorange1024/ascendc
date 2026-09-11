/**
 * @file tiling.h
 * @brief RB-T01 MIX flag1/3 握手玩具：维度、GM 布局与 TRACE 槽位契约。
 *
 * 流水线位置：Host（main）与设备核（mmad_custom）共享；无算法语义，仅脚手架。
 * 对齐任务书：AIV SET(1) → AIC WAIT(1)+极轻 Cube → AIC SET(3) → AIV WAIT(3)。
 */
#ifndef RB_T01_MIX_NTT13_TILING_H
#define RB_T01_MIX_NTT13_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（k/n 对齐 32）。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kK = 32;
constexpr uint16_t kN = 32;

constexpr size_t kMatABytes = static_cast<size_t>(kM) * kK;                   // 512
constexpr size_t kMatBBytes = static_cast<size_t>(kK) * kN;                   // 1024
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t); // 2048
constexpr size_t kOutBytes = 64; /**< 握手完成魔数落盘，供 verify 冒烟 */

/**
 * TRACE 槽（uint32，设备写 / Host 读后打印）：
 * 编号分区见 trace_map.md（Host / AIV0 / AIV1 / AIC）。
 */
constexpr size_t kTraceSlots = 16;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/** GM workspace 线性布局（字节偏移，自 ws 基址）。 */
constexpr size_t MAT_A = 0;
constexpr size_t MAT_B = MAT_A + kMatABytes;
constexpr size_t MAT_C = MAT_B + kMatBBytes;
constexpr size_t TRACE = MAT_C + kMatCBytes;
constexpr size_t wssize = TRACE + kTraceBytes;

/** CrossCore flagId：与 Encrypt NTT 同构最短握手；永禁 5/7。 */
constexpr uint16_t kFlagAivReady = 1; /**< AIV→AIC：向量侧就绪 */
constexpr uint16_t kFlagAicDone = 3;  /**< AIC→AIV：Cube 完成 */

/** TRACE 槽位（与 trace_map.md 一致）。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET1 = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET1 = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT1 = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3 = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3 = 5;
constexpr uint32_t SLOT_AIV1_POST_WAIT3 = 6;
constexpr uint32_t SLOT_HOST_POST_SYNC = 7;

/** TRACE / out 魔数。 */
constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;       // Host launch 前
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;  // AIV0 即将 SET(1)
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;  // AIV1 即将 SET(1)
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u; // AIC 已 WAIT(1)
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;   // AIC Cube 完，即将 SET(3)
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u; // Host SynchronizeStream 后
constexpr uint32_t MAGIC_OUT_OK = 0x54F10013u;         // out 冒烟魔数

} // namespace tiling

#endif
