/**
 * @file shake_ub_helpers.hpp
 * @brief SHAKE XOF 设备 UB 胶水：对齐、块写消息、统一 RunKernelShakeGeneralUb。
 *
 * 所有消费 shake_xof_kernel 的探针/toy 应经本头文件接线，避免 GM x/y 与旧 Init API。
 *
 * ## 内嵌 vs 独立 launch（必读）
 * - `RunKernelShakeGeneralUb` 调用 `KernelShakeGeneral::ProcessInline()`：
 *   当前 AIV 独占 UB 上整批 batch，**不**使用外层 `GetBlockIdx()` 做分片。
 * - 背景：KeyGen Opt-4 外层 `blockDim=2` 时，若内嵌 SHAKE 沿用 `GetBlockIdx()==1`，
 *   `batch=1` 的 Process() 循环空转 → poly 8–15 未写入（见 docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md）。
 * - 独立多核 SHAKE 设备核（未来大 batch launch）应直接调用 `op.Process()`。
 */
#pragma once

#include "shake_general.h"
#include "shake_general_tiling_data.h"

#include "kernel_operator.h"

namespace ShakeXofUb {

__aicore__ inline void PipeAll()
{
    AscendC::PipeBarrier<PIPE_ALL>();
}


constexpr uint32_t CeilAlign32(uint32_t n)
{
    return (n + 31U) & ~31U;
}

__aicore__ inline uint32_t GcdU32(uint32_t a, uint32_t b)
{
    while (b != 0U) {
        const uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

__aicore__ inline uint32_t CeilDivU32(uint32_t a, uint32_t b)
{
    return (a + b - 1U) / b;
}

/** 与 tiling_host.hpp FillShakeTiling 一致（设备可调用）。 */
__aicore__ inline void FillShakeTilingUb(ShakeGeneralTilingData &tiling, uint32_t batch, uint32_t maxMsgLen,
                                         uint32_t outLen, uint32_t rate)
{
    constexpr uint32_t kCacheLineBytes = 64U;
    const uint32_t groupSize = kCacheLineBytes / GcdU32(outLen, kCacheLineBytes);
    const uint32_t groupCount = CeilDivU32(batch, groupSize);
    constexpr uint32_t kCoreNum = 20U;
    const uint32_t blockDim = groupCount < kCoreNum ? groupCount : kCoreNum;

    tiling.batch = batch;
    tiling.maxMsgLen = maxMsgLen;
    tiling.outLen = outLen;
    tiling.rate = rate;
    tiling.blockDim = blockDim;
    tiling.groupSize = groupSize;
    tiling.reserved0 = 0U;
    tiling.reserved1 = 0U;
    tiling.reserved2 = 0U;
}

/** 小端拼 1..8 字节为 uint64。 */
__aicore__ inline uint64_t PackLeBytesU64(const uint8_t *bytes, uint32_t n)
{
    uint64_t w = 0;
    for (uint32_t i = 0; i < n; ++i) {
        w |= static_cast<uint64_t>(bytes[i]) << (8U * i);
    }
    return w;
}

/** 写单行消息 prefix[prefixLen]||tailByte 到 x_ub[rowBase..]。 */
__aicore__ inline void FillShakeRowUb(const uint8_t *prefix, uint32_t prefixLen, uint8_t tailByte,
                                      AscendC::LocalTensor<uint8_t> &xUb, uint32_t rowBase)
{
    const uint32_t fullWords = prefixLen / 8U;
    AscendC::LocalTensor<uint64_t> row64 = xUb[rowBase].ReinterpretCast<uint64_t>();
    for (uint32_t word = 0; word < fullWords; ++word) {
        row64.SetValue(word, PackLeBytesU64(prefix + word * 8U, 8U));
    }
    const uint32_t tailStart = fullWords * 8U;
    if (tailStart < prefixLen) {
        const uint32_t rem = prefixLen - tailStart;
        row64.SetValue(fullWords, PackLeBytesU64(prefix + tailStart, rem));
    }
    xUb.SetValue(rowBase + prefixLen, tailByte);
}

/** 统一调用 KernelShakeGeneral（须 staging32 ≥ SHAKE_XOF_STAGING_BYTES）。内嵌路径：ProcessInline。 */
__aicore__ inline void RunKernelShakeGeneralUb(AscendC::LocalTensor<uint8_t> &xUb,
                                               AscendC::LocalTensor<uint32_t> &lengthsUb,
                                               AscendC::LocalTensor<uint8_t> &yUb,
                                               AscendC::LocalTensor<uint8_t> &staging32,
                                               const ShakeGeneralTilingData *tiling)
{
    ShakeXofKernel::KernelShakeGeneral op;
    op.Init(xUb, lengthsUb, yUb, staging32, tiling);
    op.ProcessInline();
}

}  // namespace ShakeXofUb
