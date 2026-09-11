/**
 * @file cbd_device.hpp
 * @brief RB-T16 设备侧薄壳：读 Host coins → SHAKE256 PRF(N=0..8) → Alg.8 CBD η=2 → y‖e1‖e2。
 *
 * 流水线位置：MIX AIV0 在 flag 1/3 握手之后调用；写出 [9,256] int32。
 *
 * 积木接线（**不**大段抄探针实现）：
 *   - `library/shared/shake_xof_kernel`：`ShakeXofUb::*`（Encrypt 用 SHAKE256，coins 当 σ）
 *   - `-I` 活跃 `pass-fix-f203-alg8-cbd-eta2-k4`：`F203CbdEta2::SamplePolyCbd2OneRowUb`
 *
 * 与 KeyGen 行 8–15 差异（一行）：本刀 **无** Phase G；PRF 种子直接是 coins（对齐 T07）。
 * 与 alg8 探针差异：输入是 coins 而非预喂 prf_out；输出 9 poly（含 e₂）而非 8。
 *
 * 背景：T07 契约 nonce y=0..3 / e1=4..7 / e2=8；本刀关 Encrypt 设备 CBD。
 * 结论：I/O 对齐 T07；MIX 壳对齐 T13；禁抄 encrypt/frozen。
 */
#ifndef RB_T16_CBD_DEVICE_HPP
#define RB_T16_CBD_DEVICE_HPP

// 本刀 MIX blockDim=1：P1b-single 串行 9 行（须在 include 积木头之前锁定）
#ifndef F203_CBD_BLOCK_DIM
#define F203_CBD_BLOCK_DIM 1
#endif

#include "f203_cbd_eta2.hpp"
#include "kernel_operator.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"
#include "tiling.h"

#include <cstdint>

namespace rb_t16 {

/** Encrypt PRF：有效消息 coins‖N = 33B；UB 行 stride 须 8B 对齐（防 SIM pem_lsu）。 */
constexpr uint32_t kPrfMsgLen = 33U;
constexpr uint32_t kPrfMsgStride = ShakeXofUb::CeilAlign32(kPrfMsgLen); // 64
constexpr uint32_t kPrfBatch = 9U;
constexpr uint32_t kPrfOutLen = 128U;
constexpr uint32_t kPrfXUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfMsgStride);
constexpr uint32_t kPrfLenUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t kPrfYUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfOutLen);

/**
 * Phase P：coins → prf_out[9,128]（SHAKE256，nonce 0..8）。
 * @param coins     Host 预喂 32B
 * @param prfOutGm  workspace PRF 区
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

    // 填 9 行消息：coins‖byte(N)，lengths=33；行距 64B
    for (uint32_t nonce = 0U; nonce < kPrfBatch; ++nonce) {
        const uint32_t rowBase = nonce * kPrfMsgStride;
        ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, rowBase);
        lengthsUb.SetValue(nonce, kPrfMsgLen);
    }
    ShakeXofUb::PipeAll();

    ShakeGeneralTilingData td{};
    // maxMsgLen=stride(64)，与 lengths 有效长 33 分离（lines8-15 回归结论）
    ShakeXofUb::FillShakeTilingUb(td, kPrfBatch, kPrfMsgStride, kPrfOutLen, SHAKE256_RATE_BYTES);
    td.blockDim = 1U; // 内嵌 ProcessInline：单 AIV 独占整批

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
 * Phase C：prf_out[9,128] → src[9,256] int32（逐行 OneRowUb；扩展 alg8 的 8 行 API）。
 * @param prfGm  PRF GM
 * @param srcGm  CBD 结果 GM（y‖e1‖e2 平面）
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
    // 9 行总长（alg8 默认 ROWS=8；本刀显式扩到 9）
    prfTensor.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBatch * F203CbdEta2::PRF_BYTES);
    srcTensor.SetGlobalBuffer(srcGm, kPrfBatch * F203CbdEta2::N);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    for (uint32_t row = 0U; row < kPrfBatch; ++row) {
        F203CbdEta2::SamplePolyCbd2OneRowUb(row, prfTensor, srcTensor, prfLocal, rowQue);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0：从 ws 读 coins[32]，跑 PRF+CBD，写 yeeOut 与 ws 镜像区。
 *
 * @param ws     共享 workspace（含 OFF_COINS / OFF_PRF / OFF_YEE）
 * @param yeeOut 独立输出 GM（Host 对拍用；启动前须清零）；布局 y[4]‖e1[4]‖e2[1]
 *
 * 前置：仅 AIV0 调用；`F203_CBD_BLOCK_DIM==1`。
 */
__aicore__ inline void ComputeYeeCbd(GM_ADDR ws, GM_ADDR yeeOut)
{
    using namespace tiling;

    // ---------- 1) 读 Host 预喂 coins（标量；32B）----------
    uint8_t coins[32];
    const __gm__ uint8_t *coinsGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_COINS);
    for (uint32_t i = 0U; i < 32U; ++i) {
        coins[i] = coinsGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    __gm__ uint8_t *prfGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_PRF);
    __gm__ int32_t *yeeGm = reinterpret_cast<__gm__ int32_t *>(yeeOut);

    // ---------- 2) Phase P：coins → prf[9×128] ----------
    RunPrf9FromCoins(coins, prfGm);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 3) Phase C：prf → CBD → yeeOut（禁 GM→GM 镜像；ws.YEE 仅布局占位）----------
    RunCbd9FromPrf(prfGm, yeeGm);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace rb_t16

#endif
