#include "decompress_d_ref.h"

#include "f203_mlkem_params.h"

static void poly_decompress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    uint32_t bias = 0U;
    if (d == 4) {
        bias = 8U;
    } else if (d == 5) {
        bias = 16U;
    } else if (d == 10) {
        bias = 512U;
    } else if (d == 11) {
        bias = 1024U;
    }
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t u = (uint32_t)in[i];
        out[i] = (int32_t)((((uint32_t)u * (uint32_t)F203_MLKEM_Q) + bias) >> d);
    }
}

void poly_decompress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 4);
}

void poly_decompress_d5_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 5);
}

void poly_decompress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 10);
}

void poly_decompress_d11_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 11);
}
