/**
 * @file f203_encrypt_re_prf.hpp
 * @brief Alg.14 PRF：coins → 5× SHAKE256；逐 nonce 单行写出（ML-KEM-512）。
 *
 * 流水线位置（Encrypt prep 行 8–15 前半）：
 *   coins[32] → PRF(coins, nonce)=SHAKE256(coins‖byte(nonce)).squeeze(128)
 *   nonce 0..1 → r；2..3 → e₁；4 → e₂；写出 prf_out_gm[5,128]
 *
 * 编排原因：k2 仅 5 行，不能用 batch8 写越界；复用同一 SHAKE UB，但每次将 tiling.batch 改为 1。
 *
 * 与 golden：scripts/golden_encrypt_prep.prf_shake256；本文件只产中间态，CBD 见 f203_encrypt_re_cbd.hpp。
 */
#pragma once

#include "f203_se_vector_prf.hpp"
#include "shake_general.h"
#include "shake_ub_helpers.hpp"

#include <cstdint>

namespace F203EncryptRePrf {

constexpr uint32_t PRF_OUT_LEN = F203SeVector::PRF_OUT_LEN;
constexpr uint32_t PRF_MSG_STRIDE = F203SeVector::PRF_MSG_STRIDE;
/** Encrypt k2 需要 5 行 PRF（r2 + e₁2 + e₂1）。 */
constexpr uint32_t PRF_TOTAL_BATCH = 5U;

#define F203_ENCRYPT_PRF_PIPE_ALL() ShakeXofUb::PipeAll()

/**
 * 单 nonce PRF：coins‖byte(nonce) → prf_out[128]。
 *
 * @param coins         Host/UB 已载入的 32B coins
 * @param nonce         0..4
 * @param prf_row_gm    该行 GM 起点（通常 prf_out + nonce*128）
 * @param tilingLocal   调用方传入；本函数内强制 batch=1
 * @param xBuf/lenBuf/stagingBuf/yQue  与 batch8 路径共用的已 InitBuffer UB
 */
__aicore__ inline void RunShakePrfOneNonceUbWithUb(const uint8_t coins[32], uint32_t nonce,
                                                   __gm__ uint8_t *prf_row_gm,
                                                   ShakeGeneralTilingData tilingLocal,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    // 单行 SHAKE：覆盖 host 填的 batch=8
    tilingLocal.batch = 1U;
    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();

    // 消息：coins[32] + nonce 字节；lengths 仍为有效长 33（stride 由 FillShakeRowUb 布局）
    ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, 0U);
    lengthsUb.SetValue(0U, F203SeVector::PRF_MSG_LEN);
    F203_ENCRYPT_PRF_PIPE_ALL();

    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &tilingLocal);
    F203_ENCRYPT_PRF_PIPE_ALL();

    // 链末写 GM 单行 128B
    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prf_row_gm, PRF_OUT_LEN);
    AscendC::DataCopy(prfGm, yUb, PRF_OUT_LEN);
    F203_ENCRYPT_PRF_PIPE_ALL();
    yQue.FreeTensor(yUb);
}

/**
 * coins → prf_out[5,128]：逐 nonce 单行 SHAKE，避免 batch8 越界。
 *
 * @param coins       32B 随机性
 * @param prf_out_gm  GM 输出 [5,128] uint8，行主序
 * @param tiling_gm   host 填的 ShakeGeneralTilingData（设备侧每行覆盖 batch=1）
 * @param xBuf 等     与 Â 路径复用的 shake UB（由 BuildEncryptPrepSinglePipe Init）
 */
__aicore__ inline void RunShakePrfEncryptReUbWithUb(const uint8_t coins[32], __gm__ uint8_t *prf_out_gm,
                                                   GM_ADDR tiling_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    ShakeGeneralTilingData tilingLocal{};
    F203SeVector::LoadTilingFromGm(tiling_gm, tilingLocal);
    for (uint32_t nonce = 0U; nonce < PRF_TOTAL_BATCH; ++nonce) {
        RunShakePrfOneNonceUbWithUb(coins, nonce, prf_out_gm + nonce * PRF_OUT_LEN, tilingLocal, xBuf, lenBuf,
                                    stagingBuf, yQue);
    }
}

}  // namespace F203EncryptRePrf
