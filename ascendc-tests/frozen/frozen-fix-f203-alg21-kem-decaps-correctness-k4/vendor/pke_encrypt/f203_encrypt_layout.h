/**
 * @file f203_encrypt_layout.h
 * @brief FIPS 203 Alg.14 PKE Encrypt I/O（ml_kem_1024 / k=4）。
 */
#ifndef F203_ENCRYPT_LAYOUT_H
#define F203_ENCRYPT_LAYOUT_H
#include <stdint.h>

#define F203_ENCRYPT_K 4
#define F203_ENCRYPT_N 256
#define F203_ENCRYPT_Q 3329

#define F203_EK_PKE_BYTES 1568U
#define F203_EK_T_BYTES 1536U
#define F203_EK_RHO_BYTES 32U
#define F203_EK_RHO_OFFSET F203_EK_T_BYTES

#define F203_MSG_BYTES 32U
#define F203_ENC_COINS_BYTES 32U
#define F203_CT_PKE_BYTES 1568U

/** G1 中间张量（device GM / output 对拍） */
#define F203_AHAT_POLYS (F203_ENCRYPT_K * F203_ENCRYPT_K)
#define F203_AHAT_BYTES (F203_AHAT_POLYS * F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_R_POLYVEC_BYTES (F203_ENCRYPT_K * F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_R_HAT_BYTES F203_R_POLYVEC_BYTES
#define F203_E1_POLYVEC_BYTES (F203_ENCRYPT_K * F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_E2_POLY_BYTES (F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_RE_TOTAL_BYTES (F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES + F203_E2_POLY_BYTES)

/** PRF staging：9×128B（ml_kem_1024 η₁=η₂=2） */
#define F203_ENCRYPT_PRF_BATCH 9U
#define F203_ENCRYPT_PRF_BYTES_PER_POLY 128U
#define F203_ENCRYPT_PRF_TOTAL_BYTES (F203_ENCRYPT_PRF_BATCH * F203_ENCRYPT_PRF_BYTES_PER_POLY)

#define F203_U_HAT_BYTES (F203_ENCRYPT_K * F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_TR_HAT_BYTES (F203_ENCRYPT_N * (uint32_t)sizeof(int32_t))
#define F203_T_HAT_BYTES F203_R_POLYVEC_BYTES
#define F203_EK_T_POLYVEC_BYTES F203_EK_T_BYTES

/** Gate 阶段：0=marker 壳，1=prep，2=+NTT r̂，3=+线性层，4=+INTT/噪声/pack→c */
/** Gate：0=marker，1=prep，2=+NTT r̂，3=+线性，4=+INTT/噪声/pack（staging t_hat），5=生产单 session */
#define F203_ENCRYPT_PHASE 5

#endif
