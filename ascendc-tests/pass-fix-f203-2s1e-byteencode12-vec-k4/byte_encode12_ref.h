#ifndef BYTE_ENCODE12_REF_H
#define BYTE_ENCODE12_REF_H

/**
 * @file byte_encode12_ref.h
 * @brief FIPS 203 Alg.5 ByteEncode₁₂ 的 Host/C 参考声明。
 *
 * 流水线位置：host 侧 golden / 对照实现头；与设备 byte_encode12_pair 语义对齐。
 * 与 golden 关系：gen_data 经 v2 链调用等价编码；本头定义单 poly / polyvec 字节长度常量。
 * 作用：声明 poly / polyvec 编码入口及 384 / 1536 字节常量。
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 单 poly ByteEncode₁₂ 输出字节数：256 系数 × 12 bit / 8 = 384 */
#define BYTE_ENCODE12_POLY_BYTES 384
/** k=4 polyvec 总字节数 */
#define BYTE_ENCODE12_POLYVEC_BYTES (4 * BYTE_ENCODE12_POLY_BYTES)

/**
 * 单 poly：int32 系数 → 12-bit 紧打包字节流。
 * @param r 输出 uint8[n/2*3]，通常 n=256 → 384 B
 * @param a 输入 int32[n]，仅低 12 bit 有效
 * @param n 系数个数（须为偶数）
 */
void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n);

/**
 * polyvec：连续 k 个 poly 依次 ByteEncode₁₂。
 * @param r     输出 uint8[k*384]
 * @param polys 输入 int32[k*n]，行优先
 * @param k     poly 个数
 * @param n     每 poly 系数数
 */
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
