/**
 * @file f203_kem_dec_phase_e_prep_entry.cpp
 * @brief Phase-E prep 核：先 G(m'‖h) 写 K'/coins，再跑 Encrypt prep（Â + CBD(r')）。
 *
 * 注册名：`f203_kem_dec_phase_e_prep`。
 * 分核：`F203_AHAT16_BLOCK_DIM` 路 BuildEncryptPrep；仅 block0 执行 KemDecPhaseEHead。
 *
 * CPU twin（ASCENDC_CPU_DEBUG 且 blockDim==2）：单线程串行跑两片 prep，避免 CPU 模拟多核竞态。
 * SIM/NPU：各 block 并行 prep；block0 先写 coins，再各 block 读同一 coins。
 *
 * 输入 ek 仅为 EncryptPrep 需要的 ek_pke 视图（1184B）；m'/h 来自 Phase-D / dk 切片。
 */
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_ub.hpp"
#include "f203_kem_dec_phase_e_init.hpp"

/**
 * @param ek_gm        ek（1184B）
 * @param m_prime_gm   m'（32B）
 * @param h_gm         h（32B）
 * @param Kprime_gm    出 K'（32B）
 * @param coins_gm     出 r'（32B），兼作 EncryptPrep 的 coins
 * @param a_hat_gm     出 Â 矩阵工作区
 * @param prf_out_gm   CBD 中间 PRF
 * @param re_gm        出 y/e1/e2 等噪声工作区
 * @param tiling_gm    Shake/prep tiling
 */
extern "C" __global__ __aicore__ void f203_kem_dec_phase_e_prep(GM_ADDR ek_gm, GM_ADDR m_prime_gm, GM_ADDR h_gm,
                                                                GM_ADDR Kprime_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm,
                                                                GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
    __gm__ uint8_t *coinsPtr = reinterpret_cast<__gm__ uint8_t *>(coins_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    // CPU：只让 block0 跑；串行完成 shard0+shard1，保证 coins 写后再被 prep 读
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
    // SIM/NPU：越界 block 退出；block0 写 G 输出；各 block 按 idx 建 Â/噪声片
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
/** Host launch 包装。 */
extern "C" void f203_kem_dec_phase_e_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                             uint8_t *m_prime_gm, uint8_t *h_gm, uint8_t *Kprime_gm, uint8_t *coins_gm,
                                             uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *re_gm, uint8_t *tiling_gm)
{
    f203_kem_dec_phase_e_prep<<<blockDim, l2ctrl, stream>>>(ek_gm, m_prime_gm, h_gm, Kprime_gm, coins_gm, a_hat_gm,
                                                            prf_out_gm, re_gm, tiling_gm);
}
#endif
