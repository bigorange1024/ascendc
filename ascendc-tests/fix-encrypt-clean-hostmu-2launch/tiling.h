#ifndef ENCRYPT_CLEAN_HOSTMU_2LAUNCH_TILING_H
#define ENCRYPT_CLEAN_HOSTMU_2LAUNCH_TILING_H

/**
 * @file tiling.h
 * @brief 干净重写 Encrypt P0：Host 2-launch + **默认 Host μ** + 设备 skipNtt 无 PrefixEmbed。
 *
 * 拓扑（graph_tests/ENCRYPT_CLEAN_REWRITE.md §3；结构即约束，无「开关才正确」路径）：
 *   Host L1：prep+NTT（一轮 Cube；无设备 μ）
 *   HostFold：始终 e₂+=μ 占位写入小缓冲（非调试开关）
 *   Host L2：skipNtt — AIC 入口 Wait(4)；AIV 无 PrefixEmbed；短 stub→双 AIV SET(4)
 *            → GATE 4↔8 → INTT-like → magic pack
 *
 * P0 不对 ML-KEM golden；verify 只查 magic。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2 拓扑。 */
struct TilingData {
    int32_t phase;    /**< 0=Launch1 prep+NTT；1=Launch2 skipNtt */
    int32_t reserved; /**< 保留 */
};

namespace tiling {

/** Cube：轻量 16×32×32（P0 不做 HEAVY 加压）。 */
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
 * magic（verify）：
 *   out[0..7]  = ASCII "CLNENC01"
 *   out[8]     = 0x21（干净 2-launch + Host μ by construction）
 *   out[9..63] = 0xA5
 */
constexpr char kMagicPrefix[8] = {'C', 'L', 'N', 'E', 'N', 'C', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicCleanHostMu = 0x21;

/** Host 折 μ 写入 e2Fold[0] 的标记（'MU01'）；证明 launch2 前已折入。 */
constexpr int32_t kHostMuFoldMark = 0x4D553031;

/**
 * GM workspace：
 *   [S0]     左矩阵 A
 *   [LUT]    右矩阵 B=I₃₂
 *   [MAT_C]  Cube 输出
 *   [STUB]   stub 工作区
 *   [MU_FOLD] Host 折 μ 小缓冲（L2 前 H2D；设备只读标记，不做 PrefixEmbed）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kABytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t STUB = MAT_C + kMatCBytes;
constexpr size_t MU_FOLD = STUB + kStubVecElems * sizeof(int32_t);
constexpr size_t wssize = MU_FOLD + kMuFoldBytes;

/** TilingData.phase 取值。 */
constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
