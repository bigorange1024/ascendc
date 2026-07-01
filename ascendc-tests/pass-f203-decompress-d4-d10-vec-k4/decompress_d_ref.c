#include "decompress_d_ref.h"

#include "f203_mlkem_params.h"

static void poly_decompress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t u = (uint32_t)in[i];
        if (d == 4) {
            out[i] = (int32_t)((((uint32_t)u * (uint32_t)F203_MLKEM_Q) + 8u) >> 4);
        } else if (d == 10) {
            out[i] = (int32_t)((((uint32_t)u * (uint32_t)F203_MLKEM_Q) + 512u) >> 10);
        }
    }
}

void poly_decompress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 4);
}

void poly_decompress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 10);
}
