#ifndef BYTE_ENCODE12_TILING_H
#define BYTE_ENCODE12_TILING_H

/**
 * @file tiling.h
 * @brief ByteEncode₁₂-only probe（k=4，2×AIV 各编码 2 poly）。
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t kS = 4;
constexpr size_t kE = 4;
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
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
