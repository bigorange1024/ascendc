/**
 * @file byte_decode_d_ref.h
 * @brief ByteDecode_d 的 C 参考实现（golden 生成用）头文件。
 *        本文件仅声明纯 C 接口，供 scripts/gen_data.py 通过 ctypes 加载
 *        libbytedecode_gen.so 调用：先用上游 byte_encode_d_ref.c 生成 encoded 比特流，
 *        再用本文件声明的 decode 函数还原系数并与随机 comp 做 round-trip 校验，
 *        产出 input/encoded.bin 与 output/golden_comp.bin。
 *        这些函数是「黑盒 oracle」：只保证 I/O 与 FIPS 203 Alg.6 语义一致，
 *        AscendC 侧（byte_decode_d_vec.hpp）不要求与其实现同构。
 */
#ifndef BYTE_DECODE_D_REF_H
#define BYTE_DECODE_D_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ByteDecode_d 参考实现（各 d 一个入口，函数名按 d 区分）。
 * @param out 输出系数数组，长度 = n，每个元素落在 [0, 2^d) 范围内
 * @param in  输入比特流缓冲区，长度 = n*d/8 字节（ByteEncode_d 的输出格式）
 * @param n   系数个数（本探针固定 n = F203_MLKEM_N = 256）
 */
void poly_byte_decode_d4_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d5_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d10_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d11_ref(int32_t *out, const uint8_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
