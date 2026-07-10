/**
 * @file f203_keygen_prep_extseed.hpp
 * @brief Alg.19 旁路 A（KEM_KG_EXT_SEED，test-only）：prep 单 TPipe 的“外部 d 注入”变体。
 *
 * 背景：正确性交叉验证需让 liboqs `keypair_derand` 与本用例吃**相同随机字节**。
 *   生产路径由 device UB 内 `DerandFromSeedD(seed_d)` 派生 d（INTEGRATION_PLAN 锁定，
 *   禁止 host 预填 kem_seed）；本头仅在 **KEM_KG_EXT_SEED=1 的测试构建**中编入，
 *   把 host 提供的 d[32] 直接当作 KeyGen 随机性，其余流水与
 *   `F203KeygenPrep::BuildKeygenPrepSinglePipe` 逐字一致（唯一差异：ρ‖σ 来源）。
 *
 * 与 vendored 关系：本文件 **不改** vendor/pke_keygen（每次 build 被 rsync --delete 覆盖），
 *   而是复用其构件（BuildAHat16ShardWithUb / RunShakePrfBatchUbWithUb / SamplePolyCbd2Batch8WithUb）
 *   与常量；仅复制 ~40 行编排以替换 d 的派生点。若 stable prep 编排变更，本变体需同步。
 *
 * 数学契约：G(d‖byte(k=4)) = ρ‖σ（一次 SHA3-512，与 F203Alg7::HashGFull 同式）；
 *   d 由 host kem_seed 前 32B 提供，不再经 SHA3-256(域分离串‖SEED_D)。
 */
#pragma once

#include "f203_keygen_prep_ub.hpp"

namespace F203KeygenPrep {

/**
 * prep 单 TPipe（外部 d 注入版）：行 3–7 Â → 行 8–15 presample V3。
 * 与 BuildKeygenPrepSinglePipe 唯一差异：ρ‖σ 由 G(d_ext‖k) 得到，d_ext 来自 host kem_seed[0:32]。
 *
 * @param d_ext      host 提供的 32B 随机 d（旁路 A）
 * @param blockIdx   逻辑分片：0→poly 0–7，1→8–15；PRF/CBD 仅 block0 执行
 * @param a_hat_gm   输出 Â[16,256] int32，行主序
 * @param prf_out_gm PRF 中间态 [8,128] uint8（block0 写入，供 CBD 读）
 * @param src_gm     输出 ŝ 秘密向量 [8,256] int32
 * @param rho_gm     ρ 落盘 GM（block0），供行 21 ek‖ρ 设备拼接
 * @param tiling_gm  presample SHAKE batch tiling（host 填充）
 */
__aicore__ inline void BuildKeygenPrepSinglePipeExtD(const uint8_t d_ext[32], uint32_t blockIdx,
                                                     __gm__ int32_t *a_hat_gm, __gm__ uint8_t *prf_out_gm,
                                                     __gm__ int32_t *src_gm, __gm__ uint8_t *rho_gm, GM_ADDR tiling_gm)
{
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    // 旁路 A 差异点：ρ‖σ = G(d_ext‖byte(k=4))，一次 SHA3-512（替代 BuildRhoSigmaFromSeedD 的
    // DerandFromSeedD(seed_d)→d 步骤）；d_ext 即 host kem_seed 前 32B。
    uint8_t rho[32];
    uint8_t sigma[32];
    uint8_t gout[64];
    F203Alg7::HashGFull(d_ext, gout);
    for (uint32_t i = 0U; i < 32U; ++i) {
        rho[i] = gout[i];
        sigma[i] = gout[32U + i];
    }
    // block0 将设备侧 G(d||k) 的 ρ 落盘 GM，供行 21 ek‖ρ 设备拼接（与生产路径一致）
    if (AscendC::GetBlockIdx() == 0U && rho_gm != nullptr) {
        StoreRhoToGm(rho_gm, rho);
    }

    // 以下 UB/Que 布局与 vendor BuildKeygenPrepSinglePipe 同尺寸（Alg.19→Alg.13 prep）
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aHatQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;

    pipe.InitBuffer(shakeXBuf, kPrepShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kPrepShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(d2Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, kPrepPrfYUbBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    // 双 AIV：blockIdx 0→poly 0–7、1→8–15 并行写 Â（同生产路径）
    F203Ahat16::BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                      d1Que, d2Que, aHatQue, scratchBuf);

    F203_PREP_PIPE_ALL();

    // block0 独占 PRF+CBD 采 ŝ；block1 须等 PIPE_ALL，不得提前 return
    if (AscendC::GetBlockIdx() == 0U) {
        ShakeGeneralTilingData tilingLocal{};
        F203SeVector::LoadTilingFromGm(tiling_gm, tilingLocal);
        F203SeVector::RunShakePrfBatchUbWithUb(sigma, prf_out_gm, tilingLocal, shakeXBuf, shakeLenBuf,
                                               shakeStagingBuf, aHatQue);
        F203_PREP_PIPE_ALL();

        F203CbdEta2::SamplePolyCbd2Batch8WithUb(0U, prf_out_gm, src_gm, scratchBuf, aHatQue);
    }

    F203_PREP_PIPE_ALL();
}

}  // namespace F203KeygenPrep
