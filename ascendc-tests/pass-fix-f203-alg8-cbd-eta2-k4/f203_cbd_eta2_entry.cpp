/**
 * @file f203_cbd_eta2_entry.cpp
 * @brief FIPS 203 Alg.8 SamplePolyCBD η=2 — 8×poly 设备核入口（k=4，η=2）。
 *
 * AIV_ONLY；P2 默认 blockDim=2（SIM/NPU）。CPU 孪生由 main 固定 blockDim=1 launch，
 * 内核见 GetBlockNum()==1 时 block0 串行 8 行（避免 tikicpu 按 block 误起多颗 AIC）。
 */
#include "f203_cbd_eta2.hpp"

extern "C" __global__ __aicore__ void f203_cbd_eta2_batch8(GM_ADDR prf_gm, GM_ADDR src_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

#if F203_CBD_BLOCK_DIM == 1
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
#else
    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    F203CbdEta2::SamplePolyCbd2Batch8(reinterpret_cast<__gm__ uint8_t *>(prf_gm),
                                      reinterpret_cast<__gm__ int32_t *>(src_gm));
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_cbd_eta2_batch8_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *prf_gm,
                                        uint8_t *src_gm)
{
    f203_cbd_eta2_batch8<<<blockDim, l2ctrl, stream>>>(prf_gm, src_gm);
}
#endif
