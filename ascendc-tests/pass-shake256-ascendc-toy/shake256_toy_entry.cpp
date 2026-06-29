/**
 * 薄入口：SHAKE256 toy；仅 GM 回写 tiling.reserved2=PASS/FAIL。
 */
#include "shake256_toy_ub.hpp"
#include "shake_general_tiling_data.h"

extern "C" __global__ __aicore__ void shake256_general(GM_ADDR tiling)
{
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    const uint32_t pass = Shake256Toy::RunActiveCaseUb();
    __gm__ uint32_t *tp = reinterpret_cast<__gm__ uint32_t *>(tiling);
    tp[8] = pass;
}

#ifndef __CCE_KT_TEST__
extern "C" void shake256_general_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tiling)
{
    shake256_general<<<blockDim, l2ctrl, stream>>>(tiling);
}
#endif
