// @probe pass-fix-f203-alg14-lines3-15-encrypt-prep-k4
// @file prep/presample/f203_se_vector_entry.cpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector_entry.cpp
 * @brief SEED_D → Phase G + PRF + CBD → src[8,256]（单 AIV，blockDim=1）。
 *
 * 流水线位置：KeyGen/presample 独立探针入口；Encrypt prep 使用 f203_encrypt_prep_entry.cpp。
 * 阶段宏见 f203_se_stage_config.hpp（默认 V3）。
 * 与 golden：presample 探针的 prf_out / src 对拍（本 Encrypt prep 用例不 launch 本核）。
 */
#include "f203_se_vector.hpp"

/**
 * 设备核：仅 block0；读 seed_d[0] 后跑 BuildSrcFromSeedD。
 */
extern "C" __global__ __aicore__ void f203_se_vector_k4(GM_ADDR seed_d_gm, GM_ADDR prf_out_gm, GM_ADDR src_gm,
                                                        GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR workspace,
                                                        GM_ADDR tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    (void)workspace;
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    const __gm__ uint32_t *seed_ptr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seed_ptr[0];

    F203SeVector::BuildSrcFromSeedD(seed_d, x_gm, lengths_gm, prf_out_gm, src_gm, tiling);
}

#ifndef __CCE_KT_TEST__
/** Host launch 包装。 */
extern "C" void f203_se_vector_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *x_gm, uint8_t *lengths_gm,
                                     uint8_t *workspace, uint8_t *tiling)
{
    f203_se_vector_k4<<<blockDim, l2ctrl, stream>>>(seed_d_gm, prf_out_gm, src_gm, x_gm, lengths_gm, workspace,
                                                    tiling);
}
#endif
