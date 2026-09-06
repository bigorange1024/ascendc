#ifndef TOY_E05_SKEL_SHAKE_PLUS_NTT_TILING_H
#define TOY_E05_SKEL_SHAKE_PLUS_NTT_TILING_H

/**
 * @file tiling.h
 * @brief E05：E04 2-launch 骨架 + L1 真 SHAKE256 + L2 真单 poly NTT（ntt256）。
 *
 * 重要：本积木 NTT golden / 设备语义 = merged_kyber ntt256（ntt_sim_kyber），
 * **不等于** FIPS 203 Tag5T / MlkemNtt / RouteA 交付路径。
 * SHAKE 短向量 golden = hashlib.shake_256(b"abc").digest(32)。
 *
 * Host 用 TilingData.phase 选 L1/L2；tileLength 固定 256。
 * workspace 布局与 ntt256 一致（M0..A3 字节偏移）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 真 SHAKE256；1=L2 真 NTT + Wait/SET(4) */
    int32_t tileLength;  /**< 单 poly 长度，固定 256 */
};

namespace tiling {

/** 单 poly 系数个数（Kyber 风格 ntt256；≠ Tag5T poly-batch）。 */
constexpr size_t n = 256;

/** workspace：4 片 int8 变换矩阵肢 + Split 缓冲 + AIC 输出。 */
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
constexpr size_t wssize = A3 + n * 4 * sizeof(int32_t);

/** 输出：NTT 结果 int32[n]（对拍 golden；非 E03 magic）。 */
constexpr size_t kOutBytes = n * sizeof(int32_t);
constexpr size_t kSrcBytes = n * sizeof(int32_t);

/** L1 SHAKE256 短向量输出字节数（写在 out 缓冲前缀；Host L1 Sync 后 D2H）。 */
constexpr size_t kShakeYBytes = 32;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
