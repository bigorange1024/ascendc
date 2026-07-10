/**
 * @file hat_inner_product_ref.h
 * @brief Alg.13 行 18 纯 C 参考接口（golden / ctypes 加载）。
 *
 * 提供 MultiplyNTTs 与 polyvec 内积（无 ê）；形状由 HAT_P_OUT / HAT_S_VEC / HAT_N / HAT_Q 宏锁定。
 */
#ifndef HAT_INNER_PRODUCT_REF_H
#define HAT_INNER_PRODUCT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** final mod 变体：与设备 Barrett / 标量对照用。 */
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

/**
 * FIPS 203 Alg.11 MultiplyNTTs：h = f ⊛ g（NTT 域逐对乘，含 γ）。
 * @param h 输出 [HAT_N]
 * @param f 左多项式 [HAT_N]
 * @param g 右多项式 [HAT_N]
 */
void hat_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g);

/**
 * t̂[p] = mod_q( Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) )，无 ê。
 * a_hat 行主序 (p*S_VEC+j)*N；s_hat[j*N]；t_hat[p*N]。
 * @param a_hat Â 扁平数组
 * @param s_hat ŝ 扁平数组
 * @param t_hat 输出 t̂
 * @param mod_variant HatModVariant，gen_data 默认 0（SCALAR_I64）
 */
void hat_inner_product_dot(const int32_t *a_hat, const int32_t *s_hat, int32_t *t_hat, int mod_variant);

#ifdef __cplusplus
}
#endif

#endif
