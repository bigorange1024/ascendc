/**
 * @file alg11_rom_tables.h
 * @brief 设备侧 Alg.11 常量 ROM 的 __gm__ 声明（γ、Gather 字节索引、interleave 重排表）。
 *
 * 流水线位置：行 18 向量 basemul Init；定义见 alg11_rom_tables.cpp。
 * 作用：声明 gAlg11GammasGm / GatherEven|Odd / InterleaveReorder，供 DataCopy→UB。
 * 与 golden 关系：γ 与 Host ref 一致；索引表为设备布局辅助，不单独对拍。
 * Compute 热路径禁止 SetValue 重填 ROM。
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
