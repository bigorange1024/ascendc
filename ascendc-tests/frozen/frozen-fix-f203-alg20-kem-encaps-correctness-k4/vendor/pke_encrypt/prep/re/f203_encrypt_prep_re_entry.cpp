/**
 * @file f203_encrypt_prep_re_entry.cpp
 * @brief Alg.14 G1 Launch-2：coins[32] → r[4,256], e1[4,256], e2[256] int32 GM。
 *
 * ML-KEM Encrypt：9× CBD（8×η₁ + 1×η₂），PRF 密钥为 coins，nonce 0..8（SHAKE256）。
 */
#include "f203_encrypt_re_layout.h"
#include "f203_encrypt_re_vector.hpp"

extern "C" __global__ __aicore__ void f203_encrypt_prep_re(GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                           GM_ADDR tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    const __gm__ uint8_t *coinsPtr = reinterpret_cast<const __gm__ uint8_t *>(coins_gm);
    F203EncryptRe::BuildReFromCoinsGm(coinsPtr, prf_out_gm, re_gm, tiling);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_prep_re_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *coins_gm,
                                        uint8_t *prf_out_gm, uint8_t *re_gm, uint8_t *tiling)
{
    f203_encrypt_prep_re<<<blockDim, l2ctrl, stream>>>(coins_gm, prf_out_gm, re_gm, tiling);
}
#endif
