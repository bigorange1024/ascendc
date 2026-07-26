/**
 * @file byte_encode12_rom_tables.h
 * @brief ByteEncode₁₂ 向量路径 Gather 字节索引 ROM 的 __gm__ 声明。
 *
 * 流水线位置：行 19–20 BYTE_ENCODE12_PREFETCH=1 时 Init DataCopy→UB。
 * 作用：偶/奇系数字节偏移表（与 Alg.11 even/odd 公式一致：8i / 8i+4）。
 * 与 golden 关系：索引辅助编码布局；位级对拍仍以 byte_encode12_ref.c 为准。
 */
#ifndef BYTE_ENCODE12_ROM_TABLES_H
#define BYTE_ENCODE12_ROM_TABLES_H

#include "kernel_operator.h"
#include <stdint.h>

#define BYTE_ENCODE12_PAIR_COUNT 128

extern __gm__ const int32_t gByteEncode12GatherEvenByteGm[BYTE_ENCODE12_PAIR_COUNT];
extern __gm__ const int32_t gByteEncode12GatherOddByteGm[BYTE_ENCODE12_PAIR_COUNT];

#endif
