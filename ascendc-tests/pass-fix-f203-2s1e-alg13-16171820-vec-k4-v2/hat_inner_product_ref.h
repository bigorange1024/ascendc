/**
 * @file hat_inner_product_ref.h
 * @brief Host/C golden：Alg.11 MultiplyNTTs + 行 18 内积（dot / dot+ê）与 mod 变体 API。
 *
 * 用途：gen_data.py 编译为 libhat_inner_product_ref.so，生成 golden_t_hat_c.bin、golden_t_hat_dot.bin。
 *
 * 调用方：scripts/gen_data.py（ctypes）；设备实现不链接此库。
 *
 * 不变量：HAT_K=4、HAT_N=256、HAT_Q=3329；HatModVariant 编号与 mod_config.hpp F203_MOD_VARIANT 对齐。
 *
 * Golden：即本库输出；verify_result.py 对 output/t_hat.bin；生产 gen 固定 mod_variant=HAT_MOD_SCALAR_I64。
 *
 * CMake：无（gen_data 用 gcc -shared 现场编译 hat_inner_product_ref.c）。
 */
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

/** t̂[p] = mod_q( Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) )，无 ê。 */
void hat_inner_product_dot(const int32_t *a_hat, const int32_t *s_hat, int32_t *t_hat, int mod_variant);

/**
 * Alg.13 行 18 完整：tHat[p] = mod( sum_j MultiplyNTTs(A[p,j], s[j]) + e[p] )。
 */
void hat_inner_product_add(const int32_t *a_hat, const int32_t *s_hat, const int32_t *e_hat, int32_t *t_hat,
                           int mod_variant);

#ifdef __cplusplus
}
#endif

#endif
