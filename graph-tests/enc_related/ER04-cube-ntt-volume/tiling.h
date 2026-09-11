#ifndef ER04_CUBE_NTT_VOLUME_TILING_H
#define ER04_CUBE_NTT_VOLUME_TILING_H

/**
 * @file tiling.h
 * @brief ER04：NTT/INTT 真 Cube 多轮加压的 GM 布局与 TRACE 槽。
 *
 * 基线：ER03 壳（GATE MAC 256×32 + ER02 同步纪律）。
 * 本刀（D-EXP-ER04 / ER04-TASK）已锁：
 *   - GATE：保持 kMacElems=256、kMacRounds=32（不回退、不再加）
 *   - Cube 几何：仍 C[16,32]=A[16,32]@B[32,32]
 *   - Cube 轮数：NTT 段与 INTT 段各 kCubeRounds=16 次真 Mmad（禁空转）
 *   Launch1 phase=PREP：轻量 prep 桩
 *   Launch2 phase=COMPUTE：NTT 1/3 + GATE 4/8 + INTT 复用 1/3
 * 禁：flag 5/7、Wait 中 SyncAll、自造 SoftSync、抄旧 Encrypt 核、假循环加压。
 */

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64 字节落盘）。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与其它探针 tiling 对齐 */
    int32_t phase;      /**< 0=PREP，1=COMPUTE（见 Er04Phase） */
};

/** Host 两次 launch 的 phase 字面量（与 ER01/ER03 同值，便于对照）。 */
enum Er04Phase : int32_t {
    ER04_PHASE_PREP = 0,
    ER04_PHASE_COMPUTE = 1,
};

/** 兼容脚手架命名（mmad_custom 内仍可写 ER01_PHASE_*）。 */
enum Er01Phase : int32_t {
    ER01_PHASE_PREP = ER04_PHASE_PREP,
    ER01_PHASE_COMPUTE = ER04_PHASE_COMPUTE,
};

namespace tiling {

/** Host 预填：32B seed（可不含真 SHAKE；Host 亦可另喂 LUT）。 */
constexpr size_t kSeedBytes = 32;
/** 每 AIV 写 64B sample 输出；共 128B。 */
constexpr size_t kSampleOutPerAiv = 64;
constexpr size_t kSampleOutBytes = kSampleOutPerAiv * 2;

/** Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（B=I₃₂）；几何锁自 ER04-TASK。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;

/**
 * NTT / INTT 两段 AIC 路径各跑本轮数次真 Mmad（禁空转 for）。
 * 来源：ER04-TASK「Cube 轮数 = kCubeRounds=16」；不得擅自改参。
 */
constexpr size_t kCubeRounds = 16;

constexpr size_t kS0Bytes = kRows * kDim;
constexpr size_t kLutBytes = kDim * kCols;
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t);
constexpr size_t kOutBytes = 64;
constexpr size_t kS0PerAiv = kS0Bytes / 2;

/**
 * GATE 真积木：近生产体量 int32[256]×32 轮 Mul/Add/Muls（ER03 已锁，本刀保持）。
 * 来源：ER03-TASK / ER04-TASK「GATE 保持 256×32」。
 */
constexpr size_t kMacElems = 256;
constexpr size_t kMacRounds = 32;
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
 *   [SAMPLE_OUT ] 128 B   uint8  Launch1 AIV mixing 输出（每 AIV 64B）
 *   [S0         ] 512 B   int8   Launch2 NTT 左矩阵（由 SAMPLE_OUT 铺）
 *   [LUT        ] 1024 B  int8   右矩阵 = I₃₂
 *   [MAT_C      ] 2048 B  int32  Cube 输出（多轮 Process 覆写同一缓冲）
 *   [MAC_A/B/ACC] 各 2×1024B int32 GATE Vec MAC（每 AIV 256×int32）
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
 * TRACE 逻辑槽（设备写 1；Host 按码表打印三位十进制编号）。
 * SAMPLE 段仅 Launch1 写；NTT/GATE/INTT 仅 Launch2 写。
 */
enum ToyTraceSlot : int32_t {
    /* ---- NTT 段（Launch2 第一轮 1/3）---- */
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
    /* ---- INTT 段（Launch2 第二轮 1/3，禁 5/7）---- */
    TR_AIV0_INTT_SET1 = 12,
    TR_AIV1_INTT_SET1 = 13,
    TR_AIC_INTT_WAIT1 = 14,
    TR_AIC_INTT_SET3 = 15,
    TR_AIV0_INTT_WAIT3 = 16,
    TR_AIV1_INTT_WAIT3 = 17,
    /* ---- SAMPLE 段（Launch1 prep，210–219 区）---- */
    TR_AIV0_SAMPLE_START = 18, /**< → 211 */
    TR_AIV1_SAMPLE_START = 19, /**< → 311 */
    TR_AIV0_SAMPLE_DONE = 20,  /**< → 212 */
    TR_AIV1_SAMPLE_DONE = 21,  /**< → 312 */
};

#endif
