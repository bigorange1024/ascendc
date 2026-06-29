#ifndef TOY_MIX_S123_TILING_H
#define TOY_MIX_S123_TILING_H

/**
 * @file tiling.h
 * @brief pass-toy-mix-s123-byteencode-k2 的维度常量、GM workspace 布局与 host 侧 tiling 结构。
 *
 * 设计目标：用最小 64³ 矩阵乘验证 MIX 三阶段流水（S1 双 AIV → S2 Cube → S3+encode 双 AIV），
 * 不含跨 AIV 数据交换。详见 TOY_MIX_S123.md。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel 的运行时参数（写入 input/tiling.bin，固定 64 字节对齐填充）。 */
struct TilingData {
    int32_t tileLength; /**< 保留字段，当前未使用 */
    int32_t mixPass;    /**< 0=全流程；1=仅 S1；2=仅 S2；3=仅 S3+encode */
};

namespace tiling {

// ---- 矩阵维度（Cube matmul: C[64×64] = A[64×64] @ B[64×64]）----
constexpr size_t kRows = 64;
constexpr size_t kCols = 64;
constexpr size_t kDim = 64; /**< K 维，与行/列相同，构成 64³ */

// ---- GM 输入 / 输出规模 ----
constexpr size_t kSrcTotal = 2048;              /**< src 总系数个数（int32） */
constexpr size_t kSrcPerAiv = kSrcTotal / 2;    /**< 每 AIV 读 1024 int32（方案 B 行分片） */
constexpr size_t kS0Bytes = kRows * kCols;        /**< 左矩阵 A：4096 int8 */
constexpr size_t kLutBytes = kRows * kCols;       /**< 右矩阵 B：4096 int8（单位阵） */
constexpr size_t kMatCBytes = kRows * kCols * sizeof(int32_t); /**< Cube 输出 C：16384 B */
constexpr size_t kOutPerAiv = kS0Bytes / 2;      /**< 每 AIV 写 2048 int8 */
constexpr size_t kOutTotal = kS0Bytes;            /**< out 总长 4096 int8 */

/**
 * GM workspace 线性布局（字节偏移，从 ws 基址起算）：
 *
 *   [S0     ] 4096 B  int8   左矩阵 A，行优先 flat；AIV0→[0:2048]，AIV1→[2048:4096]
 *   [LUT    ] 4096 B  int8   右矩阵 B = I₆₄，由 host 预填
 *   [MAT_C  ] 16384 B int32  Cube 输出 C = A @ B；AIV0 读 [0:2048]，AIV1 读 [2048:4096]
 */
constexpr size_t S0 = 0;
constexpr size_t LUT = S0 + kS0Bytes;
constexpr size_t MAT_C = LUT + kLutBytes;
constexpr size_t wssize = MAT_C + kMatCBytes;

} // namespace tiling

#endif
