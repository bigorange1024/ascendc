/**
 * @file f203_encrypt_prep_ub.hpp
 * @brief Alg.14 Encrypt prep 单 TPipe：ρ→Â（双 AIV）→ coins→PRF+CBD batch9（block0）。
 *
 * 流水线位置：设备核 `f203_encrypt_prep` 的主体；FIPS 203 / ML-KEM-1024。
 * 背景：对齐 stable KeyGen `BuildKeygenPrepSinglePipe`；ρ 自 ek_pke 尾 32B，不经 G(d)。
 * 与 golden：产出 Â/re 供后续 compute；最终对拍仍为 `c.bin`。
 */
#pragma once

#include "f203_a_hat16_ub.hpp"
#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_re_cbd.hpp"
#include "f203_encrypt_re_prf.hpp"
#include "shake_general_tiling_data.h"

namespace F203EncryptPrep {

#define F203_ENCRYPT_PREP_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

constexpr uint32_t kPrepShakeXUbBytes = F203SeVector::PRF_X_UB_BYTES;
constexpr uint32_t kPrepShakeLenUbBytes = F203SeVector::PRF_LEN_UB_BYTES;
constexpr uint32_t kPrepPrfYUbBytes = F203SeVector::PRF_Y_UB_BYTES;

/** 从 ek_pke GM 读取 ρ[32]（偏移 1536）。 */
__aicore__ inline void LoadRhoFromEkGm(const __gm__ uint8_t *ek_gm, uint8_t rho[kRhoBytes])
{
    for (uint32_t i = 0U; i < kRhoBytes; ++i) {
        rho[i] = ek_gm[kRhoOffset + i];
    }
}

/** 从 coins GM 读取 32B。 */
__aicore__ inline void LoadCoinsFromGm(const __gm__ uint8_t *coins_gm, uint8_t coins[kCoinsBytes])
{
    for (uint32_t i = 0U; i < kCoinsBytes; ++i) {
        coins[i] = coins_gm[i];
    }
}

/**
 * 行 3–15 设备采样：Â + r/e₁/e₂。
 *
 * @param ek_gm       GM 输入 ek_pke[1568]
 * @param coins_gm    GM 输入 coins[32]
 * @param blockIdx    双 AIV 分片：0→poly 0–7，1→8–15；PRF/CBD 仅 block0
 * @param a_hat_gm    输出 Â[16,256] int32
 * @param prf_out_gm  PRF 中间态 [9,128] uint8（block0 写）
 * @param re_gm       输出 r‖e₁‖e₂ [9,256] int32
 * @param tiling_gm   SHAKE batch tiling（host 填 batch=9）
 */
__aicore__ inline void BuildEncryptPrepSinglePipe(const __gm__ uint8_t *ek_gm, const __gm__ uint8_t *coins_gm,
                                                  uint32_t blockIdx, __gm__ int32_t *a_hat_gm,
                                                  __gm__ uint8_t *prf_out_gm, __gm__ int32_t *re_gm, GM_ADDR tiling_gm)
{
    // 超出双 AIV 分片的 block 直接返回（防御性）
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    uint8_t rho[kRhoBytes];
    LoadRhoFromEkGm(ek_gm, rho);  // ρ = ek[1536:1568]

    // 单 TPipe：SampleNTT 与 PRF/CBD 共用 shake/xof/d12/prf 缓冲（串行阶段复用）
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> prfYQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;

    pipe.InitBuffer(shakeXBuf, kPrepShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kPrepShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(d2Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(prfYQue, 1, kPrepPrfYUbBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    // 行 3–7：本 block 负责的 Â 分片（8 poly）写 a_hat_gm
    F203Ahat16::BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                       d1Que, d2Que, prfYQue, scratchBuf);

    F203_ENCRYPT_PREP_PIPE_ALL();

    // 行 8–15：仅 block0 做 PRF+CBD，避免双核写同一 re/prf
    if (AscendC::GetBlockIdx() == 0U) {
        uint8_t coins[kCoinsBytes];
        LoadCoinsFromGm(coins_gm, coins);

        ShakeGeneralTilingData tilingLocal{};
        F203EncryptRePrf::RunShakePrfEncrypt9UbWithUb(coins, prf_out_gm, tiling_gm, shakeXBuf, shakeLenBuf,
                                                      shakeStagingBuf, prfYQue);
        F203_ENCRYPT_PREP_PIPE_ALL();

        F203EncryptReCbd::SamplePolyCbd2Batch9WithUb(prf_out_gm, re_gm, scratchBuf, prfYQue);
    }

    F203_ENCRYPT_PREP_PIPE_ALL();
}

}  // namespace F203EncryptPrep
