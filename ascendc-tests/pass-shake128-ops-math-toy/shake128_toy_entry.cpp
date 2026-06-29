/**
 * 薄入口：SHAKE128 toy；仅 GM 回写 tiling.reserved2=PASS/FAIL，无 x/y GM 搬运。
 * 参考实现见 shake128_toy_ub.hpp。
 */
#include "shake128_toy_ub.hpp"
#include "shake_general_tiling_data.h"

extern "C" __global__ __aicore__ void shake128_general(GM_ADDR tiling)
{
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    const uint32_t pass = Shake128Toy::RunActiveCaseUb();
    __gm__ uint32_t *tp = reinterpret_cast<__gm__ uint32_t *>(tiling);
    tp[8] = pass;
}

#ifndef __CCE_KT_TEST__
extern "C" void shake128_general_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tiling)
{
    shake128_general<<<blockDim, l2ctrl, stream>>>(tiling);
}
#endif
