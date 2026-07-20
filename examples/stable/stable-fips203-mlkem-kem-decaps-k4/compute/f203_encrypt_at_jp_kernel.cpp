/**
 * @file f203_encrypt_at_jp_kernel.cpp
 * @brief Alg.14 行 18 NTT 域：û ← Âᵀ∘ŷ（双 AIV halfrows，AIV_ONLY）。
 *
 * 流水线位置：CPU 五 launch 第 3 核；SIM 生产路径内联于 `l18_l19`。
 * 前置：y_hat GM 已由 NTT(y) 写满；本核验证「GM rendezvous + 双 AIV 读全量 ŷ」。
 * I/O：aHat[16,256]、yHat[4,256] → uNtt[4,256] int32。与 golden：中间态不落盘。
 */
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "f203_encrypt_alg11_rom_weak.hpp"
#endif
#include "f203_encrypt_at_jp.hpp"
#include "kernel_operator.h"

/**
 * 设备核：AIV0 算 û 行 0–1，AIV1 算行 2–3；均读完整 ŷ。
 * @param uNtt 输出；@param aHat Â；@param yHat ŷ
 */
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
/** Host ACL 启动封装。 */
extern "C" void f203_encrypt_at_jp_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uNtt, uint8_t *aHat,
                                      uint8_t *yHat)
{
    f203_encrypt_at_jp<<<blockDim, l2ctrl, stream>>>(uNtt, aHat, yHat);
}
#endif
