#ifndef BYTE_ENCODE_D_REF_H
#define BYTE_ENCODE_D_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void poly_byte_encode_d4_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d5_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d10_ref(uint8_t *out, const int32_t *in, int32_t n);
void poly_byte_encode_d11_ref(uint8_t *out, const int32_t *in, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
