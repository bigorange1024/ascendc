#ifndef BYTE_ENCODE12_ROM_TABLES_H
#define BYTE_ENCODE12_ROM_TABLES_H

/**
 * @file byte_encode12_rom_tables.h
 * @brief ByteEncode₁₂ prefetch 路径：GM 常驻 Gather 字节索引表声明。
 *
 * 流水线位置：prefetch 向量编码（byte_encode12_vec.hpp）从 GM ROM 拷入 UB 作 Gather 索引。
 * 与 golden 关系：索引仅实现 even/odd 解交错，不改变 Alg.5 打包语义；输出仍对拍 golden。
 * 作用：声明 128 对 even/odd 字节偏移表（8i / 8i+4）。
 */

#include "kernel_operator.h"
#include <stdint.h>

/** 单 poly 系数对个数：256/2 = 128 */
#define BYTE_ENCODE12_PAIR_COUNT 128

/** even 系数 Gather 字节索引：8*i（int32 元素按字节寻址） */
extern __gm__ const int32_t gByteEncode12GatherEvenByteGm[BYTE_ENCODE12_PAIR_COUNT];
/** odd 系数 Gather 字节索引：8*i+4 */
extern __gm__ const int32_t gByteEncode12GatherOddByteGm[BYTE_ENCODE12_PAIR_COUNT];

#endif
