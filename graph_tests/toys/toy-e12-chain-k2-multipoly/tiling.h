#ifndef TOY_E12_CHAIN_K2_MULTIPOLY_TILING_H
#define TOY_E12_CHAIN_K2_MULTIPOLY_TILING_H

/**
 * @file tiling.h
 * @brief E12：E11 壳扩 k=2（两路 poly 真链）；共享 2-launch + SET(4)。
 *
 * L1：SHAKE 短向量写 out 前缀；CBD 读 ws[P0] PRF 256B（2×128B），写 src[512] int32。
 * L2：每 poly 串行 NTT→basemul→INTT→+Decompress_1(μ)→Compress_d→ByteEncode_d；
 *     工作区 ws[W0]/ws[W1]；输出 out=2×128B 拼接。
 * μ：Host 写 input/mu.bin → ws[MU0]（32B，两 poly 共享同一 μ）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256（单 poly 系数）。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 真 SHAKE+CBD×2；1=L2 真链×2 + SET(4) */
    int32_t tileLength;  /**< 单 poly 长度，固定 256 */
};

namespace tiling {

constexpr size_t k = 2;   /**< ML-KEM-512 模块秩（本 toy 两路 poly） */
constexpr size_t n = 256;

constexpr size_t M0 = 0;
constexpr size_t M1 = M0 + n * n;
constexpr size_t M2 = M1 + n * n;
constexpr size_t M3 = M2 + n * n;

constexpr size_t S0 = M3 + n * n;
constexpr size_t S1 = S0 + n;
constexpr size_t S2 = S1 + n;
constexpr size_t S3 = S2 + n;

constexpr size_t A0 = S3 + n;
constexpr size_t A1 = A0 + n * 4 * sizeof(int32_t);
constexpr size_t A2 = A1 + n * 4 * sizeof(int32_t);
constexpr size_t A3 = A2 + n * 4 * sizeof(int32_t);

/** G0/G1：两路 poly 各自 basemul 的 ĝ int32[256]。 */
constexpr size_t G0 = A3 + n * 4 * sizeof(int32_t);
constexpr size_t G1 = G0 + n * sizeof(int32_t);

constexpr size_t Minv0 = G1 + n * sizeof(int32_t);
constexpr size_t Minv1 = Minv0 + n * n;
constexpr size_t Minv2 = Minv1 + n * n;
constexpr size_t Minv3 = Minv2 + n * n;

/** W0/W1：L2 每 poly INTT 后工作 int32[256]（避免 encode 覆写下一 poly）。 */
constexpr size_t W0 = Minv3 + n * n;
constexpr size_t W1 = W0 + n * sizeof(int32_t);

/** P0：L1 CBD 的 PRF 输入 256B（2×η=2·N/4）；32B 对齐。 */
constexpr size_t P0 = W1 + n * sizeof(int32_t);
constexpr size_t kPrfBytes = 128 * k;

/** MU0：L2 Decompress_1 消息 μ 32B（Host H2D；两 poly 共享）。 */
constexpr size_t MU0 = P0 + ((kPrfBytes + 31) / 32) * 32;
constexpr size_t kMuBytes = 32;

constexpr size_t wssize = MU0 + ((kMuBytes + 31) / 32) * 32;

constexpr size_t kWorkBytes = n * sizeof(int32_t);
constexpr size_t kEncodeBytesPerPoly = 128;
constexpr size_t kEncodeBytes = kEncodeBytesPerPoly * k;
constexpr size_t kOutBytes = kEncodeBytes;
constexpr size_t kSrcBytes = k * n * sizeof(int32_t);
constexpr size_t kGBytes = k * n * sizeof(int32_t);
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
