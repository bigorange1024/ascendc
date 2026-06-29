#ifndef HAT_INNER_PRODUCT_REF_H
#define HAT_INNER_PRODUCT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 与 mod_config.hpp F203_MOD_VARIANT 编号一致。 */
enum HatModVariant {
    HAT_MOD_SCALAR_I64 = 0,
    HAT_MOD_BARRETT = 1,
    HAT_MOD_CAST_DIV = 2,
};

enum { HAT_K = 4, HAT_N = 256, HAT_KK = 16, HAT_Q = 3329 };

/** Alg.11 MultiplyNTTs：单对多项式 ∘（内部 Barrett 约简）。 */
void hat_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g);

/**
 * Alg.13 行 18：tHat[p] = mod( sum_j MultiplyNTTs(A[p,j], s[j]) + e[p] )。
 * a_hat [16,N], s_hat/e_hat [4,N], t_hat [4,N]。
 */
void hat_inner_product_add(const int32_t *a_hat, const int32_t *s_hat, const int32_t *e_hat, int32_t *t_hat,
                           int mod_variant);

#ifdef __cplusplus
}
#endif

#endif
