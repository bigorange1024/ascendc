/**
 * FIPS 203 Alg.5 ByteEncode₁₂ — Host/C golden（与 byte_encode12_aiv.hpp 语义一致）。
 */
#include "byte_encode12_ref.h"

void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n)
{
    const int32_t pairs = n / 2;
    for (int32_t i = 0; i < pairs; i++) {
        const uint16_t t0 = (uint16_t)(a[2 * i] & 0xFFF);
        const uint16_t t1 = (uint16_t)(a[2 * i + 1] & 0xFFF);
        r[3 * i + 0] = (uint8_t)(t0 & 0xFF);
        r[3 * i + 1] = (uint8_t)((t0 >> 8) | ((t1 << 4) & 0xF0));
        r[3 * i + 2] = (uint8_t)(t1 >> 4);
    }
}

void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n)
{
    for (int32_t p = 0; p < k; p++) {
        poly_byte_encode12_ref(r + p * BYTE_ENCODE12_POLY_BYTES, polys + p * n, n);
    }
}
