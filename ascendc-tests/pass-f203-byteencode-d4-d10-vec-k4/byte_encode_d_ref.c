#include "byte_encode_d_ref.h"

#include "f203_mlkem_params.h"

static void poly_byte_encode_d4_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = (uint8_t)(in[8 * i + j] & 0xF);
        }
        out[i * 4 + 0] = (uint8_t)(t[0] | (t[1] << 4));
        out[i * 4 + 1] = (uint8_t)(t[2] | (t[3] << 4));
        out[i * 4 + 2] = (uint8_t)(t[4] | (t[5] << 4));
        out[i * 4 + 3] = (uint8_t)(t[6] | (t[7] << 4));
    }
}

static void poly_byte_encode_d10_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 4; ++j) {
        uint16_t t[4];
        for (int32_t k = 0; k < 4; ++k) {
            t[k] = (uint16_t)(in[4 * j + k] & 0x3FF);
        }
        out[5 * j + 0] = (uint8_t)((t[0] >> 0) & 0xFF);
        out[5 * j + 1] = (uint8_t)((t[0] >> 8) | ((t[1] << 2) & 0xFF));
        out[5 * j + 2] = (uint8_t)((t[1] >> 6) | ((t[2] << 4) & 0xFF));
        out[5 * j + 3] = (uint8_t)((t[2] >> 4) | ((t[3] << 6) & 0xFF));
        out[5 * j + 4] = (uint8_t)(t[3] >> 2);
    }
}

void poly_byte_encode_d4_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d4_c(out, in, n);
}

void poly_byte_encode_d10_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d10_c(out, in, n);
}
