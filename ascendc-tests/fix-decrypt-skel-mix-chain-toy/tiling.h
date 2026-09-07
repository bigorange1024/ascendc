#ifndef DECRYPT_SKEL_MIX_CHAIN_TOY_TILING_H
#define DECRYPT_SKEL_MIX_CHAIN_TOY_TILING_H

/**
 * @file tiling.h
 * @brief Decrypt fused 握手骨架 toy 的维度常量与 GM workspace 布局。
 *
 * 流水线位置：Host（main.cpp / gen_data）与设备侧（mmad_custom / aic / aiv）共用。
 * 对齐生产 `f203_decrypt_device_fused` 的 **SoftSync + 两轮 GATE 4/8 + stub Cube** 握手；
 * **不对** ML-KEM 正确性；不实现真 unpack / su_dot / NTT。
 *
 * 编译开关（三者互斥，run.sh 强制）：
 *   SKEL_OMIT_SET4=0/1（默认 0）：1 → 双 AIV 两轮都不 SET(4)（故障注入，预期 SIM 124）
 *   SKEL_OMIT_SLOT0=0/1（默认 0）：1 → AIV0 不写 SoftSync slot0；AIV1 自旋；SoftSync 为 SET(4) 前置
 *   SKEL_OMIT_SET4_R2=0/1（默认 0）：1 → 仅第二轮 GATE(slot1) 不 SET(4)；第一轮仍 SET(4)
 *
 * Cube：固定 16×32×32 int8；每段 NTT-like / INTT-like 各 1 轮 flag 1/3（INTT 不用 flag 2）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel 的运行时参数（写入 input/tiling.bin，固定 64 字节）。 */
struct TilingData {
    int32_t tileLength; /**< 占位，与参考壳对齐 */
    int32_t reserved;   /**< 保留 */
};

namespace tiling {

// ---- Cube 矩阵维度（int8：k/n 须为 32 倍数；m=16 为硬件最小行块）----
constexpr size_t kRows = 16;
constexpr size_t kCols = 32;
constexpr size_t kDim = 32; /**< K 维；轻量 16×32×32 */
constexpr int kCubeRoundsPerPhase = 1;

constexpr size_t kABytes = kRows * kDim;                       /**< 左矩阵 A */
constexpr size_t kLutBytes = kDim * kCols;                     /**< 右矩阵 B=Iₙ，host 预填 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t); /**< C */
constexpr size_t kSrcBytes = 64;                               /**< 输入占位（stub 不依赖内容） */
constexpr size_t kOutBytes = 64;                               /**< 输出：固定 magic 长度 */
constexpr size_t kStubVecElems = 64;                           /**< stub_prep/dot 向量长度（int32） */

/** softSyncGm：int32[2]，slot0=prep、slot1=dot；Host launch 前必须清零 */
constexpr size_t kSoftSyncSlots = 2;
constexpr size_t kSoftSyncBytes = 64; /**< 64B 对齐分配，实际只用前 2×int32 */

/**
 * 输出 magic 约定（verify 只查这些）：
 *   out[0..7]  = ASCII "SKELDEC1"
 *   out[8]     = 0x04（合法握手档）
 *   out[9..63] = 0xA5 重复
 * （OMIT_SET4 路径预期挂死，不写 magic）
 */
constexpr char kMagicPrefix[8] = {'S', 'K', 'E', 'L', 'D', 'E', 'C', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicOkMark = 0x04; /**< 合法 SoftSync+GATE 路径 out[8] */

/**
 * GM workspace 线性布局（字节偏移，从 ws 基址起算）：
 *
 *   [S0     ] kABytes   int8   NTT/INTT 共用左矩阵槽（每轮 AIV0 重填）
 *   [LUT    ] kLutBytes int8   右矩阵 B=Iₙ，host 预填
 *   [MAT_C  ] kMatCBytes int32 Cube 输出（每轮覆盖）
 *   [STUB   ] 256 B     int32  stub_prep / stub_dot 工作缓冲（64×int32）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kABytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t STUB = MAT_C + kMatCBytes;
constexpr size_t wssize = STUB + kStubVecElems * sizeof(int32_t);

} // namespace tiling

#endif
