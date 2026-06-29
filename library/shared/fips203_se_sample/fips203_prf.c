/**
 * @file fips203_prf.c
 * @brief PRF 后端选择：默认标量 SHAKE128-shim；FIPS203_PRF_BACKEND=shake256 可切换。
 */
#include "fips203_prf.h"

#include <stdlib.h>
#include <string.h>

#include "sha3.h"

static void prf_xof(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce, int use_shake256)
{
    sha3_ctx_t ctx;
    uint8_t in[33];
    memcpy(in, sigma, 32);
    in[32] = nonce;
    if (use_shake256) {
        shake256_init(&ctx);
    } else {
        shake128_init(&ctx);
    }
    shake_update(&ctx, in, sizeof(in));
    shake_xof(&ctx);
    shake_out(&ctx, out, outlen);
}

void fips203_prf_shake128_scalar(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce)
{
    prf_xof(out, outlen, sigma, nonce, 0);
}

void fips203_prf_shake256_scalar(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce)
{
    prf_xof(out, outlen, sigma, nonce, 1);
}

void fips203_prf(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce)
{
    const char *env = getenv("FIPS203_PRF_BACKEND");
    if (env != NULL && (strcmp(env, "shake256") == 0 || strcmp(env, "SHAKE256") == 0)) {
        fips203_prf_shake256_scalar(out, outlen, sigma, nonce);
    } else {
        fips203_prf_shake128_scalar(out, outlen, sigma, nonce);
    }
}
