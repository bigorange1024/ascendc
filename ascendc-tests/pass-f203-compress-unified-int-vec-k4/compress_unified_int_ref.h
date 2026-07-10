/**
 * @file compress_unified_int_ref.h
 * @brief 统一整数 Compress_d Host golden 参考（C ABI）。
 */
#ifndef COMPRESS_UNIFIED_INT_REF_H
#define COMPRESS_UNIFIED_INT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void poly_compress_unified_ref(int32_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
