#ifndef TOY_E07_SHAKE_NTT_BASEMUL_INTT_TILING_H
#define TOY_E07_SHAKE_NTT_BASEMUL_INTT_TILING_H

/**
 * @file tiling.h
 * @brief E07：E06 壳 + L1 真 SHAKE256 + L2 真 NTT + 真 basemul + 真 INTT + SET(4)。
 *
 * NTT/INTT golden / 设备语义 = merged_kyber ntt256 矩阵正/逆（≠ Tag5T）。
 * INTT = 同系 Split→AIC Mmad(Minv)→Merge+Barrett；Minv = M^{-1} (mod q)。
 * basemul = FIPS Alg.11/12 标量（γ=kMlkemGammas）；f̂=NTT(src)，ĝ=Host 玩具系数。
 * SHAKE 短向量 = hashlib.shake_256(b"abc").digest(32)。
 *
 * workspace：ntt256 布局 + G0=ĝ + Minv4（逆变换矩阵四肢）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 真 SHAKE256；1=L2 真 NTT+basemul+INTT + Wait/SET(4) */
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
constexpr size_t wssize = Minv3 + n * n;

/** 输出：INTT(MultiplyNTTs(NTT(src), ĝ)) int32[n]。 */
constexpr size_t kOutBytes = n * sizeof(int32_t);
constexpr size_t kSrcBytes = n * sizeof(int32_t);
constexpr size_t kGBytes = n * sizeof(int32_t);

/** L1 SHAKE256 短向量输出字节数（写在 out 缓冲前缀；Host L1 Sync 后 D2H）。 */
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
