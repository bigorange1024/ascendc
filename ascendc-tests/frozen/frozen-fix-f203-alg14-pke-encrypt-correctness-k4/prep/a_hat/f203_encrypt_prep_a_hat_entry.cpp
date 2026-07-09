/**
 * @file f203_encrypt_prep_a_hat_entry.cpp
 * @brief Alg.14 G1 Launch-1：ρ（ek_pke 尾 32B）→ a_hat[16,256] int32。
 *
 * 与 KeyGen 行 3–7 同几何；**直接**使用 ρ，不经 SEED_D→G 派生（Encrypt 输入已含 ρ）。
 * 核心：F203Ahat16::BuildAHat16ShardWithUb(ρ, …)，禁止 BuildAHat16ShardFromSeedD。
 */
#include "f203_a_hat16_config.h"
#include "f203_a_hat16_layout.h"
#include "f203_a_hat16_ub.hpp"

namespace F203EncryptPrepAHat {

/** 从 GM 读 ρ[32]，初始化 UB 缓冲后调用 BuildAHat16ShardWithUb。 */
__aicore__ inline void BuildAHat16ShardFromRhoGm(const __gm__ uint8_t *rho_gm, __gm__ int32_t *a_hat_gm,
                                                  uint32_t blockIdx)
{
    uint8_t rho[F203Alg7::kRhoBytes];
    for (uint32_t i = 0U; i < F203Alg7::kRhoBytes; ++i) {
        rho[i] = rho_gm[i];
    }

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aHatQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;

    constexpr uint32_t kShakeXUbBytes = 64U;
    constexpr uint32_t kShakeLenUbBytes = 32U;
    pipe.InitBuffer(shakeXBuf, kShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(d2Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, F203Ahat16::kPolyAHatBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    F203Ahat16::BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                       d1Que, d2Que, aHatQue, scratchBuf);
}

}  // namespace F203EncryptPrepAHat

extern "C" __global__ __aicore__ void f203_encrypt_prep_a_hat(GM_ADDR rho_gm, GM_ADDR a_hat_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    const __gm__ uint8_t *rhoPtr = reinterpret_cast<const __gm__ uint8_t *>(rho_gm);
    __gm__ int32_t *aHatPtr = reinterpret_cast<__gm__ int32_t *>(a_hat_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    F203EncryptPrepAHat::BuildAHat16ShardFromRhoGm(rhoPtr, aHatPtr, 0U);
    F203EncryptPrepAHat::BuildAHat16ShardFromRhoGm(rhoPtr, aHatPtr, 1U);
#else
    F203EncryptPrepAHat::BuildAHat16ShardFromRhoGm(rhoPtr, aHatPtr, AscendC::GetBlockIdx());
#endif
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_prep_a_hat_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *rho_gm,
                                           uint8_t *a_hat_gm)
{
    f203_encrypt_prep_a_hat<<<blockDim, l2ctrl, stream>>>(rho_gm, a_hat_gm);
}
#endif
