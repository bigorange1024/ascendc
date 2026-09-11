#ifndef PASS_TOY_ENCRYPT_FSM_L18_SKEL1_TILING_H
#define PASS_TOY_ENCRYPT_FSM_L18_SKEL1_TILING_H

/**
 * @file tiling.h
 * @brief pass-toy-encrypt-fsm-l18-skel1：同核 NTT→GATE→INTT 骨架的维度与 workspace。
 *
 * 目标（GT-20260903-3 / Q-TOY-FUSED-L18-SKEL）：验证 Encrypt `l18_l19` 同核三段
 * 握手在 SIM 能否跑完，并写简易 TRACE 槽；不对算法正确性。
 * 矩阵取 Cube int8 最小粒度 16×32×32（NTT / INTT 各一路极轻 MMAD）。
 *
 * GT-20260903-7：`phase` 支持两段 Host launch（模仿 F-SIM-LAUNCH）；默认 FULL 与 GT-5/6 兼容。
 */

#include <cstddef>
#include <cstdint>

/**
 * Host→Device 运行时参数（固定 64 字节落盘）。
 * phase：控制本 launch 跑哪些段（见 ToyLaunchPhase）。
 */
struct TilingData {
    int32_t tileLength; /**< 占位，与其它探针 tiling 对齐 */
    int32_t phase;      /**< ToyLaunchPhase：FULL / NTT_ONLY / GATE_INTT_ONLY */
};

/**
 * 本 launch 设备侧相位（Host 写入 tiling.phase）。
 * 背景=模仿生产 Encrypt 2 Host launch（F-SIM-LAUNCH）；结论=两段 ACLRT_LAUNCH；
 * 未采用=同核 fused 当唯一加压手段。
 */
enum ToyLaunchPhase : int32_t {
    PHASE_FULL = 0,           /**< 单 launch 全链路（默认；GT-5/6） */
    PHASE_NTT_ONLY = 1,       /**< launch0：仅 NTT 1/3（含 AIV μ 桩前缀） */
    PHASE_GATE_INTT_ONLY = 2, /**< launch1：跳过 NTT，仅 GATE 4↔8 + INTT 1/3 */
};

namespace tiling {

/** Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（B=I₃₂）。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;

constexpr size_t kS0Bytes = kRows * kDim;                       /**< 左矩阵 A：512 B int8 */
constexpr size_t kLutBytes = kDim * kCols;                      /**< 右矩阵 B：1024 B int8 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t);  /**< C：2048 B */
constexpr size_t kOutBytes = 64;                                /**< 完成标记：每 AIV 32B */
constexpr size_t kS0PerAiv = kS0Bytes / 2;                      /**< 每 AIV 写 256 int8 */

/**
 * 简易 TRACE 槽（GT-20260903-4 / Q-TOY-TRACE-DATACOPY）：
 * 每逻辑槽占 8×int32=32B，满足 DataCopy 连续块 32B 对齐；禁标量直写 `__gm__`。
 * Host 预清零；设备 DataCopy 写该槽块首元素=1；同步后 Host 按 stride 打印。
 */
constexpr size_t kTraceSlots = 8;
constexpr size_t kTraceAlignInts = 8; /**< 每槽 DataCopy 元素数（32B） */
constexpr size_t kTraceBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t);
constexpr size_t kTraceOnesBytes = kTraceAlignInts * sizeof(int32_t); /**< AIC L1 源模板 */

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0         ] 512 B   int8   左矩阵；AIV0→[0:256)，AIV1→[256:512)
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C      ] 2048 B  int32  Cube 输出（本玩具 AIV 不消费，仅占位）
 *   [TRACE_ONES ] 32 B    int32  全 1 模板，供 AIC L1←GM→槽 DataCopy（Cube 无 UB Duplicate）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t TRACE_ONES = MAT_C + kMatCBytes;
constexpr size_t wssize = TRACE_ONES + kTraceOnesBytes;

} // namespace tiling

/**
 * TRACE 槽下标（仿 l18-trace 精神，简化）。
 * AIV0：μ桩后 / NTT后 / GATE后 / INTT后；AIC：WAIT(1)后 / WAIT(4)后。
 */
enum ToyTraceSlot : int32_t {
    TR_AIV_MU_STUB = 0,    /**< AIV0：桩前缀（μ 意图）完成后 */
    TR_AIV_NTT_DONE = 1,   /**< AIV0：NTT WAIT(3) 完成后 */
    TR_AIV_GATE_DONE = 2,  /**< AIV0：GATE WAIT(8) 完成后 */
    TR_AIV_INTT_DONE = 3,  /**< AIV0：INTT WAIT(3) 完成后 */
    TR_AIC_WAIT1_NTT = 4,  /**< AIC：NTT 段 WAIT(1) 过后 */
    TR_AIC_WAIT4_GATE = 5, /**< AIC：GATE WAIT(4) 过后 */
    TR_AIC_WAIT1_INTT = 6, /**< AIC：INTT 段 WAIT(1) 过后（复用 1） */
    TR_RESERVED = 7,       /**< 保留；凑满 8 槽 */
};

#endif
