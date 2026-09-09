/**
 * @file cbd_device.hpp
 * @brief RB-T17 设备侧 CBD 薄壳：读 Host coins → SHAKE256 PRF(N=0..8) → Alg.8 η=2 → y‖e1‖e2。
 *
 * 流水线位置：Launch2 MIX AIV0，在 flag 1/3 握手之后、SampleNTT/NTT 之前。
 *
 * 积木接线（**不**大段抄探针 / T16 实现体）：
 *   - `library/shared/shake_xof_kernel`：`ShakeXofUb::*`
 *   - `-I` 活跃 `pass-fix-f203-alg8-cbd-eta2-k4`：`SamplePolyCbd2OneRowUb`
 *
 * 契约对齐 T16/T07：nonce y=0..3 / e1=4..7 / e2=8；Encrypt 无 Phase G。
 */
#ifndef RB_T17_CBD_DEVICE_HPP
#define RB_T17_CBD_DEVICE_HPP

#ifndef F203_CBD_BLOCK_DIM
#define F203_CBD_BLOCK_DIM 1
#endif

#include "f203_cbd_eta2.hpp"
#include "kernel_operator.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"
#include "tiling.h"

#include <cstdint>

namespace reenc_cbd {

constexpr uint32_t kPrfMsgLen = 33U;
constexpr uint32_t kPrfMsgStride = ShakeXofUb::CeilAlign32(kPrfMsgLen); // 64
constexpr uint32_t kPrfBatch = 9U;
constexpr uint32_t kPrfOutLen = 128U;
constexpr uint32_t kPrfXUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfMsgStride);
constexpr uint32_t kPrfLenUbBytes =
    ShakeXofUb::CeilAlign32(kPrfBatch * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t kPrfYUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfOutLen);

/**
 * Phase P：coins → prf_out[9,128]（SHAKE256，nonce 0..8）。
 * @param coins     Host 预喂 32B（寄存器副本）
 * @param prfOutGm  workspace OFF_PRF
 */
__aicore__ inline void RunPrf9FromCoins(const uint8_t coins[32], __gm__ uint8_t *prfOutGm)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> yQue;
    pipe.InitBuffer(xBuf, kPrfXUbBytes);
    pipe.InitBuffer(lenBuf, kPrfLenUbBytes);
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yQue, 1, kPrfYUbBytes);

    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();

    for (uint32_t nonce = 0U; nonce < kPrfBatch; ++nonce) {
        const uint32_t rowBase = nonce * kPrfMsgStride;
        ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, rowBase);
        lengthsUb.SetValue(nonce, kPrfMsgLen);
    }
    ShakeXofUb::PipeAll();

    ShakeGeneralTilingData td{};
    ShakeXofUb::FillShakeTilingUb(td, kPrfBatch, kPrfMsgStride, kPrfOutLen, SHAKE256_RATE_BYTES);
    td.blockDim = 1U;

    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &td);
    ShakeXofUb::PipeAll();

    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prfOutGm, kPrfBatch * kPrfOutLen);
    AscendC::DataCopy(prfGm, yUb, kPrfBatch * kPrfOutLen);
    ShakeXofUb::PipeAll();
    yQue.FreeTensor(yUb);
}

/**
 * Phase C：prf_out[9,128] → yee[9,256] int32（逐行 OneRowUb）。
 */
__aicore__ inline void RunCbd9FromPrf(__gm__ const uint8_t *prfGm, __gm__ int32_t *srcGm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, F203CbdEta2::PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(F203CbdEta2::N) * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> prfTensor;
    AscendC::GlobalTensor<int32_t> srcTensor;
    prfTensor.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBatch * F203CbdEta2::PRF_BYTES);
    srcTensor.SetGlobalBuffer(srcGm, kPrfBatch * F203CbdEta2::N);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    for (uint32_t row = 0U; row < kPrfBatch; ++row) {
        F203CbdEta2::SamplePolyCbd2OneRowUb(row, prfTensor, srcTensor, prfLocal, rowQue);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0：从 ws 读 coins，PRF+CBD，写 OFF_Y_E1_E2。
 * @param ws 共享 workspace（OFF_COINS / OFF_PRF / OFF_Y_E1_E2）
 * 前置：仅 AIV0；`F203_CBD_BLOCK_DIM==1`。
 */
__aicore__ inline void ComputeYeeCbd(GM_ADDR ws)
{
    using namespace tiling;

    uint8_t coins[32];
    const __gm__ uint8_t *coinsGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_COINS);
    for (uint32_t i = 0U; i < 32U; ++i) {
        coins[i] = coinsGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    __gm__ uint8_t *prfGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_PRF);
    __gm__ int32_t *yeeGm = reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_E1_E2);

    RunPrf9FromCoins(coins, prfGm);
    AscendC::PipeBarrier<PIPE_ALL>();
    RunCbd9FromPrf(prfGm, yeeGm);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace reenc_cbd

#endif
