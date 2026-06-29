// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/alg11_rom_tables.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_rom_tables.h` 为该子模块组件。 / Component: alg11_rom_tables.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_gammas.h, kernel_operator.h, stdint.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file alg11_rom_tables.h
 * @brief 设备侧 Alg.11 常量 ROM 的 __gm__ 声明（γ、Gather 字节索引、interleave 重排表）。
 *
 * 定义见 alg11_rom_tables.cpp；Init 阶段 DataCopy 进 UB，Compute 热路径禁止 SetValue 重填。
 *
 * 同步：alg11_gammas.h、hat_gammas.hpp、hat_inner_product_ref.c 中 kGammas 须一致。
 */
#ifndef ALG11_ROM_TABLES_H
#define ALG11_ROM_TABLES_H

#include "alg11_gammas.h"
#include "kernel_operator.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel_operator.h"

extern __gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2];

#ifdef __cplusplus
}
#endif

#endif
