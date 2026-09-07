/**
 * @file decompress_d1_ref.c
 * @brief FIPS 203 Decompress_1 消息嵌入 host 参考（golden 计算内核）。
 *
 * 本文件在流水线中的位置：由 scripts/gen_data.py 编译为 libdecompress_d1_ref.so 调用；
 * 不参与 AscendC 设备端编译，仅作 CPU/SIM 对拍黑盒 oracle。
 * 规范对齐：Alg.14 行 20 v ← v + HALF_Q·bit(m) (mod q)；与 Encrypt golden 一致。
 */
#include "decompress_d1_ref.h"

#include "f203_mlkem_params.h"

static int32_t mod_q_i32(int64_t v)
{
    const int32_t q = F203_MLKEM_Q;
    v %= q;
    if (v < 0) {
        v += q;
    }
    return (int32_t)v;
}

void embed_message_ref(int32_t *out, const int32_t *in, const uint8_t *m, int32_t n)
{
    const int32_t half_q = (F203_MLKEM_Q + 1) / 2;
    for (int32_t c = 0; c < n; ++c) {
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        const int32_t bit = ((int32_t)m[i] >> j) & 1;
        out[c] = mod_q_i32((int64_t)in[c] + (int64_t)half_q * bit);
    }
}

void mu_embed_only_ref(int32_t *mu_out, const uint8_t *m, int32_t n)
{
    const int32_t half_q = (F203_MLKEM_Q + 1) / 2;
    for (int32_t c = 0; c < n; ++c) {
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        const int32_t bit = ((int32_t)m[i] >> j) & 1;
        mu_out[c] = bit != 0 ? half_q : 0;
    }
}
