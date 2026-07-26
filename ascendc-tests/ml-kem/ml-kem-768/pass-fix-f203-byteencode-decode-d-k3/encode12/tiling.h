#ifndef BYTE_ENCODE12_TILING_H
#define BYTE_ENCODE12_TILING_H

/**
 * @file tiling.h
 * @brief ByteEncode₁₂-only probe（k=4，2×AIV 各编码 2 poly）编译期几何与运行时 TilingData。
 *
 * 流水线位置：host/device 共用；main 按此算 GM 缓冲大小，kernel 按此切分 ŝ/ê 与 poly 批。
 * 与 golden 关系：dst [12,256]、t_hat [4,256]、ek/sk 各 4×384 B 与 gen_data / verify 契约一致。
 * 作用：定义 n、kS/kE、dst 行偏移、文件字节数及 ByteEncode 输出尺寸。
 */
#include <cstddef>
#include <cstdint>

/**
 * 运行时 tiling（本探针仅 tileLength）。
 * tileLength：每 poly 系数数 n=256，由 GenerateTiling 填充。
 */
struct TilingData {
    int32_t tileLength; /**< n = 256 */
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t kS = 4;
constexpr size_t kE = 4;
/** 每 AIV 编码的 poly 数（ê 批与 ek/sk 批同为 2） */
constexpr size_t kEPerAiv = kE / 2;
constexpr size_t kHatK = kS;

/** dst [12,256]：s_aiv0[0..3] | s_aiv1[4..7] | e_aiv0[8..9] | e_aiv1[10..11] */
constexpr size_t kDstPolys = 2 * kS + kE;
constexpr size_t dstSOffAiv0 = 0;
constexpr size_t dstSOffAiv1 = kS;
constexpr size_t dstEOff = 2 * kS;
constexpr size_t dstEOffAiv0 = dstEOff;
constexpr size_t dstEOffAiv1 = dstEOff + kEPerAiv;

constexpr size_t dstFileBytes = kDstPolys * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);

} // namespace tiling

namespace byte_encode {
/** 单 poly 编码输出 384 B；polyvec = kHatK × 384 */
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
