#ifndef TOY_E04_SKEL_PLUS_REAL_NTT_TILING_H
#define TOY_E04_SKEL_PLUS_REAL_NTT_TILING_H

/**
 * @file tiling.h
 * @brief E04：E03 2-launch 骨架 + L2 真单 poly NTT（自包含拷贝自 pass-merged-kyber-mix-ntt256）。
 *
 * 重要：本积木 golden / 设备语义 = merged_kyber ntt256（ntt_sim_kyber），
 * **不等于** FIPS 203 Tag5T / MlkemNtt / RouteA 交付路径。
 *
 * Host 用 TilingData.phase 选 L1/L2；tileLength 固定 256。
 * workspace 布局与 ntt256 一致（M0..A3 字节偏移）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2；tileLength=256。 */
struct TilingData {
    int32_t phase;       /**< 0=L1 采样 stub；1=L2 真 NTT + Wait/SET(4) */
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

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

} // namespace tiling

#endif
