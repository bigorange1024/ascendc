#ifndef FIX_TOY_DECRYPT_FSM_SOFT_GATE1_TILING_H
#define FIX_TOY_DECRYPT_FSM_SOFT_GATE1_TILING_H

/**
 * @file tiling.h
 * @brief fix-toy-decrypt-fsm-soft-gate1：SoftSyncArrive + 一轮 GATE 4↔8 的轻量 MIX 布局。
 *
 * 目标（DGT-20260903-2 / Q-TOY-SOFT-GATE）：SoftSync 后再接 Decrypt prep 段末同构
 * GATE（双 AIV SET(4)→AIC WAIT(4)/MMAD/SET(8)→双 AIV WAIT(8)）；不对算法正确性。
 * Cube 取 int8 最小粒度 16×32×32。本刀无第二轮 GATE、无 NTT/INTT。
 */

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64 字节落盘）。本玩具无 mixPass 分段。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与其它探针 tiling 对齐 */
    int32_t reserved;   /**< 保留 */
};

namespace tiling {

/** Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（B=I₃₂，Host 预填）。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;

constexpr size_t kS0Bytes = kRows * kDim;                       /**< 左矩阵 A：512 B int8 */
constexpr size_t kLutBytes = kDim * kCols;                      /**< 右矩阵 B：1024 B int8 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t);  /**< C：2048 B */

/** softSyncGm：生产同构 int32[2] 语义，分配 ≥64B（F-HOST-ZERO-SOFTSYNC）。 */
constexpr size_t kSoftSyncBytes = 64;
constexpr int32_t kSoftSyncSlot = 0; /**< 本刀只用 slot0 */

/**
 * TRACE / out：GT-4 风格每槽 8×int32=32B。
 *   slot0 = SoftSync+GATE 后 AIV0 完成标记
 *   slot1 = SoftSync+GATE 后 AIV1 完成标记（证明已出 busy-wait 且过 WAIT(8)）
 */
constexpr size_t kTraceAlignInts = 8;
constexpr size_t kTraceSlots = 2;
constexpr size_t kOutBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t); /**< 64 B */

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0   ] 512 B   int8   左矩阵；本刀 Host 预填
 *   [LUT  ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C] 2048 B  int32  Cube 输出（GATE 段 AIC MMAD）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t wssize = MAT_C + kMatCBytes;

} // namespace tiling

#endif
