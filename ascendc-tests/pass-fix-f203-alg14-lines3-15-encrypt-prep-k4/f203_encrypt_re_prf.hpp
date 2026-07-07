/**
 * @file f203_encrypt_re_prf.hpp
 * @brief Alg.14 PRF：coins → 9× SHAKE256；8+1 两段（对齐 stable batch=8 + 单 nonce）。
 */
#pragma once

#include "f203_se_vector_prf.hpp"
#include "shake_general.h"
#include "shake_ub_helpers.hpp"

#include <cstdint>

namespace F203EncryptRePrf {

constexpr uint32_t PRF_BATCH8 = F203SeVector::PRF_BATCH;  // 8
constexpr uint32_t PRF_OUT_LEN = F203SeVector::PRF_OUT_LEN;
constexpr uint32_t PRF_MSG_STRIDE = F203SeVector::PRF_MSG_STRIDE;
constexpr uint32_t PRF_TOTAL_BATCH = 9U;

#define F203_ENCRYPT_PRF_PIPE_ALL() ShakeXofUb::PipeAll()

/** 单 nonce PRF：coins‖byte(nonce) → prf_out[128]。 */
__aicore__ inline void RunShakePrfOneNonceUbWithUb(const uint8_t coins[32], uint32_t nonce,
                                                   __gm__ uint8_t *prf_row_gm,
                                                   ShakeGeneralTilingData tilingLocal,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    tilingLocal.batch = 1U;
    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();

    ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, 0U);
    lengthsUb.SetValue(0U, F203SeVector::PRF_MSG_LEN);
    F203_ENCRYPT_PRF_PIPE_ALL();

    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &tilingLocal);
    F203_ENCRYPT_PRF_PIPE_ALL();

    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prf_row_gm, PRF_OUT_LEN);
    AscendC::DataCopy(prfGm, yUb, PRF_OUT_LEN);
    F203_ENCRYPT_PRF_PIPE_ALL();
    yQue.FreeTensor(yUb);
}

/**
 * coins → prf_out[9,128]：先 stable batch8（nonce 0–7），再单 nonce 8（e₂）。
 *
 * @param tiling_gm host 填 batch=8 的 ShakeGeneralTilingData
 */
__aicore__ inline void RunShakePrfEncrypt9UbWithUb(const uint8_t coins[32], __gm__ uint8_t *prf_out_gm,
                                                   GM_ADDR tiling_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    ShakeGeneralTilingData tilingLocal{};
    F203SeVector::LoadTilingFromGm(tiling_gm, tilingLocal);
    tilingLocal.batch = PRF_BATCH8;

    F203SeVector::RunShakePrfBatchUbWithUb(coins, prf_out_gm, tilingLocal, xBuf, lenBuf, stagingBuf, yQue);

    RunShakePrfOneNonceUbWithUb(coins, 8U, prf_out_gm + PRF_BATCH8 * PRF_OUT_LEN, tilingLocal, xBuf, lenBuf,
                                stagingBuf, yQue);
}

}  // namespace F203EncryptRePrf
