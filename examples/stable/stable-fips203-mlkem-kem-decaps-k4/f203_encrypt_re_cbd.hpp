/**
 * @file f203_encrypt_re_cbd.hpp
 * @brief Alg.14：PRF batch9 → CBD_η=2 batch9 → re GM（r‖e₁‖e₂ 连续 9 行）。
 *
 * 流水线位置：Encrypt prep 末段（仅 block0）；FIPS 203 / ML-KEM-1024。
 * 复用 prep/alg8 SWAR+LUT 单行 CBD；串行 9 行（F203_CBD_BLOCK_DIM=1）。
 * 与 golden：re 为设备中间态，不落盘；语义对齐 host `build_re`。
 */
#pragma once

#include "f203_cbd_eta2.hpp"
#include "f203_encrypt_prep_layout.h"

#include "kernel_operator.h"

namespace F203EncryptReCbd {

constexpr uint32_t N = F203EncryptPrep::kKyberN;
constexpr uint32_t PRF_BYTES = F203EncryptPrep::kPrfBytesPerPoly;
constexpr uint32_t RE_ROWS = F203EncryptPrep::kRePolys;

#define F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

/**
 * 9 行 CBD：row 0..3→r，4..7→e₁，8→e₂；re GM 行主序 int32[9,256]。
 *
 * @param prf_gm  PRF 输出 [9,128] uint8
 * @param re_gm   输出 [9,256] int32 扁平
 */
__aicore__ inline void SamplePolyCbd2Batch9WithUb(__gm__ const uint8_t *prf_gm, __gm__ int32_t *re_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> reOutGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), RE_ROWS * PRF_BYTES);
    reOutGm.SetGlobalBuffer(re_gm, RE_ROWS * N);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

    // 逐行：GM PRF[128] → UB → CBD → GM re[256] int32
    for (uint32_t row = 0U; row < RE_ROWS; ++row) {
        AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
        F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN();  // 等 MTE2 完成再算

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
        F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT();  // 等向量 CBD 完成再写回

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(reOutGm[row * N], rowLocal, N);
        rowQue.FreeTensor(rowLocal);
    }
}

}  // namespace F203EncryptReCbd
