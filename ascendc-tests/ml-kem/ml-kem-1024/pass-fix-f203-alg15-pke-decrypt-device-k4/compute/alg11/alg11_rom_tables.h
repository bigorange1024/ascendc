/**
 * 【文件头】设备侧 __gm__ ROM 常量表声明（γ + Gather/interleave 字节索引）。
 *
 * 本文件在流水线中的位置：向量路径 Init 阶段 DataCopy 的源地址声明。
 * 作用：声明 gAlg11GammasGm / Gather 偶奇索引 / interleave 重排索引的 GM 符号。
 * 与 golden 关系：表内容须与 host γ 及固定 n=256 索引公式一致；不直接参与 golden 数值。
 * 定义见 alg11_rom_tables.cpp（由 kernel 在非 CPU_DEBUG 下 include）。
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

/** FIPS γ，长度 128，Init 一次拷入 UB */
extern __gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT];
/** Gather 偶下标字节偏移：i → i*8（int32 元素间距 4B，偶位置） */
extern __gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT];
/** Gather 奇下标字节偏移：i → i*8+4 */
extern __gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT];
/** interleave：从 scratch=[c0||c1] 取回 AoS 的字节索引，长度 256 */
extern __gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2];

#ifdef __cplusplus
}
#endif

#endif
