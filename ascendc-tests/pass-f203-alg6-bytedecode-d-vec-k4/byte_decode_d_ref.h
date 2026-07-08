#ifndef BYTE_DECODE_D_REF_H
#define BYTE_DECODE_D_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void poly_byte_decode_d4_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d5_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d10_ref(int32_t *out, const uint8_t *in, int32_t n);
void poly_byte_decode_d11_ref(int32_t *out, const uint8_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
