/**
 * @file f203_encrypt_re_prf.hpp
 * @brief Alg.14 G1 Phase P：coins[32] → 9× SHAKE256 PRF（nonce 0..8）。
 *
 * 与 KeyGen σ→PRF 同 shake_general 路径；Encrypt 直接用 coins 作 PRF 密钥，不经 G 派生。
 */
#pragma once

#include "f203_encrypt_re_layout.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include <cstdint>

namespace F203EncryptRe {

#define F203_ENCRYPT_RE_PRF_PIPE_ALL() ShakeXofUb::PipeAll()

/** UB x 行 stride：8B 对齐（与 se_vector PRF 一致）。 */
constexpr uint32_t kPrfMsgStride = ShakeXofUb::CeilAlign32(kPrfMsgLen);
constexpr uint32_t kPrfXUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfMsgStride);
constexpr uint32_t kPrfLenUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t kPrfYUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfOutLen);

__aicore__ inline void LoadTilingFromGm(GM_ADDR tiling, ShakeGeneralTilingData &td)
{
    const __gm__ uint32_t *p = reinterpret_cast<const __gm__ uint32_t *>(tiling);
    td.batch = p[0];
    td.maxMsgLen = p[1];
    td.outLen = p[2];
    td.rate = p[3];
    td.blockDim = p[4];
    td.groupSize = p[5];
    td.reserved0 = p[6];
    td.reserved1 = p[7];
    td.reserved2 = p[8];
}

/** coins‖nonce 写入 UB batch（nonce 0..8）。 */
__aicore__ inline void FillPrfMessagesFromCoinsUb(const uint8_t coins[32], AscendC::LocalTensor<uint8_t> &xUb,
                                                  AscendC::LocalTensor<uint32_t> &lengthsUb)
{
    for (uint32_t nonce = 0U; nonce < kPrfBatch; ++nonce) {
        const uint32_t rowBase = nonce * kPrfMsgStride;
        ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, rowBase);
        lengthsUb.SetValue(nonce, kPrfMsgLen);
    }
    F203_ENCRYPT_RE_PRF_PIPE_ALL();
}

__aicore__ inline void RunShakePrfBatchUbWithUb(const uint8_t coins[32], __gm__ uint8_t *prf_out_gm,
                                                const ShakeGeneralTilingData &tilingLocal,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();

    FillPrfMessagesFromCoinsUb(coins, xUb, lengthsUb);
    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &tilingLocal);
    F203_ENCRYPT_RE_PRF_PIPE_ALL();

    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prf_out_gm, kPrfTotalBytes);
    AscendC::DataCopy(prfGm, yUb, kPrfTotalBytes);
    F203_ENCRYPT_RE_PRF_PIPE_ALL();
    yQue.FreeTensor(yUb);
}

__aicore__ inline void RunShakePrfBatchUb(const uint8_t coins[32], __gm__ uint8_t *prf_out_gm,
                                          const ShakeGeneralTilingData &tilingLocal)
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

    RunShakePrfBatchUbWithUb(coins, prf_out_gm, tilingLocal, xBuf, lenBuf, stagingBuf, yQue);
}

/** 从 GM 读 coins[32]，batch9 SHAKE256 PRF → prf_out[9,128] GM。 */
__aicore__ inline void BuildPrfOutFromCoinsGm(const __gm__ uint8_t *coins_gm, GM_ADDR prf_out_gm, GM_ADDR tiling_gm)
{
    uint8_t coins[32];
    for (uint32_t i = 0U; i < 32U; ++i) {
        coins[i] = coins_gm[i];
    }

    ShakeGeneralTilingData tilingLocal{};
    LoadTilingFromGm(tiling_gm, tilingLocal);
    RunShakePrfBatchUb(coins, reinterpret_cast<__gm__ uint8_t *>(prf_out_gm), tilingLocal);
}

}  // namespace F203EncryptRe
