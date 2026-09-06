/**
 * @file cbd_l1_ub.hpp
 * @brief E13 L1 采样：真 CBD(η=2) u×2 + v×1（Encrypt 噪声角色）。
 */
#pragma once

#include "f203_cbd_eta2.hpp"

#include "kernel_operator.h"

namespace CbdL1Toy {

constexpr uint32_t kPrfBytesPerPoly = F203CbdEta2::PRF_BYTES;
constexpr uint32_t kN = F203CbdEta2::N;
constexpr uint32_t kUPolys = 2U;
constexpr uint32_t kVPrfRow = kUPolys;

/**
 * u 路：prf[0:256B] → src[512] int32（r0∥r1）。
 */
__aicore__ inline void RunCbdEta2UPolys(__gm__ const uint8_t *prfGm, __gm__ int32_t *srcGm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, kPrfBytesPerPoly);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfG;
    AscendC::GlobalTensor<int32_t> srcG;
    prfG.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBytesPerPoly * kUPolys);
    srcG.SetGlobalBuffer(srcGm, static_cast<uint32_t>(kN) * kUPolys);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    for (uint32_t row = 0; row < kUPolys; ++row) {
        F203CbdEta2::SamplePolyCbd2OneRowUb(row, prfG, srcG, prfLocal, rowQue);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * v 路：prf 第 3 行 → e2[256] int32。
 * 背景：SamplePolyCbd2OneRowUb 用同一 row 索引 prf/src；v 输出在独立 GM 区，须 prfRow≠outRow。
 */
__aicore__ inline void RunCbdEta2VPoly(__gm__ const uint8_t *prfGm, __gm__ int32_t *e2Gm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, kPrfBytesPerPoly);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfG;
    AscendC::GlobalTensor<int32_t> e2G;
    prfG.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBytesPerPoly * (kUPolys + 1U));
    e2G.SetGlobalBuffer(e2Gm, static_cast<uint32_t>(kN));

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    AscendC::DataCopy(prfLocal, prfG[kVPrfRow * kPrfBytesPerPoly], kPrfBytesPerPoly);
    AscendC::PipeBarrier<PIPE_MTE2>();

    AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
    F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
    AscendC::PipeBarrier<PIPE_V>();

    rowQue.EnQue(rowLocal);
    rowLocal = rowQue.DeQue<int32_t>();
    AscendC::DataCopy(e2G, rowLocal, static_cast<uint32_t>(kN));
    rowQue.FreeTensor(rowLocal);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace CbdL1Toy
