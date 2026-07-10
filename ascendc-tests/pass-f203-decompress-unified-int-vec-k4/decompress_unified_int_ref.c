/**
 * @file decompress_unified_int_ref.c
 * @brief 统一整数 Decompress_d golden：(c·q + 2^(d-1)) >> d。
 */
#include "decompress_unified_int_ref.h"
#include "f203_mlkem_params.h"

#include <stdint.h>

#ifndef F203_UNIFIED_ROUND_D
#define F203_UNIFIED_ROUND_D 4
#endif

static int32_t decompress_one(int32_t c)
{
    const int64_t bias = 1LL << (F203_UNIFIED_ROUND_D - 1);
    const int64_t num = (int64_t)c * (int64_t)F203_MLKEM_Q + bias;
    return (int32_t)(num >> F203_UNIFIED_ROUND_D);
}

void poly_decompress_unified_ref(int32_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n; ++i) {
        out[i] = decompress_one(in[i]);
    }
}
