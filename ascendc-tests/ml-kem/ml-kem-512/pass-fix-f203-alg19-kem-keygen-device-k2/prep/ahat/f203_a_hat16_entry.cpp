
/** 独立 Â 探针入口；全链由 f203_keygen_prep 内联 BuildAHat16ShardWithUb。 */
// @probe pass-fix-f203-alg13-device-keygen-k2
// @file prep/ahat/f203_a_hat16_entry.cpp
// @layer prep
// @role prep/ahat：设备侧生成矩阵 A_hat（FIPS203 Alg.6/布局 f203_a_hat16）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。 / Device A_hat generation for keygen prep. 本文件 `f203_a_hat16_entry.cpp` 为该子模块组件。 / Component: f203_a_hat16_entry.cpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_a_hat16_config.h, f203_a_hat16_layout.h, f203_a_hat16_ub.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Â[4,256] 分片构建。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/ahat/f203_a_hat16_entry.cpp
 */
/**
 * @file f203_a_hat16_entry.cpp
 * @brief Alg.13 行 3–7 设备核：SEED_D → 向量 SampleNTT → a_hat[4,256] GM（1 或 2 AIV）。
 */
#include "f203_a_hat16_config.h"
#include "f203_a_hat16_layout.h"
#include "f203_a_hat16_ub.hpp"

extern "C" __global__ __aicore__ void f203_alg13_a_hat_16poly(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    // 独立 Â 探针；blockIdx 分片或 CPU 串行两分片

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
 * - BLOCK_DIM=2：AIV0→poly 0–1，AIV1→2–3；每 poly 内嵌 SHAKE 须 ProcessInline（见 shake_ub_helpers.hpp）。
 * - CPU_DEBUG 且 BLOCK_DIM=2：GetBlockIdx 不可靠，block0 内串行两分片（仅 tikicpu）。
 */

#ifndef __CCE_KT_TEST__
extern "C" void f203_alg13_a_hat_16poly_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                           uint8_t *a_hat_gm)
{
    f203_alg13_a_hat_16poly<<<blockDim, l2ctrl, stream>>>(seed_d_gm, a_hat_gm);
}
#endif
