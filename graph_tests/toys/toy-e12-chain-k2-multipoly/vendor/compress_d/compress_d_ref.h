/**
 * @file compress_d_ref.h
 * @brief compress_d_ref.c 的 C 接口声明——host 侧 golden 生成用的 Compress_d 参考实现。
 *
 * 本文件在流水线中的位置：被 scripts/gen_data.py 通过 ctypes 加载的 libcompress_d_ref.so
 * 使用（声明与之匹配的 extern "C" 符号），不参与 AscendC 设备端编译。
 * 规范对齐：FIPS 203 §4.2.1 Compress_d（Eq 4.7）标量参考实现，与 compress_d_vec.hpp 中
 * scalar_compress_u32 的向量/标量设备算法保持数学同构（但本文件仅作 golden，不是设备实现依据）。
 * 与 golden 的关系：本文件声明的 poly_compress_d*_ref 系列函数即 golden 计算内核本身，
 * gen_data.py 调用其输出写入 output/golden_comp.bin，供 verify_result.py 与设备输出对拍。
 */
#ifndef COMPRESS_D_REF_H
#define COMPRESS_D_REF_H

#include <stdint.h>

#include "f203_mlkem_params.h"

#ifdef __cplusplus
extern "C" {
#endif

// 单系数 Barrett/移位标量 Compress_d，u 为 canonical mod q 系数（[0, q-1]），返回值 ∈ [0, 2^d-1]。
uint32_t f203_scalar_compress_d4(uint32_t u);
uint32_t f203_scalar_compress_d5(uint32_t u);
uint32_t f203_scalar_compress_d10(uint32_t u);
uint32_t f203_scalar_compress_d11(uint32_t u);

// 整 poly 版本：out/in 均为长度 n 的 int32 数组（GM 语义在此处不适用，纯 host 内存指针）；
// in[i] 为 canonical mod q 系数，out[i] 为对应 d-bit 压缩域值。
void poly_compress_d4_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d5_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d10_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d11_ref(int32_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
