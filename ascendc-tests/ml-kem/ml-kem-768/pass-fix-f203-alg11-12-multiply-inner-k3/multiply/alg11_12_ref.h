/**
 * 【文件头】FIPS 203 Alg.11 / Alg.12 共享内联参考实现。
 *
 * 本文件在流水线中的位置：host golden（ctypes / gen_data）与设备 UB 标量桥的算法真源。
 * 作用：Barrett 约化、Alg.12 BaseCaseMultiply、Alg.11 MultiplyNTTs 的逐对循环。
 * 与 golden 关系：gen_data.py 的 Python 路径与本头内联逻辑对拍；C 导出见 alg11_12_ref.c。
 * 设备侧标量镜像在 multiply_ntts_ub.hpp，须与本文件保持同步。
 */
#ifndef ALG11_12_REF_H
#define ALG11_12_REF_H

#include <stdint.h>
#include "alg11_gammas.h"

#define ALG11_N 256
#define ALG11_Q 3329

/**
 * Barrett 模约化到 [0, q)。
 * @param x  任意 int32 中间积/和（可负）
 * @return   x mod q，canonical 于 [0, 3329)
 * 步骤：负值抬升 → μ=78/k=18 → μ=5039/k=24 → wrap_mod 末步。
 */
static inline int32_t alg11_barrett_red_coeff(int32_t x)
{
    const int32_t q = ALG11_Q;
    /* 若 x<0，加上 q（算术右移取符号位作掩码） */
    int32_t t = x + (q & (x >> 31));
    /* 第一轮 Barrett：t1 ≈ t * 78 / 2^18 */
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    /* 第二轮 Barrett：t2 ≈ x * 5039 / 2^24 */
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    /* wrap：若 x≥q 则减 q（与向量 wrap_mod_vec_runtime 末步一致） */
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * FIPS 203 Alg.12 BaseCaseMultiply：一对 (a0,a1)×(b0,b1) 在 γ 下的基域乘。
 * @param c0,c1  输出偶/奇系数（已约化）
 * @param a0,a1  左多项式第 i 对
 * @param b0,b1  右多项式第 i 对
 * @param gamma  kMlkemGammas[i]
 * 公式：c0 = a0*b0 + a1*b1*γ；c1 = a0*b1 + a1*b0（均 mod q）
 */
static inline void alg12_base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0,
                                           int32_t b1, int32_t gamma)
{
    int32_t a1b1 = alg11_barrett_red_coeff(a1 * b1);
    *c0 = alg11_barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = alg11_barrett_red_coeff(a0 * b1 + a1 * b0);
}

/**
 * FIPS 203 Alg.11 MultiplyNTTs：对 AoS 交错多项式逐对调用 Alg.12。
 * @param h  输出 [256] int32，布局 h[2i]=c0, h[2i+1]=c1
 * @param f  输入左 poly [256] int32（NTT 域系数）
 * @param g  输入右 poly [256] int32
 * 前置：f/g/h 至少 ALG11_N 个 int32；γ 取自 kAlg11Gammas。
 */
static inline void alg11_multiply_ntts_inline(int32_t *h, const int32_t *f, const int32_t *g)
{
    /* i：第 i 对基域乘法；系数下标 2i / 2i+1 */
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

/** 非内联导出：供 gen_data.py ctypes 链接（实现见 alg11_12_ref.c） */
void alg11_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g);

#ifdef __cplusplus
}
#endif

#endif
