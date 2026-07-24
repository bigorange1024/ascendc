/**
 * @file f203_kem_dec_phase_e_prep_entry.cpp
 * @brief Phase-E prep：先 G(m'‖h)，再 stable Encrypt prep（Â + CBD）。
 *
 * 注册 `f203_kem_dec_phase_e_prep`。仅 block0 跑 KemDecPhaseEHead。
 */
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_ub.hpp"
#include "f203_kem_dec_phase_e_init.hpp"

extern "C" __global__ __aicore__ void f203_kem_dec_phase_e_prep(GM_ADDR ek_gm, GM_ADDR m_prime_gm, GM_ADDR h_gm,
                                                                GM_ADDR Kprime_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm,
                                                                GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
    __gm__ uint8_t *coinsPtr = reinterpret_cast<__gm__ uint8_t *>(coins_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    F203KemDec::KemDecPhaseEHead(reinterpret_cast<__gm__ uint8_t *>(m_prime_gm),
                                 reinterpret_cast<__gm__ uint8_t *>(h_gm),
                                 reinterpret_cast<__gm__ uint8_t *>(Kprime_gm), coinsPtr);
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, coinsPtr, 0U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, coinsPtr, 1U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#else
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }
    if (AscendC::GetBlockIdx() == 0U) {
        F203KemDec::KemDecPhaseEHead(reinterpret_cast<__gm__ uint8_t *>(m_prime_gm),
                                     reinterpret_cast<__gm__ uint8_t *>(h_gm),
                                     reinterpret_cast<__gm__ uint8_t *>(Kprime_gm), coinsPtr);
    }
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, coinsPtr, AscendC::GetBlockIdx(),
                                                reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#endif
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_dec_phase_e_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                             uint8_t *m_prime_gm, uint8_t *h_gm, uint8_t *Kprime_gm, uint8_t *coins_gm,
                                             uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *re_gm, uint8_t *tiling_gm)
{
    f203_kem_dec_phase_e_prep<<<blockDim, l2ctrl, stream>>>(ek_gm, m_prime_gm, h_gm, Kprime_gm, coins_gm, a_hat_gm,
                                                            prf_out_gm, re_gm, tiling_gm);
}
#endif
