
/** 独立探针入口 f203_se_vector_k4；全链 KeyGen 走 f203_keygen_prep，本文件供分段验证。 */
// @probe pass-fix-f203-alg13-device-keygen-k3
// @file prep/presample/f203_se_vector_entry.cpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_vector_entry.cpp` 为该子模块组件。 / Component: f203_se_vector_entry.cpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_se_vector.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 行 8–15 PRF+CBD presample 链。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/presample/f203_se_vector_entry.cpp
 */
/**
 * @file f203_se_vector_entry.cpp
 * @brief SEED_D → Phase G + PRF + CBD → src[6,256]（单 AIV，blockDim=1）。
 * 阶段宏见 f203_se_stage_config.hpp（默认 V3）。
 */
#include "f203_se_vector.hpp"

extern "C" __global__ __aicore__ void f203_se_vector_k4(GM_ADDR seed_d_gm, GM_ADDR prf_out_gm, GM_ADDR src_gm,
                                                        GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR workspace,
                                                        GM_ADDR tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    // 独立探针：仅 block0；全链 KeyGen 不走本入口

    (void)workspace;
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    const __gm__ uint32_t *seed_ptr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seed_ptr[0];

    F203SeVector::BuildSrcFromSeedD(seed_d, x_gm, lengths_gm, prf_out_gm, src_gm, tiling);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_se_vector_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *x_gm, uint8_t *lengths_gm,
                                     uint8_t *workspace, uint8_t *tiling)
{
    f203_se_vector_k4<<<blockDim, l2ctrl, stream>>>(seed_d_gm, prf_out_gm, src_gm, x_gm, lengths_gm, workspace,
                                                    tiling);
}
#endif
