#include "byte_decode_d_ref.h"

#include "f203_mlkem_params.h"

static void poly_byte_decode_d4_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 2; ++i) {
        out[2 * i + 0] = (int32_t)(in[i] & 0xFu);
        out[2 * i + 1] = (int32_t)((in[i] >> 4) & 0xFu);
    }
}

static void poly_byte_decode_d10_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 4; ++j) {
        const uint8_t *base = &in[5 * j];
        uint16_t t[4];
        t[0] = (uint16_t)(0x3FFu & ((base[0] >> 0) | ((uint16_t)base[1] << 8)));
        t[1] = (uint16_t)(0x3FFu & ((base[1] >> 2) | ((uint16_t)base[2] << 6)));
        t[2] = (uint16_t)(0x3FFu & ((base[2] >> 4) | ((uint16_t)base[3] << 4)));
        t[3] = (uint16_t)(0x3FFu & ((base[3] >> 6) | ((uint16_t)base[4] << 2)));
        for (int32_t k = 0; k < 4; ++k) {
            out[4 * j + k] = (int32_t)t[k];
        }
    }
}

void poly_byte_decode_d4_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d4_c(out, in, n);
}

void poly_byte_decode_d10_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d10_c(out, in, n);
}
