/**
 * @file compress_unified_int_ref.c
 * @brief 统一整数 Compress_d golden：round(u·2^d/q) = (C·u + 2^(36-d)) >> (37-d)。
 *
 * 编译期 F203_UNIFIED_ROUND_D 须与 kernel 一致；C=41285357 见技术总结 §2.3。
 */
#include "compress_unified_int_ref.h"
#include "f203_mlkem_params.h"

#include <stdint.h>

#ifndef F203_UNIFIED_ROUND_D
#define F203_UNIFIED_ROUND_D 4
#endif

#define F203_UNIFIED_C 41285357LL

static int32_t compress_one(int32_t u)
{
    const int k = 37 - F203_UNIFIED_ROUND_D;
    const int64_t bias = 1LL << (k - 1);
    const int64_t prod = F203_UNIFIED_C * (int64_t)u;
    return (int32_t)((prod + bias) >> k);
}

void poly_compress_unified_ref(int32_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n; ++i) {
        out[i] = compress_one(in[i]);
    }
}
