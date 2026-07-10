/**
 * @file f203_decrypt_layout.h
 * @brief FIPS 203 Alg.15 PKE Decrypt 尺寸常量（ml_kem_1024 / k=4）。
 *
 * 生产 I/O（Host 文件 / 设备指针语义）：
 *   输入  dk_PKE = ByteEncode₁₂(ŝ)     F203_DK_PKE_BYTES = 1536
 *         c = c₁‖c₂                   F203_CT_PKE_BYTES = 1568
 *   输出  m                           F203_MSG_BYTES = 32
 *
 * 密文布局：c₁ = k×352B（d=11），c₂ = 160B（d=5）。
 * 中间态（仅设备 GM，不落盘）：u'/v'/ŝ/û/ŵ/w 等，字节数见下方 *_BYTES。
 */
#ifndef F203_DECRYPT_LAYOUT_H
#define F203_DECRYPT_LAYOUT_H
#include <stdint.h>

#define F203_DECRYPT_K 4
#define F203_DECRYPT_N 256
#define F203_DECRYPT_Q 3329

#define F203_DK_PKE_BYTES 1536U /* k × 384 = ByteEncode₁₂(ŝ) */
#define F203_CT_PKE_BYTES 1568U /* 1408 + 160 */
#define F203_MSG_BYTES 32U

#define F203_C1_POLY_BYTES 352U /* 256×11/8 */
#define F203_C1_BYTES (F203_DECRYPT_K * F203_C1_POLY_BYTES)
#define F203_C2_BYTES 160U /* 256×5/8 */

/* 中间态（仅设备 GM）：u'/ŝ/û 为 k×n int32；v'/ŵ/w 为 n int32 */
#define F203_U_POLYVEC_BYTES (F203_DECRYPT_K * F203_DECRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_V_POLY_BYTES (F203_DECRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_S_HAT_BYTES F203_U_POLYVEC_BYTES
#define F203_U_HAT_BYTES F203_U_POLYVEC_BYTES
#define F203_W_HAT_BYTES F203_V_POLY_BYTES
#define F203_W_POLY_BYTES F203_V_POLY_BYTES

#endif
