/**
 * @file f203_encrypt_tail_layout.h
 * @brief Alg.14 行 20/22–24 tail I/O 尺寸与布局常量（ml_kem_1024 / k=4）。
 *
 * 流水线位置：pack 探针与 compute-tail / device 全链的 u/v/c GM 布局对齐。
 * golden I/O：m 32B；u = K×N int32；v = N int32；mu_embed = N int32；
 * c = c₁(K×352B) ‖ c₂(160B) = 1568B。
 *
 * 魔数来源：FIPS 203 ML-KEM-1024（du=11,dv=5）→ ByteEncode₁₁ 每 poly 352B、ByteEncode₅ 160B。
 */
#ifndef F203_ENCRYPT_TAIL_LAYOUT_H
#define F203_ENCRYPT_TAIL_LAYOUT_H

#include <stdint.h>

#define F203_TAIL_K 4
#define F203_TAIL_N 256
#define F203_TAIL_Q 3329
/** ⌊(q+1)/2⌋：消息比特 1 时的 Decompress₁ 嵌入值 */
#define F203_TAIL_HALF_Q ((F203_TAIL_Q + 1) / 2)

#define F203_TAIL_MSG_BYTES 32U
#define F203_TAIL_U_BYTES (F203_TAIL_K * F203_TAIL_N * (uint32_t)sizeof(int32_t))
#define F203_TAIL_V_BYTES (F203_TAIL_N * (uint32_t)sizeof(int32_t))
#define F203_TAIL_MU_BYTES (F203_TAIL_N * (uint32_t)sizeof(int32_t))

/** c₁ 每 poly 352B = ByteEncode₁₁(Compress₁₁(u[p])) */
#define F203_TAIL_C1_POLY_BYTES 352U
#define F203_TAIL_C1_BYTES (F203_TAIL_K * F203_TAIL_C1_POLY_BYTES)
/** c₂ 160B = ByteEncode₅(Compress₅(v)) */
#define F203_TAIL_C2_BYTES 160U
#define F203_TAIL_C_BYTES (F203_TAIL_C1_BYTES + F203_TAIL_C2_BYTES)

#endif
