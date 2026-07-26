/**
 * @file byte_encode_d_ref.h
 * @brief ByteEncode_d 的 C 参考实现（golden 生成用）头文件。
 *        本文件仅声明纯 C 接口，供 scripts/gen_data.py 通过 ctypes 加载
 *        libbyte_encode_d_ref.so 调用，产出 output/golden_encoded.bin。
 *        这些函数是「黑盒 oracle」：只保证 I/O 与 FIPS 203 Alg.5 比特流一致，
 *        AscendC 侧（byte_encode_d_vec.hpp）不要求与其实现同构。
 */
#ifndef BYTE_ENCODE_D_REF_H
#define BYTE_ENCODE_D_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ByteEncode_d 参考实现（各 d 一个入口，函数名按 d 区分）。
 * @param out 输出比特流缓冲区，长度 = n*d/8 字节（由调用方按 d 预先分配）
 * @param in  输入系数数组，每个元素为已在 [0, 2^d) 范围内的压缩系数
 * @param n   系数个数（本探针固定 n = F203_MLKEM_N = 256）
 */
void poly_byte_encode_d4_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d5_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d10_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d11_ref(uint8_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
