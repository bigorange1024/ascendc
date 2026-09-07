#ifndef TOY_E10_CHAIN_PLUS_BYTEENCODE_TILING_H
#define TOY_E10_CHAIN_PLUS_BYTEENCODE_TILING_H

/**
 * @file tiling.h
 * @brief E10：E09 壳 + L2 Compress 后真 ByteEncode_d(d=4) + SET(4)。
 *
 * L1：SHAKE 短向量写 out 前缀；CBD 读 ws[P0] PRF，写 src[256]。
 * L2：NTT→basemul→INTT→Compress_d→ByteEncode_d；最终 dst = ByteEncode(Compress(…)) 128B。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 真 SHAKE+CBD；1=L2 真 NTT+basemul+INTT+Compress+ByteEncode + Wait/SET(4) */
    int32_t tileLength;  /**< 单 poly 长度，固定 256 */
};

namespace tiling {

/** 单 poly 系数个数（Kyber 风格 ntt256；≠ Tag5T poly-batch）。 */
constexpr size_t n = 256;

/** workspace：4 片 int8 正向变换矩阵肢 + Split 缓冲 + AIC 输出。 */
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

/** ĝ：NTT 域右 poly，Host 预填；供 L2 basemul 读。 */
constexpr size_t G0 = A3 + n * 4 * sizeof(int32_t);

/** Minv：INTT 逆矩阵四肢（与 M4 同布局）；Host 预填 Minv4.bin。 */
constexpr size_t Minv0 = G0 + n * sizeof(int32_t);
constexpr size_t Minv1 = Minv0 + n * n;
constexpr size_t Minv2 = Minv1 + n * n;
constexpr size_t Minv3 = Minv2 + n * n;

/** P0：L1 CBD 的 PRF 输入 128B（η=2·N/4）；32B 对齐。 */
constexpr size_t P0 = Minv3 + n * n;
constexpr size_t kPrfBytes = 128;
constexpr size_t wssize = P0 + ((kPrfBytes + 31) / 32) * 32;

/** L2 中间态：Compress 域 int32[n]（1024B）；ByteEncode 前 out 须容纳此缓冲。 */
constexpr size_t kWorkBytes = n * sizeof(int32_t);

/** ByteEncode_d(d=4) 单 poly 编码输出 128B。 */
constexpr size_t kEncodeBytes = 128;

/** GM out 分配：须 ≥ kWorkBytes（L2 整链 int32 缓冲）；golden/dst 对拍前 128B。 */
constexpr size_t kOutBytes = kWorkBytes;
constexpr size_t kSrcBytes = n * sizeof(int32_t);
constexpr size_t kGBytes = n * sizeof(int32_t);

/** L1 SHAKE256 短向量输出字节数（写在 out 缓冲前缀；Host L1 Sync 后 D2H）。 */
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
