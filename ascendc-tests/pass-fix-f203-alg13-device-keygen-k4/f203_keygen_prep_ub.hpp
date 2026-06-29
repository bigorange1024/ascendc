// @probe pass-fix-f203-alg13-device-keygen-k4
// @file f203_keygen_prep_ub.hpp
// @layer host
// @role 头文件/内联：`f203_keygen_prep_ub.hpp` 声明或配置 AscendC/host 接口与常量。 / Header `f203_keygen_prep_ub.hpp`.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影，以 profile_subtask_log*.toml 为准。
// @depends #include: f203_a_hat16_layout.h, f203_a_hat16_ub.hpp, f203_alg7_g.hpp, f203_cbd_eta2.hpp, f203_keygen_prep_layout.h, f203_se_vector_prf.hpp, shake_general_tiling_data.h
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。



/**
 * @file f203_keygen_prep_ub.hpp
 * @brief KeyGen 准备段单 TPipe：行 3–7 Â → 行 8–15 presample V3（一次 G 派生 + UB 复用）。
 *
 * 背景：多 TPipe 串接（Â 析构后再 Init PRF/CBD）在 SIM 上较分段探针之和多 ~3% tick；
 * 本路径与 a_hat / presample 子探针语义一致，仅编排为同一 pipe 生命周期。
 */
#pragma once

#include "f203_a_hat16_layout.h"
#include "f203_a_hat16_ub.hpp"
#include "f203_alg7_g.hpp"
#include "f203_cbd_eta2.hpp"
#include "f203_keygen_prep_layout.h"
#include "f203_se_vector_prf.hpp"
#include "shake_general_tiling_data.h"

namespace F203KeygenPrep {

__aicore__ inline void StoreRhoToGm(__gm__ uint8_t *rho_gm, const uint8_t rho[32])
{
    AscendC::GlobalTensor<uint8_t> rhoOut;
    rhoOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho_gm), 32U);
    for (uint32_t i = 0U; i < 32U; ++i) {
        rhoOut.SetValue(i, rho[i]);
    }
}


#define F203_PREP_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

constexpr uint32_t kPrepShakeXUbBytes = F203SeVector::PRF_X_UB_BYTES;
constexpr uint32_t kPrepShakeLenUbBytes = F203SeVector::PRF_LEN_UB_BYTES;
constexpr uint32_t kPrepPrfYUbBytes = F203SeVector::PRF_Y_UB_BYTES;

/**
 * 行 3–15 全准备段：单 TPipe 内 Â → PRF → CBD。
 *
 * @param seed_d      Host 写入的 32 bit 种子（GM uint32[1]）
 * @param blockIdx    逻辑分片：0→poly 0–7，1→8–15（与 a_hat16 一致）；PRF/CBD 仅 block0 执行
 * @param a_hat_gm    输出 Â[16,256] int32，行主序
 * @param prf_out_gm  PRF 中间态 [8,128] uint8（block0 写入，供 CBD 读）
 * @param src_gm      输出 ŝ 秘密向量 [8,256] int32
 * @param tiling_gm   presample SHAKE batch tiling（host 填充）
 *
 * 双 AIV（F203_AHAT16_BLOCK_DIM=2）：
 * - block0/1 并行跑 BuildAHat16ShardWithUb 各 8 poly
 * - block0 独占 PRF+CBD；block1 空转 PRF 段但须在末尾 F203_PREP_PIPE_ALL 等待 block0
 * - 内嵌逐 poly SHAKE 经 ProcessInline，避免 block1 SHAKE 空转（见 shake_general.h）
 */
__aicore__ inline void BuildKeygenPrepSinglePipe(uint32_t seed_d, uint32_t blockIdx, __gm__ int32_t *a_hat_gm,
                                                 __gm__ uint8_t *prf_out_gm, __gm__ int32_t *src_gm, __gm__ uint8_t *rho_gm,
                                                 GM_ADDR tiling_gm)
{
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }

    uint8_t rho[32];
    uint8_t sigma[32];
    F203Alg7::BuildRhoSigmaFromSeedD(seed_d, rho, sigma);
    // block0 将设备侧 G(d||k) 的 ρ 落盘 GM，供行 21 ek‖ρ 设备拼接
    if (AscendC::GetBlockIdx() == 0U && rho_gm != nullptr) {
        StoreRhoToGm(rho_gm, rho);
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

    pipe.InitBuffer(shakeXBuf, kPrepShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kPrepShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(d2Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, kPrepPrfYUbBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    // 双 AIV：blockIdx 0→poly 0–7、1→8–15 并行写 GM（对照 pass 探针 block0 串行 workaround）
    // 背景：T13h；独立 a_hat 探针 SIM 并行已 PASS，融合 prep 需在 SIM 证 P-02/P-04（见 PIPE_SYNC_EVAL.md）
    F203Ahat16::BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                      d1Que, d2Que, aHatQue, scratchBuf);

    F203_PREP_PIPE_ALL();

    // block0 独占 PRF+CBD；block1 不得在 block0 完成前 return（SIM 上否则可能提前结束整核）
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
