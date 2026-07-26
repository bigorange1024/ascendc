/**
 * @file f203_encrypt_prep_ub.hpp
 * @brief Alg.14 Encrypt prep 单 TPipe：ρ→Â（双 AIV 5+4）→ coins→PRF+CBD batch7（block0）。
 *
 * 流水线位置（行 3–15）：
 *   1. 自 ek_pke GM 读 ρ[32]（偏移 1152）
 *   2. BuildAHat16ShardWithUb：本 block 分片 SampleNTT → a_hat_gm
 *   3. 仅 block0：LoadCoins → PRF×7 → CBD×7 → re_gm
 *
 * 背景：对齐 k3 D13/B4-B6 已绿 prep 组件；ρ 自 ek_pke 尾 32B，不经 G(d)。
 * UB 复用：shakeX/Len/Staging 先服务 Â，再给 PRF；prfYQue 兼作 aHatQue / CBD rowQue。
 *
 * 与 golden：output a_hat / re 对拍；prf_out 为中间态。
 */
#pragma once

#include "f203_a_hat16_ub.hpp"
#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_re_cbd.hpp"
#include "f203_encrypt_re_prf.hpp"
#include "shake_general_tiling_data.h"

namespace F203EncryptPrep {

#define F203_ENCRYPT_PREP_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/** PRF batch7 的 x/len UB 尺寸（≥ 单 poly SampleNTT 消息缓冲，可复用）。 */
constexpr uint32_t kPrepShakeXUbBytes = F203SeVector::PRF_X_UB_BYTES;
constexpr uint32_t kPrepShakeLenUbBytes = F203SeVector::PRF_LEN_UB_BYTES;
constexpr uint32_t kPrepPrfYUbBytes = F203SeVector::PRF_Y_UB_BYTES;

/** 从 ek_pke GM 读取 ρ[32]（偏移 kRhoOffset=1152）。 */
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
 * @param ek_gm       GM 输入 ek_pke[1184]
 * @param coins_gm    GM 输入 coins[32]
 * @param blockIdx    双 AIV 分片：0→poly 0–4，1→5–8；PRF/CBD 仅 block0
 * @param a_hat_gm    输出 Â[9,256] int32
 * @param prf_out_gm  PRF 中间态 [7,128] uint8（block0 写）
 * @param re_gm       输出 r‖e₁‖e₂ [7,256] int32
 * @param tiling_gm   SHAKE tiling（设备每 nonce 覆盖 batch=1）
 */
__aicore__ inline void BuildEncryptPrepSinglePipe(const __gm__ uint8_t *ek_gm, const __gm__ uint8_t *coins_gm,
                                                  uint32_t blockIdx, __gm__ int32_t *a_hat_gm,
                                                  __gm__ uint8_t *prf_out_gm, __gm__ int32_t *re_gm, GM_ADDR tiling_gm)
{
    // 越界 block 直接返回（与 launch blockDim 防护一致）
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    uint8_t rho[kRhoBytes];
    LoadRhoFromEkGm(ek_gm, rho);

    // 单 TPipe：Â 与 PRF/CBD 共用 shake / scratch / y 队列
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
    // prfYQue 容量按 PRF batch y；Â 路径作 aHatQue（256×int32），CBD 作 rowQue
    pipe.InitBuffer(prfYQue, 1, kPrepPrfYUbBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    // 行 3–7：本分片 Â
    F203Ahat16::BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                       d1Que, d2Que, prfYQue, scratchBuf);

    F203_ENCRYPT_PREP_PIPE_ALL();

    // 行 8–15：仅 block0 写 PRF + CBD，避免双 AIV 写同一 re_gm
    if (AscendC::GetBlockIdx() == 0U) {
        uint8_t coins[kCoinsBytes];
        LoadCoinsFromGm(coins_gm, coins);

        // tiling 由 RunShakePrfEncrypt7UbWithUb 自 tiling_gm 加载；每个 nonce 覆盖 batch=1。
        F203EncryptRePrf::RunShakePrfEncrypt7UbWithUb(coins, prf_out_gm, tiling_gm, shakeXBuf, shakeLenBuf,
                                                      shakeStagingBuf, prfYQue);
        F203_ENCRYPT_PREP_PIPE_ALL();

        F203EncryptReCbd::SamplePolyCbd2Batch7WithUb(prf_out_gm, re_gm, scratchBuf, prfYQue);
    }

    F203_ENCRYPT_PREP_PIPE_ALL();
}

}  // namespace F203EncryptPrep
