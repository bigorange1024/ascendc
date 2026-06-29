/**
 * FIPS 203 Alg.11 / Alg.12 — shared inline reference (host golden + device UB bridge).
 */
#ifndef ALG11_12_REF_H
#define ALG11_12_REF_H

#include <stdint.h>
#include "alg11_gammas.h"

#define ALG11_N 256
#define ALG11_Q 3329

static inline int32_t alg11_barrett_red_coeff(int32_t x)
{
    const int32_t q = ALG11_Q;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

static inline void alg12_base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0,
                                           int32_t b1, int32_t gamma)
{
    int32_t a1b1 = alg11_barrett_red_coeff(a1 * b1);
    *c0 = alg11_barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = alg11_barrett_red_coeff(a0 * b1 + a1 * b0);
}

static inline void alg11_multiply_ntts_inline(int32_t *h, const int32_t *f, const int32_t *g)
{
    for (int i = 0; i < ALG11_N / 2; ++i) {
        int32_t a0 = f[i * 2];
        int32_t a1 = f[i * 2 + 1];
        int32_t b0 = g[i * 2];
        int32_t b1 = g[i * 2 + 1];
        int32_t c0 = 0;
        int32_t c1 = 0;
        alg12_base_case_multiply(&c0, &c1, a0, a1, b0, b1, kAlg11Gammas[i]);
        h[i * 2] = c0;
        h[i * 2 + 1] = c1;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

void alg11_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g);

#ifdef __cplusplus
}
#endif

#endif
