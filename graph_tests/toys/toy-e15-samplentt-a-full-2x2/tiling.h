#ifndef TOY_E15_SAMPLENTT_A_FULL_2X2_TILING_H
#define TOY_E15_SAMPLENTT_A_FULL_2X2_TILING_H

/**
 * @file tiling.h
 * @brief E15：E14 壳 + 完整 2×2 Â 真 SampleNTT（(0,0)…(1,1)）→ G0…G3。
 *
 * L1（采样）：SHAKE；CBD×2→src；CBD×1→ws[E0]。
 * L2（代数+压码）：
 *   独立 launch SampleNTT(SEED_D,(j,i)) → ws[G0/G1/G2/G3]（完整 k×k=2×2）
 *   u0/u1 用 G0/G1；(1,0) 供 v 路 basemul（G2）；G3=(1,1) 采样落盘供完整性
 *   u/v 真链 + c=c1||c2(384B) + SET(4)
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / SampleNTT / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1；1=SampleNTT；2=L2 代数+压码 + SET(4) */
    int32_t tileLength;  /**< 单 poly 长度，固定 256 */
};

namespace tiling {

constexpr size_t k = 2;
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

/** G0…G3：完整 2×2 Â；(0,0)/(0,1)/(1,0)/(1,1)。 */
constexpr size_t G0 = A3 + n * 4 * sizeof(int32_t);
constexpr size_t G1 = G0 + n * sizeof(int32_t);
constexpr size_t G2 = G1 + n * sizeof(int32_t);
constexpr size_t G3 = G2 + n * sizeof(int32_t);

constexpr size_t Minv0 = G3 + n * sizeof(int32_t);
constexpr size_t Minv1 = Minv0 + n * n;
constexpr size_t Minv2 = Minv1 + n * n;
constexpr size_t Minv3 = Minv2 + n * n;

constexpr size_t W0 = Minv3 + n * n;
constexpr size_t W1 = W0 + n * sizeof(int32_t);

constexpr size_t E0 = W1 + n * sizeof(int32_t);
constexpr size_t W2 = E0 + n * sizeof(int32_t);

constexpr size_t P0 = W2 + n * sizeof(int32_t);
constexpr size_t kPrfBytesPerPoly = 128;
constexpr size_t kPrfPolys = k + 1;
constexpr size_t kPrfBytes = kPrfBytesPerPoly * kPrfPolys;

constexpr size_t MU0 = P0 + ((kPrfBytes + 31) / 32) * 32;
constexpr size_t kMuBytes = 32;

/** SD0：SEED_D uint32 LE，供 L2 SampleNTT 派生 ρ。 */
constexpr size_t SD0 = MU0 + ((kMuBytes + 31) / 32) * 32;
constexpr size_t kSeedDBytes = 4;

/** SNTT_FLAG：AIV subBlock1 轮询 GM=1 表示 subBlock0 SampleNTT 完成（与 CrossCore7 分工）。 */
constexpr size_t SNTT_FLAG = SD0 + ((kSeedDBytes + 31) / 32) * 32;

constexpr size_t wssize = SNTT_FLAG + 32;

constexpr size_t kWorkBytes = n * sizeof(int32_t);
constexpr size_t kEncodeBytesPerPoly = 128;
constexpr size_t kC1Bytes = kEncodeBytesPerPoly * k;
constexpr size_t kC2Bytes = kEncodeBytesPerPoly;
constexpr size_t kOutBytes = kC1Bytes + kC2Bytes;
constexpr size_t kSrcBytes = k * n * sizeof(int32_t);
/** 完整 k×k=4 poly â 区（设备 SampleNTT 写入；Host 不预载 stub）。 */
constexpr size_t kGBytes = k * k * n * sizeof(int32_t);
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseSampleNtt = 1;
constexpr int32_t kPhaseLaunch2 = 2;

} // namespace tiling

#endif
