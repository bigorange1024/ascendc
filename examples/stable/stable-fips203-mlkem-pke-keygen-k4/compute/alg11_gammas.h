// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/alg11_gammas.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_gammas.h` 为该子模块组件。 / Component: alg11_gammas.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: stdint.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file alg11_gammas.h
 * @brief FIPS 203 kMlkemGammas[128] 编译期表：γ[i]=ζ^(2·BitRev7(i)+1) mod 3329。
 *
 * 宏 ALG11_GAMMAS_TABLE / kAlg11Gammas 供标量路径与 ROM 生成；与 hat_gammas.hpp、hat_inner_product_ref.c 同步。
 */
#ifndef ALG11_GAMMAS_H
#define ALG11_GAMMAS_H

#include <stdint.h>

#define ALG11_PAIR_COUNT 128

/** 编译期 ROM：FIPS kMlkemGammas，设备侧禁止在 Compute 热路径重填。 */
#define ALG11_GAMMAS_TABLE                                                          \
    17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, \
        1100, 2229, 1409, 1920, 2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, \
        3015, 314, 3050, 279, 1703, 1626, 1651, 1678, 2789, 540, 1789, 1540, 1847, 1482, \
        952, 2377, 1461, 1868, 2687, 642, 939, 2390, 2308, 1021, 2437, 892, 2388, 941, \
        733, 2596, 2337, 992, 268, 3061, 641, 2688, 1584, 1745, 2298, 1031, 2037, 1292, \
        3220, 109, 375, 2954, 2549, 780, 2090, 1239, 1645, 1684, 1063, 2266, 319, 3010, \
        2773, 556, 757, 2572, 2099, 1230, 561, 2768, 2466, 863, 2594, 735, 2804, 525, 1092, \
        2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179, 2775, 554, 886, 2443, 1722, 1607, \
        1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444, 2154, 1175

static const int32_t kAlg11Gammas[ALG11_PAIR_COUNT] = {ALG11_GAMMAS_TABLE};

#endif
