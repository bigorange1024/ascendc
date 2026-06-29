/**
 * @file shake128_toy_ub.hpp
 * @brief SHAKE128 toy 参考实现：单 TPipe、全 UB I/O、设备侧与内嵌 golden 自检（无 GM 搬运）。
 *
 * 用例数据由 gen_data.py → auto_gen/toy_active_case.h（随 SHAKE128_CASE 重建）。
 */
#pragma once

#include "auto_gen/toy_active_case.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include "kernel_operator.h"

namespace Shake128Toy {

/** 将内嵌 kX/kLengths 写入 UB（行优先 [batch, maxMsgLen]）。 */
__aicore__ inline void FillActiveCaseUb(AscendC::LocalTensor<uint8_t> &xUb, AscendC::LocalTensor<uint32_t> &lenUb)
{
    for (uint32_t i = 0; i < Shake128ToyActive::kXBytes; ++i) {
        xUb.SetValue(i, Shake128ToyActive::kX[i]);
    }
    for (uint32_t b = 0; b < Shake128ToyActive::kBatch; ++b) {
        lenUb.SetValue(b, Shake128ToyActive::kLengths[b]);
    }
}

__aicore__ inline uint32_t CompareYUbToGolden(const AscendC::LocalTensor<uint8_t> &yUb)
{
    for (uint32_t i = 0; i < Shake128ToyActive::kYBytes; ++i) {
        if (yUb.GetValue(i) != Shake128ToyActive::kGoldenY[i]) {
            return 0U;
        }
    }
    return 1U;
}

/**
 * 跑当前 active 用例：UB 填消息 → KernelShakeGeneral → UB 上逐字节对拍 golden。
 * @return 1=PASS，0=FAIL
 */
__aicore__ inline uint32_t RunActiveCaseUb()
{
    ShakeGeneralTilingData tilingHost{};
    ShakeXofUb::FillShakeTilingUb(tilingHost, Shake128ToyActive::kBatch, Shake128ToyActive::kMaxMsgLen,
                                Shake128ToyActive::kOutLen, SHAKE128_RATE_BYTES);
    tilingHost.blockDim = 1U;

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> yBuf;
    pipe.InitBuffer(xBuf, ShakeXofUb::CeilAlign32(Shake128ToyActive::kXBytes > 0U ? Shake128ToyActive::kXBytes : 1U));
    pipe.InitBuffer(lenBuf,
                    ShakeXofUb::CeilAlign32(Shake128ToyActive::kBatch * static_cast<uint32_t>(sizeof(uint32_t))));
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yBuf, ShakeXofUb::CeilAlign32(Shake128ToyActive::kYBytes > 0U ? Shake128ToyActive::kYBytes : 1U));

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

}  // namespace Shake128Toy
