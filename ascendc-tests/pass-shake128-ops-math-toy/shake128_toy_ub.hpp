/**
 * @file shake128_toy_ub.hpp
 * @brief SHAKE128 toy 参考实现：单 TPipe、全 UB I/O、设备侧与内嵌 golden 自检（无 GM 搬运）。
 *
 * 流水线位置：被 `shake128_toy_entry.cpp` 的核函数调用，是本探针「计算 + 自检」的核心逻辑。
 * 依赖共享设备原语 `library/shared/shake_xof_kernel`（`shake_general.h`，rate=168 的
 * SHAKE128 语义）与共用 UB 辅助函数 `shake_ub_helpers.hpp`（`ShakeXofUb::*`）。
 * 用例数据（消息 x、长度 lengths、期望输出 golden_y）由 Host 侧 `gen_data.py` 依据当前
 * `SHAKE128_CASE` 环境变量生成，并通过 `emit_toy_active_case_h.py` 固化为编译期常量
 * `auto_gen/toy_active_case.h`（命名空间 `Shake128ToyActive`），本文件只负责把这些常量
 * 搬入 UB、调用共享核计算、再与内嵌 golden 逐字节比对，全程不经过 GM 上的 x/y 缓冲区。
 */
#pragma once

#include "auto_gen/toy_active_case.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include "kernel_operator.h"

namespace Shake128Toy {

/**
 * 将编译期内嵌的当前用例数据（`Shake128ToyActive::kX` / `kLengths`）写入 UB。
 * @param xUb   [out] UB 上的消息缓冲区，uint8_t，行优先布局 [batch, maxMsgLen]，
 *              即第 i 条消息占 [i*maxMsgLen, i*maxMsgLen+maxMsgLen) 字节（真实长度
 *              以 lenUb[i] 为准，多余部分为占位填充，不参与哈希）
 * @param lenUb [out] UB 上的长度缓冲区，uint32_t，长度为 batch，lenUb[b] = 第 b 条消息真实字节数
 * 前置条件：xUb/lenUb 容量须分别 >= kXBytes / kBatch*4 字节（由调用侧 InitBuffer 保证）。
 */
__aicore__ inline void FillActiveCaseUb(AscendC::LocalTensor<uint8_t> &xUb, AscendC::LocalTensor<uint32_t> &lenUb)
{
    /* 逐字节把内嵌消息常量搬进 UB（toy 规模小，标量逐元素 SetValue 足够，无需向量化） */
    for (uint32_t i = 0; i < Shake128ToyActive::kXBytes; ++i) {
        xUb.SetValue(i, Shake128ToyActive::kX[i]);
    }
    /* 逐条写入每条消息的真实长度 */
    for (uint32_t b = 0; b < Shake128ToyActive::kBatch; ++b) {
        lenUb.SetValue(b, Shake128ToyActive::kLengths[b]);
    }
}

/**
 * 将设备计算得到的 UB 输出 yUb 与内嵌 golden（`Shake128ToyActive::kGoldenY`，来自
 * Python `hashlib.shake_128` 或 tiny_sha3）逐字节比对。
 * @param yUb [in] UB 上的设备输出缓冲区，uint8_t，长度为 kYBytes = batch*outLen
 * @return 1=全部字节一致（PASS），0=存在任意字节不一致（FAIL），一旦发现即提前返回
 */
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
 * 跑当前 active 用例：UB 填消息 → 调用共享 `RunKernelShakeGeneralUb` → UB 上逐字节对拍 golden。
 * @return 1=PASS，0=FAIL（供核函数入口回写到 tiling.reserved2）
 * 前置条件：仅应在 blockIdx==0 的核上调用（由 `shake128_toy_entry.cpp` 保证）。
 */
__aicore__ inline uint32_t RunActiveCaseUb()
{
    /* 构造 tiling：batch/maxMsgLen/outLen 取自内嵌用例常量，rate 固定为 SHAKE128（168B），
     * blockDim 固定为 1（本 toy 单核语义，不做多核切分） */
    ShakeGeneralTilingData tilingHost{};
    ShakeXofUb::FillShakeTilingUb(tilingHost, Shake128ToyActive::kBatch, Shake128ToyActive::kMaxMsgLen,
                                Shake128ToyActive::kOutLen, SHAKE128_RATE_BYTES);
    tilingHost.blockDim = 1U;

    /* 单 TPipe 管理全部 UB 缓冲：x（消息）/ len（长度）/ staging（共享 SHAKE 核内部中间态）/ y（输出）。
     * CeilAlign32 保证每块 UB 起始地址按 32B 对齐（Ascend UB 访问对齐要求）；kXBytes/kYBytes 为 0 时
     * 仍申请 1 字节，避免 InitBuffer(0) 的边界问题（如空消息用例）。 */
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

    /* Step1：把内嵌用例数据填入 x/len UB，PipeAll 确保填充完成后才进入计算（UB 写后读同步点） */
    FillActiveCaseUb(xUb, lenUb);
    ShakeXofUb::PipeAll();

    /* Step2：调用共享 SHAKE128 XOF 核（全 UB 版本），staging 为其内部 Keccak 状态与分块缓冲；
     * 计算完成后 PipeAll 确保 yUb 写入对后续读取可见 */
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, stagingUb, &tilingHost);
    ShakeXofUb::PipeAll();

    /* Step3：与内嵌 golden 逐字节比对，返回自检结果 */
    return CompareYUbToGolden(yUb);
}

}  // namespace Shake128Toy
