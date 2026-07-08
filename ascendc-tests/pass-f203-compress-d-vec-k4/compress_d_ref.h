#ifndef COMPRESS_D_REF_H
#define COMPRESS_D_REF_H

#include <stdint.h>

#include "f203_mlkem_params.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t f203_scalar_compress_d4(uint32_t u);
uint32_t f203_scalar_compress_d5(uint32_t u);
uint32_t f203_scalar_compress_d10(uint32_t u);
uint32_t f203_scalar_compress_d11(uint32_t u);

void poly_compress_d4_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d5_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d10_ref(int32_t *out, const int32_t *in, int32_t n);
void poly_compress_d11_ref(int32_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
