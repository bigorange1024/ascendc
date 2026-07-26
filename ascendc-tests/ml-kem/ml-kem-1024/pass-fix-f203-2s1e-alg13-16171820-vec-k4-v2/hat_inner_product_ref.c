/**
 * @file hat_inner_product_ref.c
 * @brief Alg.13 行 18 参考实现（纯 C）：MultiplyNTTs + lazy int64 累加 + final mod q。
 *
 * 语义对齐 ntt_study ref_inner_product_add / MlkemMultiplyNttsDot；basemul 内 barrett_red_coeff，
 * 行级 mod 由 final_mod_i64 按 mod_variant 选择（golden 固定 HAT_MOD_SCALAR_I64）。
 *
 * 布局：a_hat 行主序 [p,j,c]=(p*K+j)*N+c；s_hat [j,c]；e_hat [p,c]；t_hat [p,c]。
 *
 * 导出：hat_multiply_ntts、hat_inner_product_dot、hat_inner_product_add。
 *
 * Golden：gen_data.py → golden_t_hat_*.bin；与设备 F203_MOD_VARIANT 解耦（设备 Barrett 仍须 I/O 一致）。
 *
 * 同步：kGammas[] 与 alg11_gammas.h / hat_gammas.hpp 须逐元一致。
 */
#include "hat_inner_product_ref.h"

#include <string.h>

static const int32_t kGammas[HAT_N / 2] = {
    17,   3312, 2761, 568,  583,  2746, 2649, 680,  1637, 1692, 723,  2606, 2288, 1041, 1100, 2229, 1409, 1920,
    2662, 667,  3281, 48,   233,  3096, 756,  2573, 2156, 1173, 3015, 314,  3050, 279,  1703, 1626, 1651, 1678,
    2789, 540,  1789, 1540, 1847, 1482, 952,  2377, 1461, 1868, 2687, 642,  939,  2390, 2308, 1021, 2437, 892,
    2388, 941,  733,  2596, 2337, 992,  268,  3061, 641,  2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
    375,  2954, 2549, 780,  2090, 1239, 1645, 1684, 1063, 2266, 319,  3010, 2773, 556,  757,  2572, 2099, 1230,
    561,  2768, 2466, 863,  2594, 735,  2804, 525,  1092, 2237, 403,  2926, 1026, 2303, 1143, 2186, 2150, 1179,
    2775, 554,  886,  2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394,  885,  2444,
    2154, 1175,
};

/** basemul 内 Barrett 约化（与设备 hat_reduce_zq_scalar / alg11 barrett_red_coeff 同公式）。 */
static int32_t barrett_red_coeff(int32_t x)
{
    const int32_t q = HAT_Q;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/** 行级 mod 变体 Barrett：非负余数路径（HAT_MOD_BARRETT）。 */
static int32_t mod_q_nonneg_i64(int64_t x)
{
    const int64_t q = HAT_Q;
    int64_t rem;
    if (x < 0) {
        rem = x % q;
        if (rem < 0) {
            rem += q;
        }
        return (int32_t)rem;
    }
    rem = x - (x / q) * q;
    if (rem >= q) {
        rem -= q;
    }
    return (int32_t)rem;
}

/** 行级 mod 变体：向零截断商（HAT_MOD_CAST_DIV，对齐设备 Cast+Div）。 */
static int32_t mod_q_cast_div_i64(int64_t x)
{
    const int64_t q = HAT_Q;
    int64_t t = (x >= 0) ? (x / q) : -((-x) / q);
    int64_t rem = x - q * t;
    if (rem >= q) {
        rem -= q;
    }
    if (rem < 0) {
        rem += q;
    }
    return (int32_t)rem;
}

/** 行级 mod 变体：floor 除法（HAT_MOD_SCALAR_I64，golden 默认）。 */
static int32_t mod_q_scalar_i64(int64_t x)
{
    const int64_t q = HAT_Q;
    const int64_t t = (x >= 0) ? (x / q) : (-((-x) / q));
    int64_t rem = x - q * t;
    if (rem >= q) {
        rem -= q;
    }
    if (rem < 0) {
        rem += q;
    }
    return (int32_t)rem;
}

/** 按 mod_variant 分发行级 final mod；golden 固定 HAT_MOD_SCALAR_I64。 */
static int32_t final_mod_i64(int64_t x, int mod_variant)
{
    if (mod_variant == HAT_MOD_CAST_DIV) {
        return mod_q_cast_div_i64(x);
    }
    if (mod_variant == HAT_MOD_SCALAR_I64) {
        return mod_q_scalar_i64(x);
    }
    return mod_q_nonneg_i64(x);
}

/**
 * Alg.11 MultiplyNTTs：交错 f/g[HAT_N] → h[HAT_N]。
 * 内部解交错→BaseCaseMultiply(γ)→再交错；约化用 barrett_red_coeff。
 */
void hat_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g)
{
    int32_t a0[HAT_N / 2];
    int32_t a1[HAT_N / 2];
    int32_t b0[HAT_N / 2];
    int32_t b1[HAT_N / 2];
    int32_t c0[HAT_N / 2];
    int32_t c1[HAT_N / 2];

    for (int i = 0; i < HAT_N / 2; ++i) {
        a0[i] = f[i * 2];
        a1[i] = f[i * 2 + 1];
        b0[i] = g[i * 2];
        b1[i] = g[i * 2 + 1];
    }

    for (int i = 0; i < HAT_N / 2; ++i) {
        int32_t a1b1 = barrett_red_coeff(a1[i] * b1[i]);
        int32_t t0 = barrett_red_coeff(a0[i] * b0[i] + a1b1 * kGammas[i]);
        int32_t t1 = barrett_red_coeff(a0[i] * b1[i] + a1[i] * b0[i]);
        c0[i] = t0;
        c1[i] = t1;
    }

    for (int i = 0; i < HAT_N / 2; ++i) {
        h[i * 2] = c0[i];
        h[i * 2 + 1] = c1[i];
    }
}

/**
 * 行 18 dot-only：t̂[p]=mod(Σ_j MultiplyNTTs(Â[p,j],ŝ[j]))，无 ê。
 * @param a_hat [K*K*N]；@param s_hat [K*N]；@param t_hat [K*N]；@param mod_variant HatModVariant
 */
void hat_inner_product_dot(const int32_t *a_hat, const int32_t *s_hat, int32_t *t_hat, int mod_variant)
{
    int32_t prod[HAT_N];
    int64_t acc[HAT_N];

    for (int p = 0; p < HAT_K; ++p) {
        memset(acc, 0, sizeof(acc));
        for (int j = 0; j < HAT_K; ++j) {
            const int32_t *a_row = a_hat + ((p * HAT_K) + j) * HAT_N;
            const int32_t *s_row = s_hat + j * HAT_N;
            hat_multiply_ntts(prod, a_row, s_row);
            for (int c = 0; c < HAT_N; ++c) {
                acc[c] += (int64_t)prod[c];
            }
        }
        for (int c = 0; c < HAT_N; ++c) {
            t_hat[p * HAT_N + c] = final_mod_i64(acc[c], mod_variant);
        }
    }
}

/**
 * 行 18 完整：t̂[p]=mod(Σ_j MultiplyNTTs(Â[p,j],ŝ[j]) + ê[p])。
 * lazy int64 累加，最后一次 final_mod_i64。
 */
void hat_inner_product_add(const int32_t *a_hat, const int32_t *s_hat, const int32_t *e_hat, int32_t *t_hat,
                           int mod_variant)
{
    int32_t prod[HAT_N];
    int64_t acc[HAT_N];

    for (int p = 0; p < HAT_K; ++p) {
        memset(acc, 0, sizeof(acc));
        for (int j = 0; j < HAT_K; ++j) {
            const int32_t *a_row = a_hat + ((p * HAT_K) + j) * HAT_N;
            const int32_t *s_row = s_hat + j * HAT_N;
            hat_multiply_ntts(prod, a_row, s_row);
            for (int c = 0; c < HAT_N; ++c) {
                acc[c] += (int64_t)prod[c];
            }
        }
        for (int c = 0; c < HAT_N; ++c) {
            int64_t summed = acc[c] + (int64_t)e_hat[p * HAT_N + c];
            t_hat[p * HAT_N + c] = final_mod_i64(summed, mod_variant);
        }
    }
}
