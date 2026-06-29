#ifndef BYTE_ENCODE12_REF_H
#define BYTE_ENCODE12_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTE_ENCODE12_POLY_BYTES 384
#define BYTE_ENCODE12_POLYVEC_BYTES (4 * BYTE_ENCODE12_POLY_BYTES)

void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n);
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
