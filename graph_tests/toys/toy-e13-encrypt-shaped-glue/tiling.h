#ifndef TOY_E13_ENCRYPT_SHAPED_GLUE_TILING_H
#define TOY_E13_ENCRYPT_SHAPED_GLUE_TILING_H

/**
 * @file tiling.h
 * @brief E13：E12 积木 + Encrypt 形态粘合（L1 采样 / L2 代数+压码 → c=c1||c2）。
 *
 * L1（采样）：SHAKE；CBD×2→src（u/r 路）；CBD×1→ws[E0]（v/e2 路）；prf=3×128B。
 * L2（代数+压码）：
 *   u 路 c1 = poly0∥poly1 各 NTT→basemul→INTT→Compress→ByteEncode（**无** μ 嵌入）→ out[0:256]
 *   v 路 c2 = e2 真链 + Decompress_1(μ) → out[256:384]
 * 公钥 ĝ：g.bin 768 int32（ĝ_u0∥ĝ_u1∥ĝ_v stub）；A/Minv 同 E12 固定 toy 矩阵。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 采样 / L2 代数+压码；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 采样；1=L2 代数+压码 + SET(4) */
    int32_t tileLength;  /**< 单 poly 长度，固定 256 */
};

namespace tiling {

constexpr size_t k = 2;   /**< ML-KEM-512 模块秩（u 路两 poly） */
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

/** G0/G1/G2：u0/u1/v 各自 basemul 的 ĝ int32[256]（v 为 stub 公钥材料）。 */
constexpr size_t G0 = A3 + n * 4 * sizeof(int32_t);
constexpr size_t G1 = G0 + n * sizeof(int32_t);
constexpr size_t G2 = G1 + n * sizeof(int32_t);

constexpr size_t Minv0 = G2 + n * sizeof(int32_t);
constexpr size_t Minv1 = Minv0 + n * n;
constexpr size_t Minv2 = Minv1 + n * n;
constexpr size_t Minv3 = Minv2 + n * n;

/** W0/W1：L2 u 路每 poly INTT 后工作 int32[256]。 */
constexpr size_t W0 = Minv3 + n * n;
constexpr size_t W1 = W0 + n * sizeof(int32_t);

/** E0：L1 v 路 CBD 噪声 e2 int32[256]；W2：L2 v 路工作 poly。 */
constexpr size_t E0 = W1 + n * sizeof(int32_t);
constexpr size_t W2 = E0 + n * sizeof(int32_t);

/** P0：L1 PRF 384B = u0∥u1∥v 各 128B。 */
constexpr size_t P0 = W2 + n * sizeof(int32_t);
constexpr size_t kPrfBytesPerPoly = 128;
constexpr size_t kPrfPolys = k + 1; /**< u×2 + v×1 */
constexpr size_t kPrfBytes = kPrfBytesPerPoly * kPrfPolys;

/** MU0：L2 v 路 Decompress_1 消息 μ 32B（Host H2D）。 */
constexpr size_t MU0 = P0 + ((kPrfBytes + 31) / 32) * 32;
constexpr size_t kMuBytes = 32;

constexpr size_t wssize = MU0 + ((kMuBytes + 31) / 32) * 32;

constexpr size_t kWorkBytes = n * sizeof(int32_t);
constexpr size_t kEncodeBytesPerPoly = 128;
constexpr size_t kC1Bytes = kEncodeBytesPerPoly * k; /**< u 路 c1 = 2×128B */
constexpr size_t kC2Bytes = kEncodeBytesPerPoly;     /**< v 路 c2 = 128B */
constexpr size_t kOutBytes = kC1Bytes + kC2Bytes;    /**< c = c1||c2 = 384B */
constexpr size_t kSrcBytes = k * n * sizeof(int32_t);
constexpr size_t kGBytes = (k + 1) * n * sizeof(int32_t);
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
