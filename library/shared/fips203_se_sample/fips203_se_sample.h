/**
 * @file fips203_se_sample.h
 * @brief FIPS 203 Alg.13 行 8–15：Host 标量生成 src[8,256]（PRF 经 fips203_prf 抽象层）。
 *
 * G(d||k) 使用 SHA3-512（tiny_sha3）；PRF 默认 SHAKE128-shim（可经 FIPS203_PRF_BACKEND 切换）。
 */
#ifndef FIPS203_SE_SAMPLE_H
#define FIPS203_SE_SAMPLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIPS203_SE_K 4
#define FIPS203_SE_N 256
#define FIPS203_SE_Q 3329
#define FIPS203_SE_SRC_ROWS (2 * FIPS203_SE_K)
#define FIPS203_SE_SRC_COEFFS (FIPS203_SE_SRC_ROWS * FIPS203_SE_N)
#define FIPS203_SE_SEED_D_DEFAULT 20260619U

void fips203_derand_bytes_from_seed_d(uint32_t seed_d, uint8_t d[32]);
void fips203_hash_g_sigma(const uint8_t d[32], uint8_t sigma[32]);
void fips203_sample_poly_cbd2_row(int32_t *dst_row, const uint8_t *buf);

/**
 * FIPS 203 Alg.8 SamplePolyCBD_η=3：buf 须 ≥192 B；dst_row[256] 系数 ∈[0,q)。
 * 位抽取对齐 liboqs `mlk_poly_cbd3` / pqcrystals `cbd3`；负差 +q（与 cbd2 行同风格）。
 */
void fips203_sample_poly_cbd3_row(int32_t *dst_row, const uint8_t *buf);

/** 生成 src[8,256]：行 0..3 = s，4..7 = e；内部调用 fips203_prf */
int fips203_build_src(int32_t *src, uint32_t seed_d);

/** @deprecated 别名 */
int fips203_build_src_shake128_shim(int32_t *src, uint32_t seed_d);
int fips203_build_src_shake256(int32_t *src, uint32_t seed_d);

#ifdef __cplusplus
}
#endif

#endif
