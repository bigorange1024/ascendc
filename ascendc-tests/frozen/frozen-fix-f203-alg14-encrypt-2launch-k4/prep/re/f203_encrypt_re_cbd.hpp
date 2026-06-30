/**
 * @file f203_encrypt_re_cbd.hpp
 * @brief Alg.14 G1 Phase C：prf_out[9,128] → r/e1/e2 扁平 GM（η₁=η₂=2，P1b 串行 9 行）。
 */
#pragma once

#include "f203_cbd_eta2.hpp"
#include "f203_cbd_eta2_sw_lut.hpp"
#include "f203_encrypt_re_layout.h"

#include "kernel_operator.h"

namespace F203EncryptRe {

#define F203_ENCRYPT_RE_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_ENCRYPT_RE_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

__aicore__ inline void SampleCbdOneRowUb(uint32_t prfRow, AscendC::GlobalTensor<uint8_t> &prfGm,
                                         AscendC::GlobalTensor<int32_t> &reGm,
                                         AscendC::LocalTensor<uint8_t> &prfLocal,
                                         AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    AscendC::DataCopy(prfLocal, prfGm[prfRow * kPrfOutLen], kPrfOutLen);
    F203_ENCRYPT_RE_CBD_SYNC_AFTER_COPYIN();

    AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
    F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
    F203_ENCRYPT_RE_CBD_SYNC_BEFORE_COPYOUT();

    rowQue.EnQue(rowLocal);
    rowLocal = rowQue.DeQue<int32_t>();
    const uint32_t dstOff = PrfRowToReOffsetCoeffs(prfRow);
    AscendC::DataCopy(reGm[dstOff], rowLocal, kKyberN);
    rowQue.FreeTensor(rowLocal);
}

/** 9 行 CBD：blockDim=1，block0 串行（与 KeyGen se_vector V3 P1b 一致）。 */
__aicore__ inline void SampleCbdBatch9FromPrfGm(__gm__ const uint8_t *prf_gm, __gm__ int32_t *re_gm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, kPrfOutLen);
    pipe.InitBuffer(rowQue, 1, kKyberN * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> reGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), kPrfTotalBytes);
    reGm.SetGlobalBuffer(re_gm, kReTotalBytes / sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

    for (uint32_t row = 0U; row < kEncryptCbdPolys; ++row) {
        SampleCbdOneRowUb(row, prfGm, reGm, prfLocal, rowQue);
    }
}

}  // namespace F203EncryptRe
