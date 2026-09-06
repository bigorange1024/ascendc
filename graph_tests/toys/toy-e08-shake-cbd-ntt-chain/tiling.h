#ifndef TOY_E08_SHAKE_CBD_NTT_CHAIN_TILING_H
#define TOY_E08_SHAKE_CBD_NTT_CHAIN_TILING_H

/**
 * @file tiling.h
 * @brief E08：E07 壳 + L1 真 SHAKE256 + 真 CBD(η=2) + L2 真 NTT+basemul+INTT+SET(4)。
 *
 * L1：SHAKE 短向量写 out 前缀；CBD 读 ws[P0] 的 128B PRF，写 src[256] 供 L2。
 * NTT/INTT = ntt256 矩阵正/逆（≠ Tag5T）；basemul = Alg.11/12；CBD = Alg.8 η=2。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 真 SHAKE+CBD；1=L2 真 NTT+basemul+INTT + Wait/SET(4) */
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

/** 输出：INTT(MultiplyNTTs(NTT(CBD(prf)), ĝ)) int32[n]。 */
constexpr size_t kOutBytes = n * sizeof(int32_t);
constexpr size_t kSrcBytes = n * sizeof(int32_t);
constexpr size_t kGBytes = n * sizeof(int32_t);

/** L1 SHAKE256 短向量输出字节数（写在 out 缓冲前缀；Host L1 Sync 后 D2H）。 */
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
