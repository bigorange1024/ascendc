#ifndef ENCRYPT_CLEAN_HOSTMU_2LAUNCH_TILING_H
#define ENCRYPT_CLEAN_HOSTMU_2LAUNCH_TILING_H

/**
 * @file tiling.h
 * @brief 干净重写 Encrypt：Host 2-launch + 默认 Host μ + skipNtt 无 PrefixEmbed。
 *
 * PHASE：P0 握手骨架；P1a 加 L2 早 TRACE（ws+TRACE，Host 可读）。
 * 拓扑见 graph_tests/ENCRYPT_CLEAN_REWRITE.md。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2 拓扑。 */
struct TilingData {
    int32_t phase;    /**< 0=Launch1 prep+NTT；1=Launch2 skipNtt */
    int32_t reserved; /**< 保留 */
};

namespace tiling {

/** Cube：轻量 16×32×32（P0/P1a 不做 HEAVY 加压）。 */
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32;
constexpr int kCubeRoundsPerPhase = 1;

constexpr size_t kABytes = kRows * kDim;
constexpr size_t kLutBytes = kDim * kCols;
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t);
constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
constexpr size_t kStubVecElems = 64;

/** Host μ 折叠小缓冲：P0 用 8 个 int32 占位（非真 256 系数）。 */
constexpr size_t kMuFoldElems = 8;
constexpr size_t kMuFoldBytes = kMuFoldElems * sizeof(int32_t);

/**
 * P1a 段 TRACE（L2）：Host sync 后 D2H 读；服务「空 TRACE」问题。
 * 仅 AIV0 / AIC 写；槽位见 CleanTraceStage。
 */
constexpr int kTraceStages = 6;
constexpr size_t kTraceBytes = static_cast<size_t>(kTraceStages) * sizeof(int32_t);

/**
 * magic（verify）：
 *   out[0..7]  = ASCII "CLNENC01"
 *   out[8]     = 0x2A（P1a：干净 2-launch + Hostμ + 早 TRACE）
 *   out[9..63] = 0xA5
 */
constexpr char kMagicPrefix[8] = {'C', 'L', 'N', 'E', 'N', 'C', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicCleanHostMu = 0x2A; /**< P1a 标记（P0 曾为 0x21） */

/** Host 折 μ 写入 e2Fold[0] 的标记（'MU01'）；证明 launch2 前已折入。 */
constexpr int32_t kHostMuFoldMark = 0x4D553031;

/**
 * GM workspace：
 *   [S0]     左矩阵 A
 *   [LUT]    右矩阵 B=I₃₂
 *   [MAT_C]  Cube 输出
 *   [STUB]   stub 工作区
 *   [MU_FOLD] Host 折 μ 小缓冲
 *   [TRACE]  P1a L2 段标记 int32[kTraceStages]
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kABytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t STUB = MAT_C + kMatCBytes;
constexpr size_t MU_FOLD = STUB + kStubVecElems * sizeof(int32_t);
constexpr size_t TRACE = MU_FOLD + kMuFoldBytes;
constexpr size_t wssize = TRACE + kTraceBytes;

/** TilingData.phase 取值。 */
constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

/** L2 TRACE 槽（与 Host dump / verify 对齐）。 */
enum CleanTraceStage : int32_t {
    TR_AIV_BEFORE_ATJP = 0, /**< StubAtJp 前 */
    TR_AIV_AFTER_ATJP = 1,  /**< StubAtJp 后、SET(4) 前 */
    TR_AIV_AFTER_SET4 = 2,  /**< 双 AIV SET(4) 后 */
    TR_AIV_AFTER_GATE = 3,  /**< Wait(8) 后 */
    TR_AIC_AFTER_WAIT4 = 4, /**< AIC Wait(4) 返回后 */
    TR_AIC_AFTER_GATE = 5,  /**< AIC Set(8) 后 */
};

} // namespace tiling

#endif
