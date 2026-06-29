/**
 * @file shake256_toy_ub.hpp
 * @brief SHAKE256 toy 参考实现：单 TPipe、全 UB I/O、设备侧 golden 自检。
 */
#pragma once

#include "auto_gen/toy_active_case.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include "kernel_operator.h"

namespace Shake256Toy {

__aicore__ inline void FillActiveCaseUb(AscendC::LocalTensor<uint8_t> &xUb, AscendC::LocalTensor<uint32_t> &lenUb)
{
    for (uint32_t i = 0; i < Shake256ToyActive::kXBytes; ++i) {
        xUb.SetValue(i, Shake256ToyActive::kX[i]);
    }
    for (uint32_t b = 0; b < Shake256ToyActive::kBatch; ++b) {
        lenUb.SetValue(b, Shake256ToyActive::kLengths[b]);
    }
}

__aicore__ inline uint32_t CompareYUbToGolden(const AscendC::LocalTensor<uint8_t> &yUb)
{
    for (uint32_t i = 0; i < Shake256ToyActive::kYBytes; ++i) {
        if (yUb.GetValue(i) != Shake256ToyActive::kGoldenY[i]) {
            return 0U;
        }
    }
    return 1U;
}

__aicore__ inline uint32_t RunActiveCaseUb()
{
    ShakeGeneralTilingData tilingHost{};
    ShakeXofUb::FillShakeTilingUb(tilingHost, Shake256ToyActive::kBatch, Shake256ToyActive::kMaxMsgLen,
                                  Shake256ToyActive::kOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = 1U;

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> yBuf;
    pipe.InitBuffer(xBuf, ShakeXofUb::CeilAlign32(Shake256ToyActive::kXBytes > 0U ? Shake256ToyActive::kXBytes : 1U));
    pipe.InitBuffer(lenBuf,
                    ShakeXofUb::CeilAlign32(Shake256ToyActive::kBatch * static_cast<uint32_t>(sizeof(uint32_t))));
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yBuf, ShakeXofUb::CeilAlign32(Shake256ToyActive::kYBytes > 0U ? Shake256ToyActive::kYBytes : 1U));

    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> yUb = yBuf.Get<uint8_t>();

    FillActiveCaseUb(xUb, lenUb);
    ShakeXofUb::PipeAll();

    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, stagingUb, &tilingHost);
    ShakeXofUb::PipeAll();

    return CompareYUbToGolden(yUb);
}

}  // namespace Shake256Toy
