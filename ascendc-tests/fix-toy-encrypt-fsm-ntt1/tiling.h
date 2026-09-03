#ifndef FIX_TOY_ENCRYPT_FSM_NTT1_TILING_H
#define FIX_TOY_ENCRYPT_FSM_NTT1_TILING_H

/**
 * @file tiling.h
 * @brief fix-toy-encrypt-fsm-ntt1：极轻 MIX 握手玩具的维度与 workspace 布局。
 *
 * 目标（GT-20260903-1 / Q-TOY-NTT）：只验证 Encrypt NTT 同构 CrossCore flag 1/3
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

constexpr size_t kS0Bytes = kRows * kDim;                 /**< 左矩阵 A：512 B int8 */
constexpr size_t kLutBytes = kDim * kCols;                /**< 右矩阵 B：1024 B int8 */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t); /**< C：2048 B */
constexpr size_t kOutBytes = 64;                          /**< 桩输出：仅写完成标记 */
constexpr size_t kS0PerAiv = kS0Bytes / 2;                /**< 每 AIV 写 256 int8 */

/**
 * GM workspace（字节偏移，自 ws 起）：
 *   [S0   ] 512 B   int8   左矩阵；AIV0→[0:256)，AIV1→[256:512)
 *   [LUT  ] 1024 B  int8   右矩阵 = I₃₂，host 预填
 *   [MAT_C] 2048 B  int32  Cube 输出（本玩具 AIV 不消费，仅占位）
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t wssize = MAT_C + kMatCBytes;

} // namespace tiling

#endif
