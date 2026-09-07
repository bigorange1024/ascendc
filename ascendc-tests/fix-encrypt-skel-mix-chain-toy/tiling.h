#ifndef ENCRYPT_SKEL_MIX_CHAIN_TOY_TILING_H
#define ENCRYPT_SKEL_MIX_CHAIN_TOY_TILING_H

/**
 * @file tiling.h
 * @brief Encrypt 任务链骨架 toy 的维度常量与 GM workspace 布局。
 *
 * 流水线位置：Host（main.cpp / gen_data）与设备侧（mmad_custom / aic / aiv）共用。
 * 目标：模仿 Encrypt「prep → NTT-like → inner → [GATE 4/8] → INTT-like → encode」串接形态；
 * **不对** ML-KEM 正确性；验证 1 次 MIX launch + CrossCore 1/3 + 可选 GATE 4/8 + 可选 Cube 加压。
 *
 * 编译开关：
 *   SKEL_GATE=0/1（默认 1）：关则无 CrossCore 4/8（仅 SKEL_SKIPNTT=0 时生效）。
 *   SKEL_HEAVY=0/1（默认 0 接近基线；TASK-003 测 1）：
 *     0 → Cube 16×32×32，GATE 前后各 1 轮 flag 1/3（共 2 轮）
 *     1 → Cube 16×64×64，GATE 前后各 2 轮 flag 1/3（共 4 轮，仍 1 MIX launch）
 *   SKEL_SKIPNTT=0/1（默认 0；TASK-004）：1 → AIC 入口即 Wait(4)；AIV stub 后 SET(4)。
 *   SKEL_OMIT_SET4=0/1（默认 0；仅与 SKIPNTT=1 联用）：1 → AIV 故意不 SET(4)（故障注入）。
 *   SKEL_HOST_MU=0/1（默认 1；仅与 SKIPNTT=1 联用；TASK-005）：
 *     1 → Host launch 前写 μ 折入占位；设备跳过 μ-stub，尽快双 AIV SET(4)
 *     0 → 设备 AIV0 做 μ-stub（小块 DataCopy GM↔UB），再双 AIV SET(4)
 */

#include <cstddef>
#include <cstdint>

#ifndef SKEL_HEAVY
#define SKEL_HEAVY 0
#endif

/** Host 传入 kernel 的运行时参数（写入 input/tiling.bin，固定 64 字节）。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与参考壳对齐 */
    int32_t reserved;   /**< 保留 */
};

namespace tiling {

// ---- Cube 矩阵维度（int8：k/n 须为 32 倍数；m=16 为硬件最小行块）----
constexpr size_t kRows = 16;
#if SKEL_HEAVY
constexpr size_t kCols = 64;
constexpr size_t kDim = 64; /**< K 维；HEAVY=16×64×64 */
/** GATE 前 / 后各自的 flag 1/3 Cube 轮数（合计 ≥4） */
constexpr int kCubeRoundsPerPhase = 2;
#else
constexpr size_t kCols = 32;
constexpr size_t kDim = 32; /**< K 维；基线 16×32×32 */
constexpr int kCubeRoundsPerPhase = 1;
#endif

constexpr size_t kABytes = kRows * kDim;                 /**< 左矩阵 A */
constexpr size_t kLutBytes = kDim * kCols;               /**< 右矩阵 B=Iₙ，host 预填 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t); /**< C */
constexpr size_t kSrcBytes = 64;                         /**< 输入占位（stub hash 不依赖内容） */
constexpr size_t kOutBytes = 64;                         /**< 输出：固定 magic 长度 */
constexpr size_t kStubVecElems = 64;                     /**< stub_inner 向量长度（int32） */

/**
 * 输出 magic 约定（verify 只查这些）：
 *   out[0..7]  = ASCII "SKELENC1"
 *   out[8]     = 0xA5（基线）/ 0x04（GATE）
 *              / 0x14（skipNtt + 设备 μ-stub，HOST_MU=0）
 *              / 0x15（skipNtt + Host 折 μ，HOST_MU=1）
 *   out[9..63] = 0xA5 重复
 * （SKEL_HEAVY / OMIT_SET4 不改 magic；由环境变量与日志区分）
 */
constexpr char kMagicPrefix[8] = {'S', 'K', 'E', 'L', 'E', 'N', 'C', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicGateMark = 0x04;       /**< GATE 加压路径 out[8] 标记 */
constexpr uint8_t kMagicSkipNttMark = 0x14;    /**< skipNtt + 设备 μ-stub */
constexpr uint8_t kMagicHostMuMark = 0x15;     /**< skipNtt + Host 折 μ */

/** Host 折 μ 占位写入 STUB[0] 的标记（非真 μ；仅证明 launch 前已「折入」） */
constexpr int32_t kHostMuFoldMark = 0x4D553031; /**< ASCII 解释：'MU01' */

/**
 * GM workspace 线性布局（字节偏移，从 ws 基址起算）：
 *
 *   [S0     ] kABytes   int8   NTT/INTT 共用左矩阵槽（每轮 AIV0 重填）
 *   [LUT    ] kLutBytes int8   右矩阵 B=Iₙ，host 预填
 *   [MAT_C  ] kMatCBytes int32 Cube 输出（每轮覆盖）
 *   [STUB   ] 256 B     int32  stub_inner 工作缓冲（64×int32）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kABytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t STUB = MAT_C + kMatCBytes;
constexpr size_t wssize = STUB + kStubVecElems * sizeof(int32_t);

} // namespace tiling

#endif
