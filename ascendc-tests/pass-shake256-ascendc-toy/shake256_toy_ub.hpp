/**
 * @file shake256_toy_ub.hpp
 * @brief SHAKE256 toy 参考实现：单 TPipe、全 UB I/O、设备侧 golden 自检。
 *
 * 流水线位置：被 `shake256_toy_entry.cpp` 的核函数调用，是本探针「计算 + 自检」的核心逻辑。
 * 依赖共享设备原语 `library/shared/shake_xof_kernel`（`shake_general.h`，rate=136 的
 * SHAKE256 语义）与共用 UB 辅助函数 `shake_ub_helpers.hpp`（`ShakeXofUb::*`）。
 * 用例数据由 Host 侧 `gen_data.py` 依据当前 `SHAKE256_CASE` 环境变量生成，并固化为
 * 编译期常量 `auto_gen/toy_active_case.h`（命名空间 `Shake256ToyActive`）。本文件与
 * `pass-shake128-ops-math-toy/shake128_toy_ub.hpp` 结构完全对称，唯一区别是共享核内部
 * 的 rate 参数（SHAKE256_RATE_BYTES=136，对应 capacity=512bit，FIPS 规范轨）。
 */
#pragma once

#include "auto_gen/toy_active_case.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include "kernel_operator.h"

namespace Shake256Toy {

/**
 * 将编译期内嵌的当前用例数据（`Shake256ToyActive::kX` / `kLengths`）写入 UB。
 * @param xUb   [out] UB 消息缓冲区，uint8_t，行优先布局 [batch, maxMsgLen]
 * @param lenUb [out] UB 长度缓冲区，uint32_t，长度为 batch
 */
__aicore__ inline void FillActiveCaseUb(AscendC::LocalTensor<uint8_t> &xUb, AscendC::LocalTensor<uint32_t> &lenUb)
{
    /* 逐字节把内嵌消息常量搬进 UB（toy 规模小，标量逐元素 SetValue 足够） */
    for (uint32_t i = 0; i < Shake256ToyActive::kXBytes; ++i) {
        xUb.SetValue(i, Shake256ToyActive::kX[i]);
    }
    for (uint32_t b = 0; b < Shake256ToyActive::kBatch; ++b) {
        lenUb.SetValue(b, Shake256ToyActive::kLengths[b]);
    }
}

/**
 * 将设备计算得到的 UB 输出 yUb 与内嵌 golden（`Shake256ToyActive::kGoldenY`）逐字节比对。
 * @param yUb [in] UB 设备输出缓冲区，uint8_t，长度为 kYBytes = batch*outLen
 * @return 1=全部字节一致（PASS），0=存在不一致（FAIL），发现即提前返回
 */
__aicore__ inline uint32_t CompareYUbToGolden(const AscendC::LocalTensor<uint8_t> &yUb)
{
    for (uint32_t i = 0; i < Shake256ToyActive::kYBytes; ++i) {
        if (yUb.GetValue(i) != Shake256ToyActive::kGoldenY[i]) {
            return 0U;
        }
    }
    return 1U;
}

/**
 * 跑当前 active 用例：UB 填消息 → 调用共享 `RunKernelShakeGeneralUb` → UB 上逐字节对拍 golden。
 * @return 1=PASS，0=FAIL（供核函数入口回写到 tiling.reserved2）
 * 前置条件：仅应在 blockIdx==0 的核上调用。
 */
__aicore__ inline uint32_t RunActiveCaseUb()
{
    /* 构造 tiling：rate 固定为 SHAKE256（136B），blockDim 固定 1（单核 toy） */
    ShakeGeneralTilingData tilingHost{};
    ShakeXofUb::FillShakeTilingUb(tilingHost, Shake256ToyActive::kBatch, Shake256ToyActive::kMaxMsgLen,
                                  Shake256ToyActive::kOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = 1U;

    /* 单 TPipe 管理全部 UB 缓冲：x（消息）/ len（长度）/ staging（共享核内部中间态）/ y（输出）。
     * CeilAlign32 保证 32B 对齐；kXBytes/kYBytes 为 0 时仍申请 1 字节，避免空消息用例边界问题。 */
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

    /* Step1：填入 x/len UB，PipeAll 确保填充完成后才进入计算 */
    FillActiveCaseUb(xUb, lenUb);
    ShakeXofUb::PipeAll();

    /* Step2：调用共享 SHAKE256 XOF 核（全 UB 版本），计算完成后 PipeAll 确保 yUb 写入可见 */
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, stagingUb, &tilingHost);
    ShakeXofUb::PipeAll();

    /* Step3：与内嵌 golden 逐字节比对，返回自检结果 */
    return CompareYUbToGolden(yUb);
}

}  // namespace Shake256Toy
