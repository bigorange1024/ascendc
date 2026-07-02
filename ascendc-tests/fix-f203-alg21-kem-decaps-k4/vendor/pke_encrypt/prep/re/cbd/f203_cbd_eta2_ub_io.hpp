/**
 * @file f203_cbd_eta2_ub_io.hpp
 * @brief P1b/P2：GM↔UB MTE DataCopy + PipeBarrier。
 *
 * 每行流水线（8 行或 P2 下每 AIV 4 行）：
 *   1. DataCopy 128B  prf_gm[row] → prfLocal
 *   2. PipeBarrier    — MTE 完成后再读 UB（无 barrier 时 SIM ~23k 但对拍 FAIL）
 *   3. SamplePolyCbd2RowSwLutUb
 *   4. PipeBarrier    — 计算完成后再写 GM
 *   5. DataCopy 256×int32 rowLocal → src_gm[row]
 *   6. PipeBarrier
 *
 * P1b：`F203_CBD_BLOCK_DIM==1`，block0 串行 8 行。
 * P2：`F203_CBD_BLOCK_DIM==2`，RowForBlock 双 AIV 分片（SIM/NPU blockDim=2）。
 * P2 + CPU 孪生：`ICPU_RUN_KF` 固定 blockDim=1（910B 每 block 会误起 1 AIC+2 AIV），
 *   内核见 `GetBlockNum()==1` 时 block0 串行 8 行，语义与 P2 对拍一致。
 *
 * Pipe 细同步（Opt-5 Phase 1+5）：CopyIn→PIPE_MTE2，Vector 后→PIPE_V；CopyOut 后 barrier 已删减（C-04）。
 */
#pragma once

#include "f203_cbd_eta2_sw_lut.hpp"

#include "kernel_operator.h"

namespace F203CbdEta2 {

#define F203_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

__aicore__ inline void SamplePolyCbd2OneRowUb(uint32_t row, AscendC::GlobalTensor<uint8_t> &prfGm,
                                              AscendC::GlobalTensor<int32_t> &srcGm,
                                              AscendC::LocalTensor<uint8_t> &prfLocal,
                                              AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
    F203_CBD_SYNC_AFTER_COPYIN();

    AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
    SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
    F203_CBD_SYNC_BEFORE_COPYOUT();

    rowQue.EnQue(rowLocal);
    rowLocal = rowQue.DeQue<int32_t>();
    AscendC::DataCopy(srcGm[row * N], rowLocal, N);
    rowQue.FreeTensor(rowLocal);
}

__aicore__ inline void SamplePolyCbd2Batch8WithUb(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                   __gm__ int32_t *src_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> srcGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), PRF_TOTAL_BYTES);
    srcGm.SetGlobalBuffer(src_gm, SRC_COEFFS);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

#if F203_CBD_BLOCK_DIM == 1
    for (uint32_t row = 0; row < ROWS; ++row) {
        SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
    }
#else
    if (AscendC::GetBlockNum() == 1U) {
        if (blockIdx != 0U) {
            return;
        }
        for (uint32_t row = 0; row < ROWS; ++row) {
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    } else {
        for (uint32_t i = 0; i < ROWS_PER_AIV; ++i) {
            const uint32_t row = RowForBlock(blockIdx, i);
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    }
#endif
}

__aicore__ inline void SamplePolyCbd2Batch8DataCopy(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                    __gm__ int32_t *src_gm)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(N) * sizeof(int32_t));
    SamplePolyCbd2Batch8WithUb(blockIdx, prf_gm, src_gm, scratchBuf, rowQue);
}

}  // namespace F203CbdEta2
