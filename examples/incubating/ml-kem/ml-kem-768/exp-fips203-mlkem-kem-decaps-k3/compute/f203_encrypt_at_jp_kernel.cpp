/**
 * @file f203_encrypt_at_jp_kernel.cpp
 * @brief Alg.14 行 18 NTT 域段（独立 launch）：û ← Âᵀ∘ŷ（双 AIV halfrows，AIV_ONLY）。
 *
 * 流水线：CPU 三 launch 中段；SIM 生产默认走融合核，本核供分段调试。
 * 前置：y_hat GM 已写满；本核验证「GM rendezvous + 双 AIV 读全量 ŷ」。
 * Golden：a_hat + y_hat → u_ntt.bin。
 */
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "f203_encrypt_alg11_rom_weak.hpp"
#endif
#include "f203_encrypt_at_jp.hpp"
#include "kernel_operator.h"

/**
 * AIV_ONLY：block0 写 û0/û1，block1 只写 û2；禁止写第 4 行。
 * @param uNtt 输出 [kK,N]；@param aHat Â；@param yHat ŷ
 */
extern "C" __global__ __aicore__ void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetBlockIdx());
    if (subBlockID >= 2) {
        return;
    }
    const int32_t pBegin = (subBlockID == 0) ? 0 : 2;
    const int32_t pEnd = (subBlockID == 0) ? 2 : 3;
    encrypt_at_jp::innerproduct_halfrows_to_gm(aHat, yHat, uNtt, pBegin, pEnd);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_at_jp_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uNtt, uint8_t *aHat,
                                      uint8_t *yHat)
{
    f203_encrypt_at_jp<<<blockDim, l2ctrl, stream>>>(uNtt, aHat, yHat);
}
#endif
