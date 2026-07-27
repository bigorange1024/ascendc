// @probe exp-fips203-mlkem-pke-keygen-k2
// @file prep/presample/f203_se_vector_prf.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_vector_prf.hpp` 为该子模块组件。 / Component: f203_se_vector_prf.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_se_vector_g.hpp, shake_general.h, shake_general_tiling_data.h, shake_ub_helpers.hpp, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 行 8–15 PRF+CBD presample 链。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/presample/f203_se_vector_prf.hpp
 */
/**
 * @file f203_se_vector_prf.hpp
 * @brief Phase P：σ → 4× SHAKE256 PRF（单 TPipe UB shake，链末 DataCopy 对拍 prf_out GM）。
 */
#pragma once

#include "f203_se_vector_g.hpp"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include <cstdint>

namespace F203SeVector {

#define F203_SE_PRF_PIPE_ALL() ShakeXofUb::PipeAll()

constexpr uint32_t PRF_BATCH = 4U;
/** 有效 PRF 消息长度 σ‖N（FIPS 203）。 */
constexpr uint32_t PRF_MSG_LEN = 33U;
/**
 * UB x 行 stride：须 8B 对齐，供 shake_general XorBlock22 的 uint64 块读。
 * 背景：maxMsgLen=33 时 msgBase=33,66,99… 非 8B 对齐 → SIM pem_lsu 告警（2026-06-25 回归）。
 * lengths[i] 仍为 PRF_MSG_LEN；仅布局 stride 垫到 64。
 */
constexpr uint32_t PRF_MSG_STRIDE = ShakeXofUb::CeilAlign32(PRF_MSG_LEN);
constexpr uint32_t PRF_OUT_LEN = 192U;
constexpr uint32_t PRF_X_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * PRF_MSG_STRIDE);
constexpr uint32_t PRF_LEN_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t PRF_Y_UB_BYTES = ShakeXofUb::CeilAlign32(PRF_BATCH * PRF_OUT_LEN);

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

/** σ‖N 写入 UB x/lengths（batch=4，行优先）。 */
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

/** UB SHAKE batch → prf_out GM；yQue 须已 InitBuffer(1, PRF_Y_UB_BYTES)。 */
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

    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prf_out_gm, PRF_BATCH * PRF_OUT_LEN);
    AscendC::DataCopy(prfGm, yUb, PRF_BATCH * PRF_OUT_LEN);
    F203_SE_PRF_PIPE_ALL();
    yQue.FreeTensor(yUb);
}

/** 单 TPipe：UB SHAKE batch → prf_out GM。 */
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
 * 本函数为 KeyGen 流水线组件 `BuildPrfOutFromSeedD`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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
