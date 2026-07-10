/**
 * @file f203_decrypt_intt_w_tiling.h
 * @brief Alg.15 INTT(ŵ) 段 tiling：与 NTT(u) 共用同一套常量/偏移。
 *
 * 流水线位置：intt_w 实现与 fused kernel 的 inttWs 布局。
 * 背景：ŵ pad 成 k=4 polyvec 后走与 û 同构的 Stage1/MMAD/Stage3，
 * 故直接复用 f203_decrypt_ntt_u_tiling.h，避免两套 LUT 偏移漂移。
 */
#ifndef F203_DECRYPT_INTT_W_TILING_H
#define F203_DECRYPT_INTT_W_TILING_H
#include "f203_decrypt_ntt_u_tiling.h"
#endif
