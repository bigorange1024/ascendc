/**
 * @file decompress_unified_int_ref.h
 * @brief 统一整数 Decompress_d Host golden 参考（C ABI）。
 */
#ifndef DECOMPRESS_UNIFIED_INT_REF_H
#define DECOMPRESS_UNIFIED_INT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void poly_decompress_unified_ref(int32_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
