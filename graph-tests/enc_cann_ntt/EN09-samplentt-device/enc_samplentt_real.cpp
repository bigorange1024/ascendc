/**
 * EN09 · Alg.7 SampleNTT 设备积木（独立 AIV launch，X32）
 *
 * 作用：对 k=4 的 Â[k][k] 全矩阵（16 poly）逐格 SampleNTT → â[256]。
 * 输入：rhoGm [32] uint8（Host：G(d‖byte(k)) 前 32B = ρ；本核不做 Phase G）
 * 输出：aHatGm [k·k·n] int32，flat(i,j,c)=(i·k+j)·n+c；与 Matvec 行主序同构。
 *
 * 契约（笔记 F203-Alg7-SampleNTT）：
 *   B=ρ‖byte(j)‖byte(i) → SHAKE128 固定 squeeze 672B → 224×(d1,d2) → rej 前 256 个 d<q。
 * 实现：shared `shake_xof_kernel` + 标量解交织/rej（生产默认路径；禁向量 compact 未关门）。
 * 禁抄：Encrypt prep / alg7 探针整文件 / ER 核；禁 GATE 4/8 / CrossCore / SoftSync。
 */
#include "enc_aiv_stub_common.hpp"

#include "shake_ub_helpers.hpp"
#include "shake_general_tiling_data.h"

namespace {

constexpr int32_t kQ = 3329;
constexpr uint32_t kRhoBytes = 32U;
/** ρ‖j‖i = 34B；Alg.7 absorb 消息。 */
constexpr uint32_t kMsgLen = 34U;
constexpr uint32_t kMsgRowBytes = ShakeXofUb::CeilAlign32(kMsgLen);
constexpr uint32_t kShake128Rate = 168U;
/** 固定 4×rate = 672B（504+168；不做 lazy tail while）。 */
constexpr uint32_t kXofBytes = 672U;
constexpr uint32_t kCandPairs = kXofBytes / 3U; // 224
constexpr uint32_t kN = 256U;

/**
 * Alg.7 line 6–7 + line 8–15（标量）：672B XOF → â[256]。
 * 三字节 (c0,c1,c2) → (d1,d2)；按 d1 后 d2 扫描，接受 v<q 直至满 256。
 */
__aicore__ inline void SampleNttOnePolyFromXof(AscendC::LocalTensor<int32_t> &aHat,
                                               uint32_t aBase,
                                               AscendC::LocalTensor<uint8_t> &xof,
                                               uint32_t xofBase, int32_t q)
{
    uint32_t filled = 0U;
    for (uint32_t t = 0; t < kCandPairs && filled < kN; ++t) {
        const uint32_t pos = xofBase + 3U * t;
        const uint32_t c0 = static_cast<uint32_t>(xof.GetValue(pos + 0U));
        const uint32_t c1 = static_cast<uint32_t>(xof.GetValue(pos + 1U));
        const uint32_t c2 = static_cast<uint32_t>(xof.GetValue(pos + 2U));
        const int32_t d1 = static_cast<int32_t>(c0 + 256U * (c1 & 0x0FU));
        const int32_t d2 = static_cast<int32_t>((c1 >> 4) + 16U * c2);
        if (d1 < q && filled < kN) {
            aHat.SetValue(aBase + filled, d1);
            ++filled;
        }
        if (d2 < q && filled < kN) {
            aHat.SetValue(aBase + filled, d2);
            ++filled;
        }
    }
    // 672B 统计上应凑满 256；不足时剩余写 0（不挂；golden 侧会 soft-fail）
    while (filled < kN) {
        aHat.SetValue(aBase + filled, 0);
        ++filled;
    }
}

}  // namespace

/**
 * @param a_hat 输出 Â[k·k·n] int32（行主序 i 外、j 内）
 * @param rho   输入 ρ[32] uint8
 * @param k     矩阵阶（锁 4）
 * @param n     系数数（锁 256）
 * @param q     模数（锁 3329）
 */
extern "C" __global__ __aicore__ void enc_samplentt_real(GM_ADDR a_hat, GM_ADDR rho, int32_t k,
                                                         int32_t n, int32_t q)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    // 已锁参数护栏：非法则早退（不挂、不擅自改参）
    if (k != 4 || n != 256 || q != kQ) {
        return;
    }
    const uint32_t kk = static_cast<uint32_t>(k);
    const uint32_t nn = static_cast<uint32_t>(n);
    const uint32_t outElems = kk * kk * nn;

    AscendC::GlobalTensor<int32_t> gmOut;
    AscendC::GlobalTensor<uint8_t> gmRho;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(a_hat), outElems);
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho), kRhoBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queRho;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufShake;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufLen;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufXof;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufStaging;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufPoly;

    const uint32_t rhoUb = ShakeXofUb::CeilAlign32(kRhoBytes);
    // 单 poly 一轮：batch=1，控 UB；k×k 外循环串行（主目标不挂）
    const uint32_t xBytes = kMsgRowBytes;
    const uint32_t yBytes = kXofBytes;
    const uint32_t lenBytes = ShakeXofUb::CeilAlign32(static_cast<uint32_t>(sizeof(uint32_t)));
    const uint32_t polyBytes = nn * static_cast<uint32_t>(sizeof(int32_t));

    pipe.InitBuffer(queRho, 1, rhoUb);
    pipe.InitBuffer(queOut, 1, polyBytes);
    pipe.InitBuffer(bufShake, xBytes);
    pipe.InitBuffer(bufLen, lenBytes);
    pipe.InitBuffer(bufXof, yBytes);
    pipe.InitBuffer(bufStaging, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(bufPoly, polyBytes);

    // GM ρ → UB → 栈副本（后续 FillShakeRow 读）
    AscendC::LocalTensor<uint8_t> rhoLocal = queRho.AllocTensor<uint8_t>();
    AscendC::DataCopy(rhoLocal, gmRho, rhoUb);
    queRho.EnQue(rhoLocal);
    rhoLocal = queRho.DeQue<uint8_t>();

    uint8_t rhoStack[kRhoBytes];
    for (uint32_t i = 0; i < kRhoBytes; ++i) {
        rhoStack[i] = rhoLocal.GetValue(i);
    }
    queRho.FreeTensor(rhoLocal);

    AscendC::LocalTensor<uint8_t> xUb = bufShake.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = bufLen.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> yUb = bufXof.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> staging = bufStaging.Get<uint8_t>();
    AscendC::LocalTensor<int32_t> polyUb = bufPoly.Get<int32_t>();

    ShakeGeneralTilingData shakeTiling;
    ShakeXofUb::FillShakeTilingUb(shakeTiling, /*batch=*/1U, kMsgRowBytes, kXofBytes, kShake128Rate);

    // FIPS：Â[i,j] ← SampleNTT(ρ‖byte(j)‖byte(i))
    for (uint32_t rowI = 0; rowI < kk; ++rowI) {
        for (uint32_t colJ = 0; colJ < kk; ++colJ) {
            for (uint32_t z = 0; z < xBytes; ++z) {
                xUb.SetValue(z, static_cast<uint8_t>(0));
            }
            AscendC::PipeBarrier<PIPE_ALL>();

            // prefix = ρ[32]‖j（33B），tail = i → 消息 34B
            uint8_t prefix33[33];
            for (uint32_t b = 0; b < kRhoBytes; ++b) {
                prefix33[b] = rhoStack[b];
            }
            prefix33[32] = static_cast<uint8_t>(colJ);
            ShakeXofUb::FillShakeRowUb(prefix33, 33U, static_cast<uint8_t>(rowI), xUb, 0U);
            lenUb.SetValue(0, kMsgLen);
            AscendC::PipeBarrier<PIPE_ALL>();

            ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, staging, &shakeTiling);
            AscendC::PipeBarrier<PIPE_ALL>();

            SampleNttOnePolyFromXof(polyUb, 0U, yUb, 0U, q);
            AscendC::PipeBarrier<PIPE_ALL>();

            const uint32_t gmOff = (rowI * kk + colJ) * nn;
            AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();
            AscendC::DataCopy(outLocal, polyUb, nn);
            AscendC::PipeBarrier<PIPE_V>();
            queOut.EnQue(outLocal);
            outLocal = queOut.DeQue<int32_t>();
            AscendC::DataCopy(gmOut[gmOff], outLocal, nn);
            queOut.FreeTensor(outLocal);
        }
    }
}
