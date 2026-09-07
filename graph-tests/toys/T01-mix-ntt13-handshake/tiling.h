#ifndef T01_MIX_NTT13_HANDSHAKE_TILING_H
#define T01_MIX_NTT13_HANDSHAKE_TILING_H

/**
 * @file tiling.h
 * @brief T01：最短 MIX NTT 同构握手（flag 1/3）的维度与 workspace。
 *
 * 目标（D-EXP-T01）：只验证 AIV SET(1)→AIC WAIT(1)+极轻 Cube→AIC SET(3)→AIV WAIT(3)
 * 在 SIM 能否跑完；不对算法正确性。矩阵取 Cube int8 最小粒度 16×32×32。
 */

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64 字节落盘）。本玩具无 mixPass 分段。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与其它探针 tiling 对齐 */
    int32_t reserved;   /**< 保留 */
};

namespace tiling {

/** Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（B=I₃₂）。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;

constexpr size_t kS0Bytes = kRows * kDim;                      /**< 左矩阵 A：512 B int8 */
constexpr size_t kLutBytes = kDim * kCols;                     /**< 右矩阵 B：1024 B int8 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t); /**< C：2048 B */
constexpr size_t kOutBytes = 64;                               /**< 桩输出：仅写完成标记 */
constexpr size_t kS0PerAiv = kS0Bytes / 2;                     /**< 每 AIV 写 256 int8 */

/**
 * TRACE：每逻辑槽 8×int32=32B（DataCopy 对齐）；块首放 KB 编号或 1。
 * 槽序见 ToyTraceSlot / trace_map.md。
 */
constexpr size_t kTraceAlignInts = 8;
constexpr size_t kTraceSlots = 6;
constexpr size_t kTraceBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t);
constexpr size_t kTraceOnesBytes = kTraceAlignInts * sizeof(int32_t);

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0         ] 512 B   int8   左矩阵；AIV0→[0:256)，AIV1→[256:512)
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C      ] 2048 B  int32  Cube 输出（本玩具 AIV 不消费，仅占位）
 *   [TRACE_ONES ] 32 B    int32  全 1 模板，供 AIC L1←GM→槽 DataCopy
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t TRACE_ONES = MAT_C + kMatCBytes;
constexpr size_t wssize = TRACE_ONES + kTraceOnesBytes;

} // namespace tiling

/**
 * TRACE 逻辑槽（设备写 1；Host 按码表打印 KB 编号）。
 * 编号约定：Host 100s / AIV0 200s / AIV1 300s / AIC 400s（见 KB §6）。
 */
enum ToyTraceSlot : int32_t {
    TR_AIV0_SET1 = 0,  /**< → 201：AIV0 SET(1) 前 */
    TR_AIV1_SET1 = 1,  /**< → 301：AIV1 SET(1) 前 */
    TR_AIC_WAIT1 = 2,  /**< → 401：AIC WAIT(1) 后 */
    TR_AIC_SET3 = 3,   /**< → 402：AIC SET(3) 前 */
    TR_AIV0_WAIT3 = 4, /**< → 203：AIV0 WAIT(3) 后 */
    TR_AIV1_WAIT3 = 5, /**< → 303：AIV1 WAIT(3) 后 */
};

#endif
