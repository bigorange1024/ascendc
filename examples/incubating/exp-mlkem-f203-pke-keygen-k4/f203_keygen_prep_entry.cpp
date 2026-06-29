// @exp exp-mlkem-f203-pke-keygen-k4
// @file f203_keygen_prep_entry.cpp
// @layer host
// @role prep 设备 kernel 统一 entry（注册 f203_keygen_prep）。 / Prep device entry TU.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 段 0×AIC + 2×AIV block（block0 负载更重）；CPU SUCCESS 日志中 AIC_* 为 tikicpu 仿真伪影，非 prep 真拓扑。
// @depends #include: f203_a_hat16_config.h, f203_keygen_prep_ub.hpp
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

/**
 * @file f203_keygen_prep_entry.cpp
 * @brief Alg.13 KeyGen 准备段 **单内核 / 单 TPipe**：行 3–7 Â → 行 8–15 presample V3。
 */
#include "f203_a_hat16_config.h"
#include "f203_keygen_prep_ub.hpp"

extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    (void)x_gm;
    (void)lengths_gm;
    (void)se_workspace_gm;

    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seedPtr[0];

    F203KeygenPrep::BuildKeygenPrepSinglePipe(seed_d, AscendC::GetBlockIdx(),
                                              reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                              reinterpret_cast<__gm__ int32_t *>(src_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(rho_gm), se_tiling_gm);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm)
{
    f203_keygen_prep<<<blockDim, l2ctrl, stream>>>(seed_d_gm, a_hat_gm, prf_out_gm, src_gm, rho_gm, x_gm,
                                                   lengths_gm, se_workspace_gm, se_tiling_gm);
}
#endif
