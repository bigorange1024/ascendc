/**
 * Alg.13 NTT 对拍：逐行调用 ntt_study MlkemNtt（FIPS 203 Alg.9）。
 * 口径对齐 stage123_stage31mod_golden_app：输入 MlkemReduceToZq → int16 NTT → 输出再 ReduceToZq。
 */
#include "mlkem_ntt_batch_ref.h"

#include "mlkem_ntt.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    NTT_N = 256
};

void mlkem_ntt_batch_ref(int32_t *out, const int32_t *in, int k_polys)
{
    int16_t tmp[MLKEM_N];

    for (int k = 0; k < k_polys; k++)
    {
        for (uint32_t j = 0; j < MLKEM_N; j++)
        {
            tmp[j] = (int16_t)MlkemReduceToZq(in[(size_t)k * (size_t)NTT_N + (size_t)j]);
        }
        MlkemNtt(tmp);
        for (uint32_t j = 0; j < MLKEM_N; j++)
        {
            out[(size_t)k * (size_t)NTT_N + (size_t)j] = MlkemReduceToZq((int32_t)tmp[j]);
        }
    }
}
