#ifndef T06_GATE_REAL_BRICK_TILING_H
#define T06_GATE_REAL_BRICK_TILING_H

/**
 * @file tiling.h
 * @brief T06：NTT 1/3 + 生产 GATE 4/8（AIV 真积木 Vec MAC）+ INTT 复用 1/3（禁 5/7）。
 *
 * 目标（D-EXP-T06）：单 launch 全 FSM；GATE 段 AIV 做有界真 Vec MAC，禁 X14 假循环加码。
 * 矩阵仍取 Cube int8 最小粒度 16×32×32（AIC NTT/INTT 极轻 MMAD）。
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
 * GATE 真积木：有界 int32 Vec MAC（GM↔UB，AscendC::Mul + Add）。
 * 每 AIV：a[64]、b[64]、acc[64] int32；8 轮 MAC 累加。
 */
constexpr size_t kMacElems = 64;
constexpr size_t kMacRounds = 8;
constexpr size_t kMacVecBytes = kMacElems * sizeof(int32_t); /**< 256 B/向量 */
constexpr size_t kMacPerAivBytes = kMacVecBytes * 3;         /**< a+b+acc = 768 B/AIV */
constexpr size_t kMacTotalBytes = kMacPerAivBytes * 2;       /**< 双 AIV */

/**
 * TRACE：每逻辑槽 8×int32=32B（DataCopy 对齐）；块首放 1。
 * 槽序见 ToyTraceSlot / trace_map.md；须能区分 NTT / GATE / INTT 三段。
 */
constexpr size_t kTraceAlignInts = 8;
constexpr size_t kTraceSlots = 18;
constexpr size_t kTraceBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t);
constexpr size_t kTraceOnesBytes = kTraceAlignInts * sizeof(int32_t);

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0         ] 512 B   int8   左矩阵；AIV0→[0:256)，AIV1→[256:512)
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C      ] 2048 B  int32  Cube 输出（占位）
 *   [MAC_A      ] 512 B   int32  GATE Vec MAC 左操作数（每 AIV 64 int32）
 *   [MAC_B      ] 512 B   int32  GATE Vec MAC 右操作数
 *   [MAC_ACC    ] 512 B   int32  GATE Vec MAC 累加器
 *   [TRACE_ONES ] 32 B    int32  全 1 模板，供 AIC L1←GM→槽 DataCopy
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
/** MAC 区：a/b/acc 各 512B（双 AIV × 64 int32）；见 MAC_A_OFF 系列偏移。 */
constexpr size_t MAC_A_OFF = MAT_C + kMatCBytes;
constexpr size_t MAC_B_OFF = MAC_A_OFF + kMacVecBytes * 2;
constexpr size_t MAC_ACC_OFF = MAC_B_OFF + kMacVecBytes * 2;

constexpr size_t TRACE_ONES = MAC_ACC_OFF + kMacVecBytes * 2;
constexpr size_t wssize = TRACE_ONES + kTraceOnesBytes;

} // namespace tiling

/**
 * TRACE 逻辑槽（设备写 1；Host 按码表打印 KB 编号）。
 * 编号约定：Host 100s / AIV0 200s / AIV1 300s / AIC 400s（见 KB §6）。
 * INTT 段用 206/306/405/406/207/307，与 NTT 段 201/301/401/402/203/303 区分。
 */
enum ToyTraceSlot : int32_t {
    /* ---- NTT 段（第一轮 1/3）---- */
    TR_AIV0_SET1 = 0,       /**< → 201：AIV0 SET(1) 前 */
    TR_AIV1_SET1 = 1,       /**< → 301：AIV1 SET(1) 前 */
    TR_AIC_WAIT1 = 2,       /**< → 401：AIC WAIT(1) 后 */
    TR_AIC_SET3 = 3,        /**< → 402：AIC SET(3) 前 */
    TR_AIV0_WAIT3 = 4,      /**< → 203：AIV0 WAIT(3) 后（NTT 完成） */
    TR_AIV1_WAIT3 = 5,      /**< → 303：AIV1 WAIT(3) 后（NTT 完成） */
    /* ---- GATE 段（生产时序 4/8）---- */
    TR_AIC_WAIT4 = 6,       /**< → 403：AIC 进入 WAIT(4) 前（占坑前） */
    TR_AIV0_SET4 = 7,       /**< → 204：AIV0 真积木后、SET(4) 前 */
    TR_AIV1_SET4 = 8,       /**< → 304：AIV1 真积木后、SET(4) 前 */
    TR_AIC_SET8 = 9,        /**< → 404：AIC WAIT(4) 返回后、SET(8) 前 */
    TR_AIV0_WAIT8 = 10,     /**< → 205：AIV0 WAIT(8) 后 */
    TR_AIV1_WAIT8 = 11,     /**< → 305：AIV1 WAIT(8) 后 */
    /* ---- INTT 段（第二轮 1/3，禁 5/7）---- */
    TR_AIV0_INTT_SET1 = 12, /**< → 206：AIV0 INTT SET(1) 前 */
    TR_AIV1_INTT_SET1 = 13, /**< → 306：AIV1 INTT SET(1) 前 */
    TR_AIC_INTT_WAIT1 = 14, /**< → 405：AIC INTT WAIT(1) 后 */
    TR_AIC_INTT_SET3 = 15,  /**< → 406：AIC INTT SET(3) 前 */
    TR_AIV0_INTT_WAIT3 = 16,/**< → 207：AIV0 INTT WAIT(3) 后 */
    TR_AIV1_INTT_WAIT3 = 17,/**< → 307：AIV1 INTT WAIT(3) 后 */
};

#endif
