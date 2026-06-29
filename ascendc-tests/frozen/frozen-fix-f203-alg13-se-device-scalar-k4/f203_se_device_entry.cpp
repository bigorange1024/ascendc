/**
 * @file f203_se_device_entry.cpp
 * @brief 阶段一：Device 完整 Alg.13 行 8–15（SEED_D → src[8,256]）。
 */
#include "f203_se_device_scalar.hpp"

extern "C" __global__ __aicore__ void f203_se_device_k4(GM_ADDR seed_d_gm, GM_ADDR src_gm, GM_ADDR workspace,
                                                        GM_ADDR tiling)
{
    (void)workspace;
    (void)tiling;
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    const __gm__ uint32_t *seed_ptr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seed_ptr[0];
    __gm__ int32_t *dst = reinterpret_cast<__gm__ int32_t *>(src_gm);

    F203SeDevice::BuildSrcFromSeedD(seed_d, dst);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_se_device_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *src_gm, uint8_t *workspace, uint8_t *tiling)
{
    f203_se_device_k4<<<blockDim, l2ctrl, stream>>>(seed_d_gm, src_gm, workspace, tiling);
}
#endif
