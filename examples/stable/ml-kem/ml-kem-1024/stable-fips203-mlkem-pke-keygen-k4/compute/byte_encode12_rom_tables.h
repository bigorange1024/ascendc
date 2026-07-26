
/** 声明 ByteEncode₁₂ Gather 偶/奇字节偏移 ROM（定义见 .cpp）；pair 数=128。 */
// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/byte_encode12_rom_tables.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `byte_encode12_rom_tables.h` 为该子模块组件。 / Component: byte_encode12_rom_tables.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h, stdint.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 19–20 ByteEncode₁₂：将 t̂/ŝ 编成 ek/dk polyvec。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/byte_encode12_rom_tables.h
 */
#ifndef BYTE_ENCODE12_ROM_TABLES_H
#define BYTE_ENCODE12_ROM_TABLES_H

#include "kernel_operator.h"
#include <stdint.h>

#define BYTE_ENCODE12_PAIR_COUNT 128

extern __gm__ const int32_t gByteEncode12GatherEvenByteGm[BYTE_ENCODE12_PAIR_COUNT];
extern __gm__ const int32_t gByteEncode12GatherOddByteGm[BYTE_ENCODE12_PAIR_COUNT];

#endif
