/**
 * @file f203_kem_enc_prep_entry.cpp
 * @brief Alg.20/17 Encaps prep：先 KEM 头（H/G），再 stable Encrypt prep（Â + CBD）。
 *
 * 注册符号 `f203_kem_enc_prep`（替换纯 `f203_encrypt_prep`）。
 * 双 AIV：仅 block0（及 CPU 串行入口）跑 KemEncInitHead；随后各核跑 Â 分片，
 * CBD 仍仅 block0（见 BuildEncryptPrepSinglePipe）。
 *
 * @param ek_gm / m_gm / K_gm / coins_gm 见 KemEncInitHead
 * @param a_hat_gm / prf_out_gm / re_gm / tiling_gm 同 stable Encrypt prep
 */
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_ub.hpp"
#include "f203_kem_enc_init.hpp"

extern "C" __global__ __aicore__ void f203_kem_enc_prep(GM_ADDR ek_gm, GM_ADDR m_gm, GM_ADDR K_gm, GM_ADDR coins_gm,
                                                        GM_ADDR a_hat_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                        GM_ADDR tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
    __gm__ uint8_t *coinsPtr = reinterpret_cast<__gm__ uint8_t *>(coins_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    // CPU：单 block 串行两次 Â 分片；头只跑一次
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm), reinterpret_cast<__gm__ uint8_t *>(m_gm),
                               reinterpret_cast<__gm__ uint8_t *>(K_gm), coinsPtr);
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
    // SIM/NPU：block0 先写 coins/K，再与 block1 并行做 Â；CBD 在 block0 的 SinglePipe 内
    if (AscendC::GetBlockIdx() == 0U) {
        F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm),
                                   reinterpret_cast<__gm__ uint8_t *>(m_gm),
                                   reinterpret_cast<__gm__ uint8_t *>(K_gm), coinsPtr);
    }
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, coinsPtr, AscendC::GetBlockIdx(),
                                                reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#endif
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_enc_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm, uint8_t *m_gm,
                                     uint8_t *K_gm, uint8_t *coins_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm,
                                     uint8_t *re_gm, uint8_t *tiling_gm)
{
    f203_kem_enc_prep<<<blockDim, l2ctrl, stream>>>(ek_gm, m_gm, K_gm, coins_gm, a_hat_gm, prf_out_gm, re_gm,
                                                    tiling_gm);
}
#endif
