#include "compress_d_ref.h"

#include <stdint.h>

uint32_t f203_scalar_compress_d4(uint32_t u)
{
    const uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
}

uint32_t f203_scalar_compress_d5(uint32_t u)
{
    const uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
}

uint32_t f203_scalar_compress_d10(uint32_t u)
{
    uint64_t d0 = (uint64_t)u * 2642263040ull;
    d0 = (d0 + ((uint64_t)1u << 32)) >> 33;
    return (uint32_t)(d0 & 0x3ffu);
}

uint32_t f203_scalar_compress_d11(uint32_t u)
{
    uint64_t d0 = (uint64_t)u * 5284526080ull;
    d0 = (d0 + ((uint64_t)1u << 32)) >> 33;
    return (uint32_t)(d0 & 0x7ffu);
}

static uint32_t scalar_compress(uint32_t u, int d)
{
    if (u >= (uint32_t)F203_MLKEM_Q) {
        u = (uint32_t)F203_MLKEM_Q - 1u;
    }
    switch (d) {
    case 4:
        return f203_scalar_compress_d4(u);
    case 5:
        return f203_scalar_compress_d5(u);
    case 10:
        return f203_scalar_compress_d10(u);
    case 11:
        return f203_scalar_compress_d11(u);
    default:
        return 0u;
    }
}

static void poly_compress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t u = (uint32_t)in[i];
        out[i] = (int32_t)scalar_compress(u, d);
    }
}

void poly_compress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 4);
}

void poly_compress_d5_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 5);
}

void poly_compress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 10);
}

void poly_compress_d11_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 11);
}
