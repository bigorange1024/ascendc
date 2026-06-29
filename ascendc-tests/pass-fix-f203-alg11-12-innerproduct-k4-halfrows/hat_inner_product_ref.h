#ifndef HAT_INNER_PRODUCT_REF_H
#define HAT_INNER_PRODUCT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum HatModVariant {
    HAT_MOD_SCALAR_I64 = 0,
    HAT_MOD_BARRETT = 1,
    HAT_MOD_CAST_DIV = 2,
};

#ifndef HAT_P_OUT
#define HAT_P_OUT 4
#endif
#ifndef HAT_S_VEC
#define HAT_S_VEC 4
#endif
#ifndef HAT_N
#define HAT_N 256
#endif
#ifndef HAT_Q
#define HAT_Q 3329
#endif

void hat_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g);

/** t̂[p] = mod_q( Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) )，无 ê。 */
void hat_inner_product_dot(const int32_t *a_hat, const int32_t *s_hat, int32_t *t_hat, int mod_variant);

#ifdef __cplusplus
}
#endif

#endif
