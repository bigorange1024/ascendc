#include "compress_d_ref.h"

#include <stdint.h>

uint32_t f203_scalar_compress_d4(uint32_t u)
{
    uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
}

uint32_t f203_scalar_compress_d10(uint32_t u)
{
    uint64_t d0 = (uint64_t)u * 2642263040ull;
    d0 = (d0 + ((uint64_t)1u << 32)) >> 33;
    return (uint32_t)(d0 & 0x3ffu);
}

static void poly_compress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    for (int32_t i = 0; i < n; ++i) {
        uint32_t u = (uint32_t)in[i];
        if (u >= (uint32_t)F203_MLKEM_Q) {
            u = (uint32_t)F203_MLKEM_Q - 1u;
        }
        if (d == 4) {
            out[i] = (int32_t)f203_scalar_compress_d4(u);
        } else if (d == 10) {
            out[i] = (int32_t)f203_scalar_compress_d10(u);
        }
    }
}

void poly_compress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 4);
}

void poly_compress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 10);
}
