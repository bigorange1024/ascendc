/**
 * @file f203_kem_enc_prep_re_entry.cpp
 * @brief KEM Encaps：融合 Alg.17 头（m/H/G）+ Alg.14 prep_re（coins→r/e₁/e₂）。
 *
 * 背景：func_key≤5 约束下不新增独立 AIV 核；在 prep_re 入口先写 coins/m/K。
 */
#include "f203_encrypt_re_layout.h"
#include "f203_encrypt_re_vector.hpp"
#include "f203_kem_enc_init.hpp"

extern "C" __global__ __aicore__ void f203_kem_enc_prep_re(GM_ADDR ek_gm, GM_ADDR seed_d_gm, GM_ADDR K_gm, GM_ADDR m_gm,
                                                           GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                           GM_ADDR tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm),
                               reinterpret_cast<__gm__ uint8_t *>(seed_d_gm),
                               reinterpret_cast<__gm__ uint8_t *>(K_gm),
                               reinterpret_cast<__gm__ uint8_t *>(m_gm),
                               reinterpret_cast<__gm__ uint8_t *>(coins_gm));

    const __gm__ uint8_t *coinsPtr = reinterpret_cast<const __gm__ uint8_t *>(coins_gm);
    F203EncryptRe::BuildReFromCoinsGm(coinsPtr, prf_out_gm, re_gm, tiling);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_enc_prep_re_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                        uint8_t *seed_d_gm, uint8_t *K_gm, uint8_t *m_gm, uint8_t *coins_gm,
                                        uint8_t *prf_out_gm, uint8_t *re_gm, uint8_t *tiling)
{
    f203_kem_enc_prep_re<<<blockDim, l2ctrl, stream>>>(ek_gm, seed_d_gm, K_gm, m_gm, coins_gm, prf_out_gm, re_gm,
                                                     tiling);
}
#endif
