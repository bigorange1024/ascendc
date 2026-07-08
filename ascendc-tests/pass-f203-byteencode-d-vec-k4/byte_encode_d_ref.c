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

/** d=5：8 系数 × 5bit → 5B/组（与 FIPS Alg.5 比特流及 ml-kem-1024 c₂ 布局一致）。 */
static void poly_byte_encode_d5_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = (uint8_t)(in[8 * i + j] & 0x1F);
        }
        out[i * 5 + 0] = (uint8_t)(0xFF & ((t[0] >> 0) | (t[1] << 5)));
        out[i * 5 + 1] = (uint8_t)(0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7)));
        out[i * 5 + 2] = (uint8_t)(0xFF & ((t[3] >> 1) | (t[4] << 4)));
        out[i * 5 + 3] = (uint8_t)(0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6)));
        out[i * 5 + 4] = (uint8_t)(0xFF & ((t[6] >> 2) | (t[7] << 3)));
    }
}

/** d=11：8 系数 × 11bit → 11B/组（ML-KEM-1024 c₁ 单 poly 352B）。 */
static void poly_byte_encode_d11_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        uint16_t t[8];
        for (int32_t k = 0; k < 8; ++k) {
            t[k] = (uint16_t)(in[8 * j + k] & 0x7FF);
        }
        out[11 * j + 0] = (uint8_t)((t[0] >> 0) & 0xFF);
        out[11 * j + 1] = (uint8_t)((t[0] >> 8) | ((t[1] << 3) & 0xFF));
        out[11 * j + 2] = (uint8_t)((t[1] >> 5) | ((t[2] << 6) & 0xFF));
        out[11 * j + 3] = (uint8_t)((t[2] >> 2) & 0xFF);
        out[11 * j + 4] = (uint8_t)((t[2] >> 10) | ((t[3] << 1) & 0xFF));
        out[11 * j + 5] = (uint8_t)((t[3] >> 7) | ((t[4] << 4) & 0xFF));
        out[11 * j + 6] = (uint8_t)((t[4] >> 4) | ((t[5] << 7) & 0xFF));
        out[11 * j + 7] = (uint8_t)((t[5] >> 1) & 0xFF);
        out[11 * j + 8] = (uint8_t)((t[5] >> 9) | ((t[6] << 2) & 0xFF));
        out[11 * j + 9] = (uint8_t)((t[6] >> 6) | ((t[7] << 5) & 0xFF));
        out[11 * j + 10] = (uint8_t)(t[7] >> 3);
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

void poly_byte_encode_d5_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d5_c(out, in, n);
}

void poly_byte_encode_d11_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d11_c(out, in, n);
}
