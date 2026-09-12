/**
 * EN15 · L1 prep 融合核（1 Host launch）
 *
 * 作用：同核串级完成 SampleNTT(ρ)→Â 与 Prep CBD(coins)→y。
 * 输入：rho[32]、sigma/coins[32]；输出：a_hat[K·K·N]、y[K·N]。
 * Host mid-sync（L1→L2）：Â 转置为 Âᵀ、上传 t̂/gammas/e1/e2/μ/M4。
 * CPU=AIV_ONLY；SIM=MIX 占位（AIC 立即 return）。禁 GATE 4/8。
 */
#include "enc_aiv_stub_common.hpp"
#include "shake_ub_helpers.hpp"
#include "shake_general_tiling_data.h"

namespace enc_sample_ns {

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

namespace enc_cbd_ns {

constexpr int32_t kQ = 3329;
constexpr uint32_t kSigmaBytes = 32U;
constexpr uint32_t kEta = 2U;
/** η·n/4 = 128：Alg.8 η=2 单 poly PRF 字节数。 */
constexpr uint32_t kPrfBytes = (kEta * 256U) / 4U;
constexpr uint32_t kShake256Rate = 136U;
/** σ‖nonce 共 33B；UB 行宽 32B 对齐。 */
constexpr uint32_t kMsgLen = 33U;
constexpr uint32_t kMsgRowBytes = ShakeXofUb::CeilAlign32(kMsgLen);

/**
 * Alg.8 SamplePolyCBD_η=2：PRF 行 → 256 个系数（负差 +q，∈[0,q)）。
 * 位抽取对齐 FIPS / shared golden_se_sampling.sample_poly_cbd2（非探针核抄码）。
 */
__aicore__ inline void SamplePolyCbdEta2Row(AscendC::LocalTensor<int32_t> &dst, uint32_t dstBase,
                                            AscendC::LocalTensor<uint8_t> &prf, uint32_t prfBase,
                                            int32_t q)
{
    for (uint32_t i = 0; i < 32U; ++i) {
        const uint32_t b0 = static_cast<uint32_t>(prf.GetValue(prfBase + 4U * i + 0U));
        const uint32_t b1 = static_cast<uint32_t>(prf.GetValue(prfBase + 4U * i + 1U));
        const uint32_t b2 = static_cast<uint32_t>(prf.GetValue(prfBase + 4U * i + 2U));
        const uint32_t b3 = static_cast<uint32_t>(prf.GetValue(prfBase + 4U * i + 3U));
        const uint32_t t = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (uint32_t j = 0; j < 8U; ++j) {
            const int32_t a = static_cast<int32_t>((d >> (4U * j + 0U)) & 0x3U);
            const int32_t b = static_cast<int32_t>((d >> (4U * j + 2U)) & 0x3U);
            int32_t c = a - b;
            while (c < 0) {
                c += q;
            }
            c %= q;
            dst.SetValue(dstBase + 8U * i + j, c);
        }
    }
}

}  // namespace

/**
 * L1 入口：采样 Â + CBD(y)。
 */
extern "C" __global__ __aicore__ void enc_prep_l1(GM_ADDR a_hat, GM_ADDR y_out, GM_ADDR rho,
                                                  GM_ADDR sigma, int32_t k, int32_t n, int32_t q,
                                                  int32_t nonce0)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    // -------- SampleNTT(ρ)→Â --------
    {

// 已锁参数护栏：非法则早退（不挂、不擅自改参）
    if (k != 4 || n != 256 || q != enc_sample_ns::kQ) {
        return;
    }
    const uint32_t kk = static_cast<uint32_t>(k);
    const uint32_t nn = static_cast<uint32_t>(n);
    const uint32_t outElems = kk * kk * nn;

    AscendC::GlobalTensor<int32_t> gmOut;
    AscendC::GlobalTensor<uint8_t> gmRho;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(a_hat), outElems);
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho), enc_sample_ns::kRhoBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queRho;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufShake;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufLen;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufXof;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufStaging;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufPoly;

    const uint32_t rhoUb = ShakeXofUb::CeilAlign32(enc_sample_ns::kRhoBytes);
    // 单 poly 一轮：batch=1，控 UB；k×k 外循环串行（主目标不挂）
    const uint32_t xBytes = enc_sample_ns::kMsgRowBytes;
    const uint32_t yBytes = enc_sample_ns::kXofBytes;
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

    uint8_t rhoStack[enc_sample_ns::kRhoBytes];
    for (uint32_t i = 0; i < enc_sample_ns::kRhoBytes; ++i) {
        rhoStack[i] = rhoLocal.GetValue(i);
    }
    queRho.FreeTensor(rhoLocal);

    AscendC::LocalTensor<uint8_t> xUb = bufShake.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = bufLen.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> yUb = bufXof.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> staging = bufStaging.Get<uint8_t>();
    AscendC::LocalTensor<int32_t> polyUb = bufPoly.Get<int32_t>();

    ShakeGeneralTilingData shakeTiling;
    ShakeXofUb::FillShakeTilingUb(shakeTiling, /*batch=*/1U, enc_sample_ns::kMsgRowBytes, enc_sample_ns::kXofBytes, enc_sample_ns::kShake128Rate);

    // FIPS：Â[i,j] ← SampleNTT(ρ‖byte(j)‖byte(i))
    for (uint32_t rowI = 0; rowI < kk; ++rowI) {
        for (uint32_t colJ = 0; colJ < kk; ++colJ) {
            for (uint32_t z = 0; z < xBytes; ++z) {
                xUb.SetValue(z, static_cast<uint8_t>(0));
            }
            AscendC::PipeBarrier<PIPE_ALL>();

            // prefix = ρ[32]‖j（33B），tail = i → 消息 34B
            uint8_t prefix33[33];
            for (uint32_t b = 0; b < enc_sample_ns::kRhoBytes; ++b) {
                prefix33[b] = rhoStack[b];
            }
            prefix33[32] = static_cast<uint8_t>(colJ);
            ShakeXofUb::FillShakeRowUb(prefix33, 33U, static_cast<uint8_t>(rowI), xUb, 0U);
            lenUb.SetValue(0, enc_sample_ns::kMsgLen);
            AscendC::PipeBarrier<PIPE_ALL>();

            ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, staging, &shakeTiling);
            AscendC::PipeBarrier<PIPE_ALL>();

            enc_sample_ns::SampleNttOnePolyFromXof(polyUb, 0U, yUb, 0U, q);
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

    // -------- Prep CBD(σ)→y --------
    {
        GM_ADDR dst = y_out;

// 已锁参数护栏（歧义时不擅自改参；仅早退避免坏 launch）
    if (k != 4 || n != 256 || q != enc_cbd_ns::kQ) {
        return;
    }
    const uint32_t kk = static_cast<uint32_t>(k);
    const uint32_t nn = static_cast<uint32_t>(n);
    const uint32_t outElems = kk * nn;

    AscendC::GlobalTensor<int32_t> gmOut;
    AscendC::GlobalTensor<uint8_t> gmSigma;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), outElems);
    gmSigma.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(sigma), enc_cbd_ns::kSigmaBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queSigma;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufShake;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufLen;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufPrf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufStaging;

    const uint32_t sigmaUb = ShakeXofUb::CeilAlign32(enc_cbd_ns::kSigmaBytes);
    const uint32_t xBytes = kk * enc_cbd_ns::kMsgRowBytes;
    const uint32_t yBytes = kk * enc_cbd_ns::kPrfBytes;
    const uint32_t lenBytes = ShakeXofUb::CeilAlign32(kk * static_cast<uint32_t>(sizeof(uint32_t)));
    const uint32_t coeffBytes = outElems * static_cast<uint32_t>(sizeof(int32_t));

    pipe.InitBuffer(queSigma, 1, sigmaUb);
    pipe.InitBuffer(queOut, 1, coeffBytes);
    pipe.InitBuffer(bufShake, xBytes);
    pipe.InitBuffer(bufLen, lenBytes);
    pipe.InitBuffer(bufPrf, yBytes);
    pipe.InitBuffer(bufStaging, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);

    // GM σ → UB
    AscendC::LocalTensor<uint8_t> sigmaLocal = queSigma.AllocTensor<uint8_t>();
    AscendC::DataCopy(sigmaLocal, gmSigma, sigmaUb);
    queSigma.EnQue(sigmaLocal);
    sigmaLocal = queSigma.DeQue<uint8_t>();

    AscendC::LocalTensor<uint8_t> xUb = bufShake.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = bufLen.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> yUb = bufPrf.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> staging = bufStaging.Get<uint8_t>();

    for (uint32_t i = 0; i < xBytes; ++i) {
        xUb.SetValue(i, static_cast<uint8_t>(0));
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    uint8_t sigmaStack[enc_cbd_ns::kSigmaBytes];
    for (uint32_t i = 0; i < enc_cbd_ns::kSigmaBytes; ++i) {
        sigmaStack[i] = sigmaLocal.GetValue(i);
    }
    queSigma.FreeTensor(sigmaLocal);

    for (uint32_t row = 0; row < kk; ++row) {
        const uint8_t nonce = static_cast<uint8_t>(static_cast<uint32_t>(nonce0) + row);
        ShakeXofUb::FillShakeRowUb(sigmaStack, enc_cbd_ns::kSigmaBytes, nonce, xUb, row * enc_cbd_ns::kMsgRowBytes);
        lenUb.SetValue(row, enc_cbd_ns::kMsgLen);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    ShakeGeneralTilingData shakeTiling;
    ShakeXofUb::FillShakeTilingUb(shakeTiling, kk, enc_cbd_ns::kMsgRowBytes, enc_cbd_ns::kPrfBytes, enc_cbd_ns::kShake256Rate);
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, staging, &shakeTiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();
    for (uint32_t row = 0; row < kk; ++row) {
        enc_cbd_ns::SamplePolyCbdEta2Row(outLocal, row * nn, yUb, row * enc_cbd_ns::kPrfBytes, q);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    queOut.EnQue(outLocal);
    outLocal = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmOut, outLocal, outElems);
    queOut.FreeTensor(outLocal);

    }
}
