/**
 * @file byte_encode12_ref.h
 * @brief Host/C golden：FIPS 203 Alg.5 ByteEncode₁₂ 单 poly / polyvec API。
 *
 * 用途：gen_data.py 编译 libbyte_encode12_ref.so，生成 golden_ek.bin、golden_sk.bin（各 4×384B）。
 *
 * 调用方：scripts/gen_data.py；设备侧见 byte_encode12_pair.hpp。
 *
 * 不变量：BYTE_ENCODE12_POLY_BYTES=384；n=256 系数；每对系数 12 bit 打包为 3 字节。
 *
 * Golden：即本库输出；verify_result.py cmp ek_out/sk_out。
 *
 * CMake：无（gen_data gcc -shared 编译 byte_encode12_ref.c）。
 */
#ifndef BYTE_ENCODE12_REF_H
#define BYTE_ENCODE12_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTE_ENCODE12_POLY_BYTES 384
#define BYTE_ENCODE12_POLYVEC_BYTES (4 * BYTE_ENCODE12_POLY_BYTES)

/** 单 poly：int32[n] → uint8[3*n/2]（n=256 → 384B）。 */
void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n);
/** polyvec：k 个 poly 拼接编码（k=4 → 1536B）。 */
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
