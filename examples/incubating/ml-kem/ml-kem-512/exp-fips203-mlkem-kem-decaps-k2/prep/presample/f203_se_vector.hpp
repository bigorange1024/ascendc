// @probe exp-fips203-mlkem-kem-decaps-k2
// @file prep/presample/f203_se_vector.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector.hpp
 * @brief 阶段编排：G → P → C（KeyGen/presample 独立核）。
 *
 * 流水线：SEED_D → BuildPrfOutFromSeedD（含 G→σ→PRF）→ CBD×8 → src[8,256]。
 * Encrypt prep 不调用本文件；其编排在 f203_encrypt_prep_ub.hpp。
 *
 * V3（F203_SE_VECTOR_V3，默认）：shake UB batch PRF + alg8 P1b-single（polyvec6 batch helper）。
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

/**
 * SEED_D → prf_out + src（x_gm/lengths_gm 历史占位，当前未用）。
 * @param tiling_gm host 填的 SHAKE batch tiling
 */
__aicore__ inline void BuildSrcFromSeedD(uint32_t seed_d, GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR prf_out_gm,
                                         GM_ADDR src_gm, GM_ADDR tiling_gm)
{
    (void)x_gm;
    (void)lengths_gm;
    // Phase P：σ→PRF×8
    BuildPrfOutFromSeedD(seed_d, prf_out_gm, tiling_gm);
    AscendC::PipeBarrier<PIPE_ALL>();
#if defined(F203_SE_VECTOR_V25)
    // 实验：bulk UB CBD
    BuildSrcFromPrfGmUb(reinterpret_cast<const __gm__ uint8_t *>(prf_out_gm),
                        reinterpret_cast<__gm__ int32_t *>(src_gm));
#else
    // 生产：alg8 逐行 DataCopy + SWAR+LUT
    F203CbdEta2::SamplePolyCbd2Batch8(reinterpret_cast<const __gm__ uint8_t *>(prf_out_gm),
                                      reinterpret_cast<__gm__ int32_t *>(src_gm));
#endif
}

}  // namespace F203SeVector
