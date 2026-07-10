/**
 * @file alg11_rom_tables.h
 * @brief Alg.11 设备侧 ROM 常量表声明（Decrypt su_dot Init 一次 DataCopy 进 UB）。
 *
 * 含：γ[128]、Gather 偶/奇字节索引[128]、interleave 重排字节索引[256]。
 * 禁止在 Compute 热路径 SetValue 重填；定义见 alg11_rom_tables.cpp。
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

/** FIPS kMlkemGammas，设备 GM 镜像。 */
extern __gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT];
/** Gather 偶半字字节偏移：i*8。 */
extern __gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT];
/** Gather 奇半字字节偏移：i*8+4。 */
extern __gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT];
/** interleave：scratch=[c0||c1] → h 的字节重排索引（256 个）。 */
extern __gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2];

#ifdef __cplusplus
}
#endif

#endif
