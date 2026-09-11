#ifndef PASS_TOY_DECRYPT_FSM_FUSED_SKEL1_TILING_H
#define PASS_TOY_DECRYPT_FSM_FUSED_SKEL1_TILING_H

/**
 * @file tiling.h
 * @brief pass-toy-decrypt-fsm-fused-skel1：Decrypt 同核融合握手骨架的维度与 workspace。
 *
 * 目标（DGT-20260903-3 / Q-TOY-FUSED-SKEL）：同构 SoftSync×2 + GATE 4/8×2 + NTT/INTT 1/3；
 * 不对 Alg.15 正确性；只认 SIM。Cube 取 int8 最小粒度 16×32×32。
 *
 * 背景：承接 F-TOY-SOFT-GATE-SIM-PASS；本刀叠第二轮 SoftSync/GATE 与 NTT/INTT。
 * 未采用：Encrypt 单 GATE 序；INTT flag 5/7；AIC Wait 中 SyncAll。
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
constexpr size_t kOutBytes = 64;                                /**< 完成标记：每 AIV 32B */

/** softSyncGm：生产同构 int32[2]（slot0=prep，slot1=su）；分配 ≥64B。 */
constexpr size_t kSoftSyncBytes = 64;

/**
 * 简易 TRACE 槽（GT-4 / Q-TOY-TRACE-DATACOPY）：
 * 每逻辑槽占 8×int32=32B；Host 预清零；设备 DataCopy 写块首=1。
 */
constexpr size_t kTraceSlots = 8;
constexpr size_t kTraceAlignInts = 8; /**< 每槽 DataCopy 元素数（32B） */
constexpr size_t kTraceBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t);
constexpr size_t kTraceOnesBytes = kTraceAlignInts * sizeof(int32_t); /**< AIC L1 源模板 */

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0         ] 512 B   int8   左矩阵；Host 预填常数（禁真 unpack）
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C      ] 2048 B  int32  Cube 输出（NTT/INTT 各一路极轻 MMAD）
 *   [TRACE_ONES ] 32 B    int32  全 1 模板，供 AIC ones→A1→槽 DataCopy
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t TRACE_ONES = MAT_C + kMatCBytes;
constexpr size_t wssize = TRACE_ONES + kTraceOnesBytes;

} // namespace tiling

/**
 * TRACE 槽下标（便于挂死时看最后置位段）。
 * AIV0：Soft0+GATE / NTT / Soft1+GATE / INTT；AIC：两轮 WAIT(4)/WAIT(1)。
 */
enum ToyTraceSlot : int32_t {
    TR_AIV_SOFT0_GATE = 0, /**< AIV0：SoftSync(slot0)+GATE WAIT(8)+Clear 后 */
    TR_AIV_NTT_DONE = 1,   /**< AIV0：NTT WAIT(3) 完成后 */
    TR_AIV_SOFT1_GATE = 2, /**< AIV0：SoftSync(slot1)+GATE WAIT(8)+Clear 后 */
    TR_AIV_INTT_DONE = 3,  /**< AIV0：INTT WAIT(3) 完成后 */
    TR_AIC_WAIT4_PREP = 4, /**< AIC：prep GATE WAIT(4) 过后 */
    TR_AIC_WAIT1_NTT = 5,  /**< AIC：NTT WAIT(1) 过后 */
    TR_AIC_WAIT4_SU = 6,   /**< AIC：su GATE WAIT(4) 过后 */
    TR_AIC_WAIT1_INTT = 7, /**< AIC：INTT WAIT(1) 过后 */
};

#endif
