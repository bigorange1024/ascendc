/**
 * @file fips203_se_sample.c
 * @brief Alg.13 行 8–15 标量采样；Keccak 来自 thirdparty/tiny_sha3。
 */
#include "fips203_se_sample.h"

#include "fips203_prf.h"

#include <stdio.h>
#include <string.h>

#include "sha3.h"

#define ETA2 2
#define PRF_BYTES ((ETA2 * FIPS203_SE_N) / 4)

static uint32_t load32_le(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static uint32_t load24_le(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
}

void fips203_derand_bytes_from_seed_d(uint32_t seed_d, uint8_t d[32])
{
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "exp-mlkem-f203-2s1e-k4:SEED_D=%u", seed_d);
    sha3(msg, (size_t)n, d, 32);
}

void fips203_hash_g_sigma(const uint8_t d[32], uint8_t sigma[32])
{
    uint8_t in[33];
    uint8_t out[64];
    memcpy(in, d, 32);
    in[32] = (uint8_t)FIPS203_SE_K;
    sha3(in, sizeof(in), out, 64);
    memcpy(sigma, out + 32, 32);
}

void fips203_sample_poly_cbd2_row(int32_t *dst_row, const uint8_t *buf)
{
    for (int i = 0; i < FIPS203_SE_N / 8; ++i) {
        uint32_t t = load32_le(buf + 4U * (uint32_t)i);
        uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (int j = 0; j < 8; ++j) {
            int32_t a = (int32_t)((d >> (4 * j + 0)) & 0x3U);
            int32_t b = (int32_t)((d >> (4 * j + 2)) & 0x3U);
            int32_t c = a - b;
            while (c < 0) {
                c += FIPS203_SE_Q;
            }
            c %= FIPS203_SE_Q;
            dst_row[8 * i + j] = c;
        }
    }
}

void fips203_sample_poly_cbd3_row(int32_t *dst_row, const uint8_t *buf)
{
    /* 背景：ML-KEM-512 η1=3；结论：与 liboqs cbd3 同抽取，负差 +q 对齐仓内 cbd2 I/O 风格。 */
    for (int i = 0; i < FIPS203_SE_N / 4; ++i) {
        uint32_t t = load24_le(buf + 3U * (uint32_t)i);
        uint32_t d = (t & 0x00249249U) + ((t >> 1) & 0x00249249U) + ((t >> 2) & 0x00249249U);
        for (int j = 0; j < 4; ++j) {
            int32_t a = (int32_t)((d >> (6 * j + 0)) & 0x7U);
            int32_t b = (int32_t)((d >> (6 * j + 3)) & 0x7U);
            int32_t c = a - b;
            while (c < 0) {
                c += FIPS203_SE_Q;
            }
            c %= FIPS203_SE_Q;
            dst_row[4 * i + j] = c;
        }
    }
}

int fips203_build_src(int32_t *src, uint32_t seed_d)
{
    if (src == NULL) {
        return -1;
    }
    uint8_t d[32];
    uint8_t sigma[32];
    uint8_t prf_buf[PRF_BYTES];
    uint8_t nonce = 0;

    fips203_derand_bytes_from_seed_d(seed_d, d);
    fips203_hash_g_sigma(d, sigma);

    for (int i = 0; i < FIPS203_SE_K; ++i) {
        fips203_prf(prf_buf, PRF_BYTES, sigma, nonce++);
        fips203_sample_poly_cbd2_row(src + i * FIPS203_SE_N, prf_buf);
    }
    for (int i = 0; i < FIPS203_SE_K; ++i) {
        fips203_prf(prf_buf, PRF_BYTES, sigma, nonce++);
        fips203_sample_poly_cbd2_row(src + (FIPS203_SE_K + i) * FIPS203_SE_N, prf_buf);
    }
    return 0;
}

int fips203_build_src_shake128_shim(int32_t *src, uint32_t seed_d)
{
    return fips203_build_src(src, seed_d);
}

int fips203_build_src_shake256(int32_t *src, uint32_t seed_d)
{
    return fips203_build_src(src, seed_d);
}
