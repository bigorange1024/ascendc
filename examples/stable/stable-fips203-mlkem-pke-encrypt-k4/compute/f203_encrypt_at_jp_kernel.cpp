/**
 * @file f203_encrypt_at_jp_kernel.cpp
 * @brief 行 19 NTT 域段：û ← Âᵀ∘ŷ（双 AIV halfrows，AIV_ONLY）。
 *
 * 前置：y_hat GM 已由行 18 写满；本核仅验证「GM rendezvous + 双 AIV 读全量 ŷ」。
 */
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "f203_encrypt_alg11_rom_weak.hpp"
#endif
#include "f203_encrypt_at_jp.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetBlockIdx());
    if (subBlockID >= 2) {
        return;
    }
    const int32_t pBegin = subBlockID * 2;
    const int32_t pEnd = pBegin + 2;
    encrypt_at_jp::innerproduct_halfrows_to_gm(aHat, yHat, uNtt, pBegin, pEnd);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_at_jp_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uNtt, uint8_t *aHat,
                                      uint8_t *yHat)
{
    f203_encrypt_at_jp<<<blockDim, l2ctrl, stream>>>(uNtt, aHat, yHat);
}
#endif
