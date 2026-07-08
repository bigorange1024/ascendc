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

/** d=5：5B/组 → 8×5bit 系数（Alg.6 逆 ml-kem-1024 c₂）。 */
static void poly_byte_decode_d5_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        const int32_t offset = i * 5;
        uint8_t t[8];
        t[0] = (uint8_t)(0x1Fu & (in[offset + 0] >> 0));
        t[1] = (uint8_t)(0x1Fu & ((in[offset + 0] >> 5) | (in[offset + 1] << 3)));
        t[2] = (uint8_t)(0x1Fu & (in[offset + 1] >> 2));
        t[3] = (uint8_t)(0x1Fu & ((in[offset + 1] >> 7) | (in[offset + 2] << 1)));
        t[4] = (uint8_t)(0x1Fu & ((in[offset + 2] >> 4) | (in[offset + 3] << 4)));
        t[5] = (uint8_t)(0x1Fu & (in[offset + 3] >> 1));
        t[6] = (uint8_t)(0x1Fu & ((in[offset + 3] >> 6) | (in[offset + 4] << 2)));
        t[7] = (uint8_t)(0x1Fu & (in[offset + 4] >> 3));
        for (int32_t j = 0; j < 8; ++j) {
            out[8 * i + j] = (int32_t)t[j];
        }
    }
}

/** d=11：11B/组 → 8×11bit 系数（ML-KEM-1024 c₁）。 */
static void poly_byte_decode_d11_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        const uint8_t *base = &in[11 * j];
        uint16_t t[8];
        t[0] = (uint16_t)(0x7FFu & ((base[0] >> 0) | ((uint16_t)base[1] << 8)));
        t[1] = (uint16_t)(0x7FFu & ((base[1] >> 3) | ((uint16_t)base[2] << 5)));
        t[2] = (uint16_t)(0x7FFu & ((base[2] >> 6) | ((uint16_t)base[3] << 2) | ((uint16_t)base[4] << 10)));
        t[3] = (uint16_t)(0x7FFu & ((base[4] >> 1) | ((uint16_t)base[5] << 7)));
        t[4] = (uint16_t)(0x7FFu & ((base[5] >> 4) | ((uint16_t)base[6] << 4)));
        t[5] = (uint16_t)(0x7FFu & ((base[6] >> 7) | ((uint16_t)base[7] << 1) | ((uint16_t)base[8] << 9)));
        t[6] = (uint16_t)(0x7FFu & ((base[8] >> 2) | ((uint16_t)base[9] << 6)));
        t[7] = (uint16_t)(0x7FFu & ((base[9] >> 5) | ((uint16_t)base[10] << 3)));
        for (int32_t k = 0; k < 8; ++k) {
            out[8 * j + k] = (int32_t)t[k];
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

void poly_byte_decode_d5_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d5_c(out, in, n);
}

void poly_byte_decode_d11_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d11_c(out, in, n);
}
