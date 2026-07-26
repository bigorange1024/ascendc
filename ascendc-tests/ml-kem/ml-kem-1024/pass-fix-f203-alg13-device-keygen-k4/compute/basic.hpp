// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/basic.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `basic.hpp` 为该子模块组件。 / Component: basic.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file basic.hpp
 * @brief KeyGen compute 公共前置：引入 kernel_operator 并导出 LocalTensor 别名。
 *
 * ## 流水线位置
 * 被 aic_func / aiv_func / mmad_custom 等 Launch 2 头文件包含；无算法逻辑。
 *
 * ## 对齐
 * FIPS 203 Alg.13 / ML-KEM-1024（k=4）compute 段公共依赖；与 golden 无直接 I/O。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
