/**
 * @file f203_decrypt_intt_w_tiling.h
 * @brief Decrypt INTT(ŵ) tiling：与 NTT(u) 共用同一套 workspace / LUT 尺寸常量。
 *
 * INTT 与正向 NTT 几何相同（k=4 polyvec、mat_c 平面布局）；仅 LUT 内容为逆变换表。
 * 故直接包含 f203_decrypt_ntt_u_tiling.h，避免重复定义偏移。
 */
// Decrypt INTT(w) tiling 常量。
// 流水线：Alg.15 逆 NTT 段。
// 与 golden：布局一致即可。

#ifndef F203_DECRYPT_INTT_W_TILING_H
#define F203_DECRYPT_INTT_W_TILING_H
#include "f203_decrypt_ntt_u_tiling.h"
#endif
