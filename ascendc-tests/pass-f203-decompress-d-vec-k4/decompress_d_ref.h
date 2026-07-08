#ifndef DECOMPRESS_D_REF_H
#define DECOMPRESS_D_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void poly_decompress_d4_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_decompress_d5_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_decompress_d10_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_decompress_d11_ref(int32_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
