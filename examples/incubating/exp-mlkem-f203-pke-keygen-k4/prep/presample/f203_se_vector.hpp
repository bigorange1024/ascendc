// @probe exp-mlkem-f203-pke-keygen-k4
// @file prep/presample/f203_se_vector.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_vector.hpp` 为该子模块组件。 / Component: f203_se_vector.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_se_stage_config.hpp, f203_se_vector_prf.hpp, f203_se_vector_cbd_ub.hpp, f203_cbd_eta2.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_se_vector.hpp
 * @brief 阶段编排：G → P → C。
 *
 * V3（F203_SE_VECTOR_V3，默认）：shake UB batch PRF + alg8 P1b-single（SamplePolyCbd2Batch8）。
 * V2.5（F203_SE_VECTOR_V25，实验）：shake UB PRF + bulk UB CBD；SIM 更慢，禁止集成。
 */
#pragma once

#include "f203_se_stage_config.hpp"
#include "f203_se_vector_prf.hpp"

#if defined(F203_SE_VECTOR_V25)
#include "f203_se_vector_cbd_ub.hpp"
#else
#include "f203_cbd_eta2.hpp"
#endif

namespace F203SeVector {

__aicore__ inline void BuildSrcFromSeedD(uint32_t seed_d, GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR prf_out_gm,
                                         GM_ADDR src_gm, GM_ADDR tiling_gm)
{
    (void)x_gm;
    (void)lengths_gm;
    BuildPrfOutFromSeedD(seed_d, prf_out_gm, tiling_gm);
    AscendC::PipeBarrier<PIPE_ALL>();
#if defined(F203_SE_VECTOR_V25)
    BuildSrcFromPrfGmUb(reinterpret_cast<const __gm__ uint8_t *>(prf_out_gm),
                        reinterpret_cast<__gm__ int32_t *>(src_gm));
#else
    F203CbdEta2::SamplePolyCbd2Batch8(reinterpret_cast<const __gm__ uint8_t *>(prf_out_gm),
                                      reinterpret_cast<__gm__ int32_t *>(src_gm));
#endif
}

}  // namespace F203SeVector
