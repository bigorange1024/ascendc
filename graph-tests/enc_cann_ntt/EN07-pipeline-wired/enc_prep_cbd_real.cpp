/**
 * EN05 · L1 Prep 真采样积木（独立 AIV launch，X32）
 *
 * 作用：PRF(SHAKE256, σ‖N) + FIPS 203 Alg.8 SamplePolyCBD_η=2，产出 Encrypt 形 y 向量。
 * 输入：sigmaGm [32] uint8（Host 已派生 σ；本刀不做 SampleNTT，Â 仍 Host 喂 Matvec）
 * 输出：dstGm [k*n] int32，行主序 poly0..poly{k-1}，每 poly 256 系数 ∈[0,q)
 * 布局：与 cann-ntt NTT src [bench=k, n] 同构；Host 可直接 memcpy 喂 L2（本刀 L2 仍独立造数对拍）。
 *
 * 实现来源：shared `shake_xof_kernel` + Alg.8/笔记契约重写；禁止抄 Encrypt prep / alg14 / ER / 探针整文件。
 * 同步：禁 CrossCore / GATE 4/8 / SoftSync（B1–B3）。
 */
#include "enc_aiv_stub_common.hpp"

#include "shake_ub_helpers.hpp"
#include "shake_general_tiling_data.h"

namespace {

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
 * @param dst    输出 y[k*n] int32（行主序）
 * @param sigma  输入 σ[32] uint8
 * @param k      poly 数（本刀锁 4）
 * @param n      系数数（锁 256）
 * @param q      模数（锁 3329）
 * @param nonce0 首个 PRF nonce；poly i 用 nonce0+i
 */
extern "C" __global__ __aicore__ void enc_prep_cbd_real(GM_ADDR dst, GM_ADDR sigma, int32_t k, int32_t n,
                                                        int32_t q, int32_t nonce0)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    // 已锁参数护栏（歧义时不擅自改参；仅早退避免坏 launch）
    if (k != 4 || n != 256 || q != kQ) {
        return;
    }
    const uint32_t kk = static_cast<uint32_t>(k);
    const uint32_t nn = static_cast<uint32_t>(n);
    const uint32_t outElems = kk * nn;

    AscendC::GlobalTensor<int32_t> gmOut;
    AscendC::GlobalTensor<uint8_t> gmSigma;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), outElems);
    gmSigma.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(sigma), kSigmaBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queSigma;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufShake;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufLen;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufPrf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufStaging;

    const uint32_t sigmaUb = ShakeXofUb::CeilAlign32(kSigmaBytes);
    const uint32_t xBytes = kk * kMsgRowBytes;
    const uint32_t yBytes = kk * kPrfBytes;
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

    uint8_t sigmaStack[kSigmaBytes];
    for (uint32_t i = 0; i < kSigmaBytes; ++i) {
        sigmaStack[i] = sigmaLocal.GetValue(i);
    }
    queSigma.FreeTensor(sigmaLocal);

    for (uint32_t row = 0; row < kk; ++row) {
        const uint8_t nonce = static_cast<uint8_t>(static_cast<uint32_t>(nonce0) + row);
        ShakeXofUb::FillShakeRowUb(sigmaStack, kSigmaBytes, nonce, xUb, row * kMsgRowBytes);
        lenUb.SetValue(row, kMsgLen);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    ShakeGeneralTilingData shakeTiling;
    ShakeXofUb::FillShakeTilingUb(shakeTiling, kk, kMsgRowBytes, kPrfBytes, kShake256Rate);
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, staging, &shakeTiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();
    for (uint32_t row = 0; row < kk; ++row) {
        SamplePolyCbdEta2Row(outLocal, row * nn, yUb, row * kPrfBytes, q);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    queOut.EnQue(outLocal);
    outLocal = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmOut, outLocal, outElems);
    queOut.FreeTensor(outLocal);
}
