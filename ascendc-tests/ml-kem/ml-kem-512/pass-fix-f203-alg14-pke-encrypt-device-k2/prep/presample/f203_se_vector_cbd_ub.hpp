// @probe pass-fix-f203-alg14-pke-encrypt-device-k2
// @file prep/presample/f203_se_vector_cbd_ub.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector_cbd_ub.hpp
 * @brief V2.5 实验 Phase C：一次 bulk GM→UB DataCopy；CBD 无逐行 prf GM 读（更慢，不接入集成）。
 *
 * 流水线位置：仅 F203_SE_VECTOR_V25 实验对照；生产默认 V3 走 alg8 SamplePolyCbd2Batch8。
 * Encrypt prep 不调用本文件。
 *
 * 与 golden：I/O 仍应对齐 sample_poly_cbd2；本路径 SIM tick 更差，禁止作默认。
 */
#pragma once

#include "f203_cbd_eta2.hpp"

#include "kernel_operator.h"

namespace F203SeVector {

#define F203_SE_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/**
 * 从 bulk prfAllUb 抽出第 row 行 128B 到 prfRowLocal（标量 SetValue，实验路径）。
 */
__aicore__ inline void FillPrfRowFromUb(AscendC::LocalTensor<uint8_t> &prfRowLocal,
                                        const AscendC::LocalTensor<uint8_t> &prfAllUb, uint32_t row)
{
    const uint32_t base = row * F203CbdEta2::PRF_BYTES;
    for (uint32_t b = 0; b < F203CbdEta2::PRF_BYTES; ++b) {
        prfRowLocal.SetValue(b, prfAllUb.GetValue(base + b));
    }
}

/**
 * 单 TPipe：bulk prf GM→UB + 8 行 SWAR+LUT + src DataCopy。
 * @param prf_out_gm [8,128]；@param src_gm [8,256]
 */
__aicore__ inline void BuildSrcFromPrfGmUb(__gm__ const uint8_t *prf_out_gm, __gm__ int32_t *src_gm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> prfAllQue;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> prfRowQue;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    pipe.InitBuffer(prfAllQue, 1, F203CbdEta2::PRF_TOTAL_BYTES);
    pipe.InitBuffer(prfRowQue, 1, F203CbdEta2::PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(F203CbdEta2::N) * sizeof(int32_t));

    // 一次搬入全部 8×128B PRF
    AscendC::LocalTensor<uint8_t> prfAllUb = prfAllQue.AllocTensor<uint8_t>();
    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_out_gm), F203CbdEta2::PRF_TOTAL_BYTES);
    AscendC::DataCopy(prfAllUb, prfGm, F203CbdEta2::PRF_TOTAL_BYTES);
    F203_SE_PIPE_ALL();

    AscendC::GlobalTensor<int32_t> srcGm;
    srcGm.SetGlobalBuffer(src_gm, F203CbdEta2::SRC_COEFFS);

    for (uint32_t row = 0; row < F203CbdEta2::ROWS; ++row) {
        // 从 bulk UB 切出一行再 CBD（切行本身有开销，故实验更慢）
        AscendC::LocalTensor<uint8_t> prfRowLocal = prfRowQue.AllocTensor<uint8_t>();
        FillPrfRowFromUb(prfRowLocal, prfAllUb, row);
        F203_SE_PIPE_ALL();
        prfRowQue.EnQue(prfRowLocal);
        prfRowLocal = prfRowQue.DeQue<uint8_t>();

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfRowLocal);
        prfRowQue.FreeTensor(prfRowLocal);
        F203_SE_PIPE_ALL();

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(srcGm[row * F203CbdEta2::N], rowLocal, F203CbdEta2::N);
        F203_SE_PIPE_ALL();
        rowQue.FreeTensor(rowLocal);
    }

    prfAllQue.FreeTensor(prfAllUb);
}

}  // namespace F203SeVector
