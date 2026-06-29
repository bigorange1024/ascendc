/**
 * @file f203_a_hat16_entry.cpp
 * @brief Alg.13 行 3–7 设备核：SEED_D → 向量 SampleNTT → a_hat[16,256] GM（1 或 2 AIV）。
 */
#include "f203_a_hat16_config.h"
#include "f203_a_hat16_layout.h"
#include "f203_a_hat16_ub.hpp"

extern "C" __global__ __aicore__ void f203_alg13_a_hat_16poly(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seedPtr[0];
    __gm__ int32_t *aHatGm = reinterpret_cast<__gm__ int32_t *>(a_hat_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    // tikicpu 多 block 时 GetBlockIdx 不可靠；仅在 block0 串行跑两分片。
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    F203Ahat16::BuildAHat16ShardFromSeedD(seed_d, aHatGm, 0U);
    F203Ahat16::BuildAHat16ShardFromSeedD(seed_d, aHatGm, 1U);
#else
    F203Ahat16::BuildAHat16ShardFromSeedD(seed_d, aHatGm, AscendC::GetBlockIdx());
#endif
}

/*
 * blockDim 语义（SIM/NPU）：
 * - BLOCK_DIM=2：AIV0→poly 0–7，AIV1→8–15；每 poly 内嵌 SHAKE 须 ProcessInline（见 shake_ub_helpers.hpp）。
 * - CPU_DEBUG 且 BLOCK_DIM=2：GetBlockIdx 不可靠，block0 内串行两分片（仅 tikicpu）。
 */

#ifndef __CCE_KT_TEST__
extern "C" void f203_alg13_a_hat_16poly_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                           uint8_t *a_hat_gm)
{
    f203_alg13_a_hat_16poly<<<blockDim, l2ctrl, stream>>>(seed_d_gm, a_hat_gm);
}
#endif
