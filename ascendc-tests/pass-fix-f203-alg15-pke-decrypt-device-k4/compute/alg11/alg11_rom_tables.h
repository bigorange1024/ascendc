/**
 * @file alg11_rom_tables.h
 * @brief Alg.11 设备侧 ROM 常量表声明（γ、Gather 字节索引、interleave 重排）。
 *
 * 流水线位置：Init 一次 DataCopy 进 UB；禁止 Compute 热路径 SetValue 填表。
 * 与 alg11_fixed_n256 / ALG11_GAMMAS_TABLE 公式一致。
 * 定义见 alg11_rom_tables.cpp。
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

/** FIPS γ[128]，NTT 域 basemul 用 */
extern __gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT];
/** Gather 偶/奇系数字节偏移（int32 SoA） */
extern __gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT];
/** interleave：scratch=[c0‖c1] → h 的字节重排索引（256 个） */
extern __gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2];

#ifdef __cplusplus
}
#endif

#endif
