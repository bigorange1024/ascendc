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
