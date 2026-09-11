#ifndef T07_SAMPLING_THEN_FSM_TILING_H
#define T07_SAMPLING_THEN_FSM_TILING_H

/**
 * @file tiling.h
 * @brief T07：SAMPLE 前置 stub → NTT 1/3 + 生产 GATE 4/8（AIV 真 Vec MAC）+ INTT 复用 1/3（禁 5/7）。
 *
 * 目标（D-EXP-T07）：Host 填 seed + SHA3 参考；AIV 轻量向量 mixing 写 SAMPLE_OUT；
 * 再跑 T06 全 FSM；TRACE 区分 SAMPLE / NTT / GATE / INTT 四段。
 */

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64 字节落盘）。本玩具无 mixPass 分段。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与其它探针 tiling 对齐 */
    int32_t reserved;   /**< 保留 */
};

namespace tiling {

/** Host 预填：32B seed（Encaps 式 urandom 占位，不对 KAT）。 */
constexpr size_t kSeedBytes = 32;
/** 每 AIV 写 64B sample 输出；共 128B。 */
constexpr size_t kSampleOutPerAiv = 64;
constexpr size_t kSampleOutBytes = kSampleOutPerAiv * 2;

/** Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（B=I₃₂）。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;

constexpr size_t kS0Bytes = kRows * kDim;
constexpr size_t kLutBytes = kDim * kCols;
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t);
constexpr size_t kOutBytes = 64;
constexpr size_t kS0PerAiv = kS0Bytes / 2;

constexpr size_t kMacElems = 64;
constexpr size_t kMacRounds = 8;
constexpr size_t kMacVecBytes = kMacElems * sizeof(int32_t);
constexpr size_t kMacPerAivBytes = kMacVecBytes * 3;
constexpr size_t kMacTotalBytes = kMacPerAivBytes * 2;

constexpr size_t kTraceAlignInts = 8;
constexpr size_t kTraceSlots = 22;
constexpr size_t kTraceBytes = kTraceSlots * kTraceAlignInts * sizeof(int32_t);
constexpr size_t kTraceOnesBytes = kTraceAlignInts * sizeof(int32_t);

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [SEED       ] 32 B    uint8  Host 预填 seed
 *   [SAMPLE_OUT ] 128 B   uint8  AIV 轻量 mixing 输出（每 AIV 64B）
 *   [S0         ] 512 B   int8   NTT 左矩阵
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂
 *   [MAT_C      ] 2048 B  int32  Cube 输出
 *   [MAC_A/B/ACC] 各 512B int32 GATE Vec MAC
 *   [TRACE_ONES ] 32 B    int32  AIC TRACE 模板
 */
constexpr size_t SEED = 0;
constexpr size_t SAMPLE_OUT = SEED + kSeedBytes;
constexpr size_t S0 = SAMPLE_OUT + kSampleOutBytes;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t MAC_A_OFF = MAT_C + kMatCBytes;
constexpr size_t MAC_B_OFF = MAC_A_OFF + kMacVecBytes * 2;
constexpr size_t MAC_ACC_OFF = MAC_B_OFF + kMacVecBytes * 2;
constexpr size_t TRACE_ONES = MAC_ACC_OFF + kMacVecBytes * 2;
constexpr size_t wssize = TRACE_ONES + kTraceOnesBytes;

} // namespace tiling

/**
 * TRACE 逻辑槽（设备写 1；Host 按码表打印 KB 编号）。
 * SAMPLE 段用 211/212/311/312（210–219 区）；NTT/GATE/INTT 同 T06。
 */
enum ToyTraceSlot : int32_t {
    /* ---- NTT 段（第一轮 1/3）---- */
    TR_AIV0_SET1 = 0,
    TR_AIV1_SET1 = 1,
    TR_AIC_WAIT1 = 2,
    TR_AIC_SET3 = 3,
    TR_AIV0_WAIT3 = 4,
    TR_AIV1_WAIT3 = 5,
    /* ---- GATE 段（生产时序 4/8）---- */
    TR_AIC_WAIT4 = 6,
    TR_AIV0_SET4 = 7,
    TR_AIV1_SET4 = 8,
    TR_AIC_SET8 = 9,
    TR_AIV0_WAIT8 = 10,
    TR_AIV1_WAIT8 = 11,
    /* ---- INTT 段（第二轮 1/3，禁 5/7）---- */
    TR_AIV0_INTT_SET1 = 12,
    TR_AIV1_INTT_SET1 = 13,
    TR_AIC_INTT_WAIT1 = 14,
    TR_AIC_INTT_SET3 = 15,
    TR_AIV0_INTT_WAIT3 = 16,
    TR_AIV1_INTT_WAIT3 = 17,
    /* ---- SAMPLE 段（FSM 之前，210–219 区）---- */
    TR_AIV0_SAMPLE_START = 18, /**< → 211：AIV0 SAMPLE mixing 前 */
    TR_AIV1_SAMPLE_START = 19, /**< → 311：AIV1 SAMPLE mixing 前 */
    TR_AIV0_SAMPLE_DONE = 20,  /**< → 212：AIV0 SAMPLE 写 GM 后 */
    TR_AIV1_SAMPLE_DONE = 21,  /**< → 312：AIV1 SAMPLE 写 GM 后 */
};

#endif
