/**
 * @file f203_encrypt_prep_entry.cpp
 * @brief Alg.14 行 3–15 设备入口：单 launch `f203_encrypt_prep`（ek+coins → a_hat + re）。
 *
 * 数学：
 *   行 3–7：ρ = ek_pke[1536:1568] → SampleNTT → a_hat[16,256]
 *   行 8–15：coins → PRF+CBD(η=2) batch9 → re[9,256]（nonce 0–3=r, 4–7=e₁, 8=e₂）
 *
 * 编排：双 AIV 并行 Â 分片（block0→poly 0–7，block1→8–15）；PRF/CBD 仅 block0。
 * CPU tikicpu：blockDim=2 时单 block 串行跑两次分片（见 ASCENDC_CPU_DEBUG 分支）。
 * 代码来源：stable KeyGen prep vendoring；禁止抄 correctness 全链。
 */
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_ub.hpp"

extern "C" __global__ __aicore__ void f203_encrypt_prep(GM_ADDR ek_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm,
                                                        GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
    const __gm__ uint8_t *coinsPtr = reinterpret_cast<const __gm__ uint8_t *>(coins_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
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
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, coinsPtr, AscendC::GetBlockIdx(),
                                                reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#endif
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                       uint8_t *coins_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *re_gm,
                                       uint8_t *tiling_gm)
{
    f203_encrypt_prep<<<blockDim, l2ctrl, stream>>>(ek_gm, coins_gm, a_hat_gm, prf_out_gm, re_gm, tiling_gm);
}
#endif
