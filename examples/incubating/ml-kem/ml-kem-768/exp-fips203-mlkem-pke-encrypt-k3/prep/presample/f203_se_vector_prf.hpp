// @probe pass-fix-f203-alg14-pke-encrypt-device-k3
// @file prep/presample/f203_se_vector_prf.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector_prf.hpp
 * @brief Phase P：σ → 8× SHAKE256 PRF（单 TPipe UB shake，链末 DataCopy 对拍 prf_out GM）。
 *
 * 流水线位置：
 *   - KeyGen/presample：σ = G(d) 后半 → PRF(σ, nonce=0..7) → prf_out[8,128]
 *   - Encrypt prep：复用 RunShakePrfBatchUbWithUb（coins 代替 σ），再由 f203_encrypt_re_prf 补 nonce 8
 *
 * 关键点：PRF_MSG_STRIDE=64（8B 对齐），lengths 仍为 33；见 2026-06-25 SIM pem_lsu 回归。
 * 与 golden：prf_shake256 / PRF_BYTES=128。
 */
#pragma once

#include "f203_se_vector_g.hpp"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include <cstdint>

namespace F203SeVector {

#define F203_SE_PRF_PIPE_ALL() ShakeXofUb::PipeAll()

constexpr uint32_t PRF_BATCH = 8U;
/** 有效 PRF 消息长度 σ‖N（FIPS 203）。 */
constexpr uint32_t PRF_MSG_LEN = 33U;
/**
 * UB x 行 stride：须 8B 对齐，供 shake_general XorBlock32 的 uint64 块读。
 * 背景：maxMsgLen=33 时 msgBase=33,66,99… 非 8B 对齐 → SIM pem_lsu 告警（2026-06-25 回归）。
 * lengths[i] 仍为 PRF_MSG_LEN；仅布局 stride 垫到 64。
 */
constexpr uint32_t PRF_MSG_STRIDE = ShakeXofUb::CeilAlign32(PRF_MSG_LEN);
constexpr uint32_t PRF_OUT_LEN = 128U;
constexpr uint32_t PRF_X_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * PRF_MSG_STRIDE);
constexpr uint32_t PRF_LEN_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t PRF_Y_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * PRF_OUT_LEN);

/**
 * 自 GM 加载 ShakeGeneralTilingData（uint32 字段顺序与 host FillShakeTiling 一致）。
 */
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

/** σ‖N 写入 UB x/lengths（batch=8，行优先；stride=PRF_MSG_STRIDE）。 */
__aicore__ inline void FillPrfMessagesUb(const uint8_t sigma[32], AscendC::LocalTensor<uint8_t> &xUb,
                                         AscendC::LocalTensor<uint32_t> &lengthsUb)
{
    for (uint32_t nonce = 0U; nonce < PRF_BATCH; ++nonce) {
        const uint32_t rowBase = nonce * PRF_MSG_STRIDE;
        ShakeXofUb::FillShakeRowUb(sigma, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, rowBase);
        lengthsUb.SetValue(nonce, PRF_MSG_LEN);
    }
    F203_SE_PRF_PIPE_ALL();
}

/**
 * UB SHAKE batch → prf_out GM；yQue 须已 InitBuffer(1, PRF_Y_UB_BYTES)。
 * Encrypt prep 与 KeyGen 共用此入口（密钥材料为 coins 或 σ）。
 */
__aicore__ inline void RunShakePrfBatchUbWithUb(const uint8_t sigma[32], __gm__ uint8_t *prf_out_gm,
                                                const ShakeGeneralTilingData &tilingLocal,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &xBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &lenBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &stagingBuf,
                                                AscendC::TQue<AscendC::TPosition::VECOUT, 1> &yQue)
{
    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();

    FillPrfMessagesUb(sigma, xUb, lengthsUb);
    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &tilingLocal);
    F203_SE_PRF_PIPE_ALL();

    // 链末一次写出 8×128B
    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prf_out_gm, PRF_BATCH * PRF_OUT_LEN);
    AscendC::DataCopy(prfGm, yUb, PRF_BATCH * PRF_OUT_LEN);
    F203_SE_PRF_PIPE_ALL();
    yQue.FreeTensor(yUb);
}

/** 单 TPipe：UB SHAKE batch → prf_out GM（独立探针入口用）。 */
__aicore__ inline void RunShakePrfBatchUb(const uint8_t sigma[32], __gm__ uint8_t *prf_out_gm,
                                          const ShakeGeneralTilingData &tilingLocal)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> yQue;
    pipe.InitBuffer(xBuf, PRF_X_UB_BYTES);
    pipe.InitBuffer(lenBuf, PRF_LEN_UB_BYTES);
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yQue, 1, PRF_Y_UB_BYTES);

    RunShakePrfBatchUbWithUb(sigma, prf_out_gm, tilingLocal, xBuf, lenBuf, stagingBuf, yQue);
}

/**
 * SEED_D → σ → PRF×8 → prf_out（KeyGen/presample 独立核路径）。
 * Encrypt prep 不经此函数（coins 直接进 RunShakePrfEncrypt9UbWithUb）。
 */
__aicore__ inline void BuildPrfOutFromSeedD(uint32_t seed_d, GM_ADDR prf_out_gm, GM_ADDR tiling_gm)
{
    uint8_t sigma[32];
    BuildSigmaFromSeedD(seed_d, sigma);

    ShakeGeneralTilingData tilingLocal{};
    LoadTilingFromGm(tiling_gm, tilingLocal);
    RunShakePrfBatchUb(sigma, reinterpret_cast<__gm__ uint8_t *>(prf_out_gm), tilingLocal);
}

}  // namespace F203SeVector
