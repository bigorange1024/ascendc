/**
 * @file cbd_l1_ub.hpp
 * @brief E08 L1：真 CBD(η=2) 单 poly 积木（DataCopy + SWAR+LUT）。
 *
 * 流水线位置：L1 真 SHAKE256 之后、返回 Host μ 之前；输出写 GM `src`，供 L2 NTT 消费。
 * 积木来自本目录 `vendor/cbd_eta2/`（自包含拷贝自 pass-fix-f203-alg8-cbd-eta2-k4）。
 *
 * 数据流：prf_gm[128] uint8 → SamplePolyCbd2OneRowUb(row=0) → src_gm[256] int32。
 * 背景：D-exp-e08 — 接真 CBD，非 TRACE stub；短链单 poly（非整图 8 行 batch）。
 * 未采用：抄 Encrypt；改原探针；P2 blockDim=2（本壳 MIX blockDim=1）。
 */
#pragma once

#include "f203_cbd_eta2.hpp"

#include "kernel_operator.h"

namespace CbdL1Toy {

/** η=2 单 poly：PRF 128B → 256 系数。 */
constexpr uint32_t kPrfBytes = F203CbdEta2::PRF_BYTES;
constexpr uint32_t kN = F203CbdEta2::N;

/**
 * 跑真 CBD(η=2) 单 poly：GM prf → UB → SWAR+LUT → GM src。
 * @param prfGm [in]  PRF 输出，长度 128B（Host 预填；玩具短链）
 * @param srcGm [out] CBD 采样结果 int32[256]（覆盖 L2 的 NTT 输入）
 * 前置：仅 AIV 调用；独立 TPipe（与 SHAKE 作用域分离，避免叠 EventID）。
 */
__aicore__ inline void RunCbdEta2OnePoly(__gm__ const uint8_t *prfGm, __gm__ int32_t *srcGm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, kPrfBytes);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfG;
    AscendC::GlobalTensor<int32_t> srcG;
    prfG.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBytes);
    srcG.SetGlobalBuffer(srcGm, kN);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    // row=0：短链单 poly；复用探针 OneRowUb 流水（CopyIn→SWAR+LUT→CopyOut）
    F203CbdEta2::SamplePolyCbd2OneRowUb(0U, prfG, srcG, prfLocal, rowQue);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace CbdL1Toy
