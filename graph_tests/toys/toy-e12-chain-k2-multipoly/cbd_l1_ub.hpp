/**
 * @file cbd_l1_ub.hpp
 * @brief E12 L1：真 CBD(η=2) k=2（两 poly，单 TPipe + row 索引）。
 */
#pragma once

#include "f203_cbd_eta2.hpp"

#include "kernel_operator.h"

namespace CbdL1Toy {

constexpr uint32_t kPrfBytesPerPoly = F203CbdEta2::PRF_BYTES;
constexpr uint32_t kN = F203CbdEta2::N;
constexpr uint32_t kPolys = 2U;

/**
 * k=2：对 prf[2,128B] 各跑 CBD → src[2,256] int32（row 索引，非指针偏移）。
 */
__aicore__ inline void RunCbdEta2K2Polys(__gm__ const uint8_t *prfGm, __gm__ int32_t *srcGm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, kPrfBytesPerPoly);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfG;
    AscendC::GlobalTensor<int32_t> srcG;
    prfG.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBytesPerPoly * kPolys);
    srcG.SetGlobalBuffer(srcGm, static_cast<uint32_t>(kN) * kPolys);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    for (uint32_t row = 0; row < kPolys; ++row) {
        F203CbdEta2::SamplePolyCbd2OneRowUb(row, prfG, srcG, prfLocal, rowQue);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace CbdL1Toy
