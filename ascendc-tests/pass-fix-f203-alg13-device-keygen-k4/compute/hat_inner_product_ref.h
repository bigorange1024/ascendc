// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/hat_inner_product_ref.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_inner_product_ref.h` 为该子模块组件。 / Component: hat_inner_product_ref.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: stdint.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 hat 点积（Â∘ŝ）与相关 UB/tiling。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/hat_inner_product_ref.h
 */
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
