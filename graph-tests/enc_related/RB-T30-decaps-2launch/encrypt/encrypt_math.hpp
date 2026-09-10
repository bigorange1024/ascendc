/**
 * @file encrypt_math.hpp
 * @brief RB-T30 Encaps/Re-Encrypt 设备数学（FIPS 203 Alg.14/16/17 外形）。
 *
 * 覆盖：G/H → μ → Decode₁₂ → CBD → SampleNTT → NTT(y) → Mul → INTT+噪 → pack→c'。
 * CBD/SampleNTT 接活跃探针积木 + shared SHAKE/Keccak（#include 共享库，非抄实验核）。
 * 业务写出 UB+DataCopy。
 */
#ifndef RB_T30_ENCRYPT_MATH_HPP
#define RB_T30_ENCRYPT_MATH_HPP

// SampleNTT / CBD 硬锁（单 AIV 写满）
#ifdef F203_AHAT16_BLOCK_DIM
#undef F203_AHAT16_BLOCK_DIM
#endif
#define F203_AHAT16_BLOCK_DIM 1
#ifdef F203_AHAT16_BATCH_SHAKE
#undef F203_AHAT16_BATCH_SHAKE
#endif
#define F203_AHAT16_BATCH_SHAKE 0
#ifdef F203_ALG7_REJ_IMPL
#undef F203_ALG7_REJ_IMPL
#endif
#define F203_ALG7_REJ_IMPL 1
#ifdef F203_ALG7_D12_GATHER
#undef F203_ALG7_D12_GATHER
#endif
#define F203_ALG7_D12_GATHER 0
#ifdef F203_ALG7_XOF_504
#undef F203_ALG7_XOF_504
#endif
#define F203_ALG7_XOF_504 0
#ifndef F203_CBD_BLOCK_DIM
#define F203_CBD_BLOCK_DIM 1
#endif

#include "f203_a_hat16_ub.hpp"
#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "f203_cbd_eta2.hpp"
#include "fips203_device_sha3.hpp"
#include "kernel_operator.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"
#include "tiling.h"
#include <cstdint>

namespace rb_t30 {

__aicore__ inline int32_t EncModQ(int32_t x)
{
    int32_t r = x % t30_enc::kQ;
    if (r < 0) {
        r += t30_enc::kQ;
    }
    return r;
}

// -------------------- Encaps 头 G/H --------------------

/**
 * (K‖r)←G(m‖H(ek))；h←SHA3-256(ek)。
 * @param mGm/ekGm [in]；hOut/kOut/coins [out]
 * 写出经标量缓冲后逐字节写 GM（头材料；后续 CBD 读 coins）。
 */
__aicore__ inline void RunEncapsHeadG(const __gm__ uint8_t *mGm, const __gm__ uint8_t *ekGm,
                                      __gm__ uint8_t *hOutGm, __gm__ uint8_t *kOutGm,
                                      __gm__ uint8_t *coinsGm)
{
    using namespace t30_enc;
    uint8_t ekUb[kEkBytes];
    for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
        ekUb[i] = ekGm[i];
    }
    uint8_t h[kHashBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashBytes), ekUb,
                                    static_cast<uint32_t>(kEkBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    uint8_t mh[kMsgBytes + kHashBytes];
    for (uint32_t i = 0; i < static_cast<uint32_t>(kMsgBytes); ++i) {
        mh[i] = mGm[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kHashBytes); ++i) {
        mh[static_cast<uint32_t>(kMsgBytes) + i] = h[i];
    }
    uint8_t gOut[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(gOut, static_cast<int>(kGOutBytes), mh,
                                    static_cast<uint32_t>(kMsgBytes + kHashBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    for (uint32_t i = 0; i < static_cast<uint32_t>(kHashBytes); ++i) {
        hOutGm[i] = h[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kSharedKeyBytes); ++i) {
        kOutGm[i] = gOut[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCoinsBytes); ++i) {
        coinsGm[i] = gOut[static_cast<uint32_t>(kSharedKeyBytes) + i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * prep 前缀：ek→OFF_EK（DataCopy）+ ρ 尾 + G；可选 μ/Decode/CBD/Â。
 * 背景：L2 把原 enc_prep 融进单 launch；ek 镜像用 DataCopy（X12）。
 */
__aicore__ inline void PrepEncapsPrefix(GM_ADDR ekIn, GM_ADDR ws)
{
    using namespace t30_enc;

    AscendC::GlobalTensor<uint8_t> gmEkIn;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmRho;
    gmEkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                           static_cast<uint32_t>(kEkBytes));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK),
                           static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    // ek=1536‖ρ32：分段 DataCopy（1536/32 均 32B 对齐；禁整段 1568）
    {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> qBody;
        pipe.InitBuffer(qBody, 1, static_cast<uint32_t>(kEkBodyBytes));
        AscendC::LocalTensor<uint8_t> body = qBody.AllocTensor<uint8_t>();
        AscendC::DataCopy(body, gmEkIn, static_cast<uint32_t>(kEkBodyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmEkWs, body, static_cast<uint32_t>(kEkBodyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        qBody.FreeTensor(body);
    }
    {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> qRho;
        pipe.InitBuffer(qRho, 1, 32);
        AscendC::LocalTensor<uint8_t> rhoLoc = qRho.AllocTensor<uint8_t>();
        AscendC::DataCopy(rhoLoc, gmEkIn[static_cast<uint32_t>(kEkBodyBytes)], 32);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmEkWs[static_cast<uint32_t>(kEkBodyBytes)], rhoLoc, 32);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmRho, rhoLoc, 32);
        AscendC::PipeBarrier<PIPE_ALL>();
        qRho.FreeTensor(rhoLoc);
    }

    const __gm__ uint8_t *mGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_M);
    const __gm__ uint8_t *ekGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_EK);
    __gm__ uint8_t *hGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_H);
    __gm__ uint8_t *kGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_K);
    __gm__ uint8_t *coinsGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_COINS);
    RunEncapsHeadG(mGm, ekGm, hGm, kGm, coinsGm);
}

// -------------------- μ / Decode₁₂ --------------------

/** μ ← Decompress₁∘ByteDecode₁(m)。 */
__aicore__ inline void EmbedMuFromMsg(GM_ADDR ws)
{
    using namespace t30_enc;
    AscendC::GlobalTensor<uint8_t> gmMsg;
    AscendC::GlobalTensor<int32_t> gmMu;
    gmMsg.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_M),
                          static_cast<uint32_t>(kMsgBytes));
    gmMu.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_MU),
                         static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kMsgBytes));
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));
    AscendC::LocalTensor<uint8_t> msgLocal = queIn.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> muLocal = queOut.AllocTensor<int32_t>();

    AscendC::DataCopy(msgLocal, gmMsg, static_cast<uint32_t>(kMsgBytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kPolyN; ++i) {
        const uint32_t byteIdx = static_cast<uint32_t>(i >> 3);
        const uint32_t bitOff = static_cast<uint32_t>(i & 7);
        const uint8_t b = msgLocal.GetValue(byteIdx);
        const int32_t bit = static_cast<int32_t>((b >> bitOff) & 1u);
        muLocal.SetValue(static_cast<uint32_t>(i), (bit * kQ + 1) >> 1);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMu, muLocal, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    queIn.FreeTensor(msgLocal);
    queOut.FreeTensor(muLocal);
}

/** t̂ ← ByteDecode₁₂(ek 体[1536])。 */
__aicore__ inline void DecodeTHatFromEk(GM_ADDR ws)
{
    using namespace t30_enc;
    AscendC::GlobalTensor<int32_t> gmOut;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_T_HAT),
                          static_cast<uint32_t>(kTHatCoeffs));
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();
    for (int32_t p = 0; p < kKem; ++p) {
        const uint32_t byteOff = static_cast<uint32_t>(p) * static_cast<uint32_t>(kPolyPackedBytes);
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK) + byteOff;
        f203_byte_codec::poly_byte_decode12_scalar_gm(outLocal, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmOut[static_cast<uint32_t>(p * kPolyN)], outLocal,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    queOut.FreeTensor(outLocal);
}

// -------------------- CBD（shared SHAKE + 探针 OneRow） --------------------

constexpr uint32_t kPrfMsgLen = 33U;
constexpr uint32_t kPrfMsgStride = ShakeXofUb::CeilAlign32(kPrfMsgLen);
constexpr uint32_t kPrfBatch = 9U;
constexpr uint32_t kPrfOutLen = 128U;
constexpr uint32_t kPrfXUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfMsgStride);
constexpr uint32_t kPrfLenUbBytes =
    ShakeXofUb::CeilAlign32(kPrfBatch * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t kPrfYUbBytes = ShakeXofUb::CeilAlign32(kPrfBatch * kPrfOutLen);

__aicore__ inline void Prf9FromCoins(const uint8_t coins[32], __gm__ uint8_t *prfOutGm)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> yQue;
    pipe.InitBuffer(xBuf, kPrfXUbBytes);
    pipe.InitBuffer(lenBuf, kPrfLenUbBytes);
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yQue, 1, kPrfYUbBytes);

    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lengthsUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();
    for (uint32_t nonce = 0U; nonce < kPrfBatch; ++nonce) {
        const uint32_t rowBase = nonce * kPrfMsgStride;
        ShakeXofUb::FillShakeRowUb(coins, 32U, static_cast<uint8_t>(nonce & 0xFFU), xUb, rowBase);
        lengthsUb.SetValue(nonce, kPrfMsgLen);
    }
    ShakeXofUb::PipeAll();

    ShakeGeneralTilingData td{};
    ShakeXofUb::FillShakeTilingUb(td, kPrfBatch, kPrfMsgStride, kPrfOutLen, SHAKE256_RATE_BYTES);
    td.blockDim = 1U;
    AscendC::LocalTensor<uint8_t> yUb = yQue.AllocTensor<uint8_t>();
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lengthsUb, yUb, stagingUb, &td);
    ShakeXofUb::PipeAll();

    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(prfOutGm, kPrfBatch * kPrfOutLen);
    AscendC::DataCopy(prfGm, yUb, kPrfBatch * kPrfOutLen);
    ShakeXofUb::PipeAll();
    yQue.FreeTensor(yUb);
}

__aicore__ inline void Cbd9FromPrf(__gm__ const uint8_t *prfGm, __gm__ int32_t *srcGm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, F203CbdEta2::PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(F203CbdEta2::N) * sizeof(int32_t));
    AscendC::GlobalTensor<uint8_t> prfTensor;
    AscendC::GlobalTensor<int32_t> srcTensor;
    prfTensor.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prfGm), kPrfBatch * F203CbdEta2::PRF_BYTES);
    srcTensor.SetGlobalBuffer(srcGm, kPrfBatch * F203CbdEta2::N);
    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();
    for (uint32_t row = 0U; row < kPrfBatch; ++row) {
        F203CbdEta2::SamplePolyCbd2OneRowUb(row, prfTensor, srcTensor, prfLocal, rowQue);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** coins→PRF→CBD → y‖e1‖e2。 */
__aicore__ inline void SampleNoiseCbd(GM_ADDR ws)
{
    using namespace t30_enc;
    uint8_t coins[32];
    const __gm__ uint8_t *coinsGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_COINS);
    for (uint32_t i = 0U; i < 32U; ++i) {
        coins[i] = coinsGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint8_t *prfGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_PRF);
    __gm__ int32_t *yeeGm = reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_E1_E2);
    Prf9FromCoins(coins, prfGm);
    AscendC::PipeBarrier<PIPE_ALL>();
    Cbd9FromPrf(prfGm, yeeGm);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** Â ← SampleNTT(ρ)（16 poly，BLOCK_DIM=1）。 */
__aicore__ inline void SampleAHatMatrix(GM_ADDR ws)
{
    using namespace t30_enc;
    uint8_t rho[F203Alg7::kRhoBytes];
    const __gm__ uint8_t *rhoGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_RHO);
    for (uint32_t i = 0U; i < F203Alg7::kRhoBytes; ++i) {
        rho[i] = rhoGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();

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

    __gm__ int32_t *aHatGm = reinterpret_cast<__gm__ int32_t *>(ws + OFF_A_HAT);
    F203Ahat16::BuildAHat16ShardWithUb(rho, aHatGm, 0U, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf,
                                       d1Que, d2Que, aHatQue, scratchBuf);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * SyncAll 前缀：G 已完成；再做 μ / Decode₁₂ / CBD / Â。
 * 结论：NTT 路径只剩 ŷ / Mul / INTT / pack。
 */
__aicore__ inline void PrepEncapsHeavy(GM_ADDR ws)
{
    EmbedMuFromMsg(ws);
    DecodeTHatFromEk(ws);
    SampleNoiseCbd(ws);
    SampleAHatMatrix(ws);
}

// -------------------- NTT(y) / Mul / INTT+噪 --------------------

__aicore__ inline void Alg9Fwd(AscendC::LocalTensor<int32_t> &f,
                               const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 1;
    for (int32_t len = 128; len >= 2; len /= 2) {
        for (int32_t start = 0; start < t30_enc::kPolyN; start += 2 * len) {
            const int32_t zeta = zetas.GetValue(static_cast<uint32_t>(zi));
            zi += 1;
            for (int32_t j = start; j < start + len; ++j) {
                const int32_t t = EncModQ(zeta * f.GetValue(static_cast<uint32_t>(j + len)));
                const int32_t uj = f.GetValue(static_cast<uint32_t>(j));
                f.SetValue(static_cast<uint32_t>(j + len), EncModQ(uj - t));
                f.SetValue(static_cast<uint32_t>(j), EncModQ(uj + t));
            }
        }
    }
}

__aicore__ inline void Alg10Inv(AscendC::LocalTensor<int32_t> &f,
                                const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 127;
    for (int32_t len = 2; len <= 128; len *= 2) {
        for (int32_t start = 0; start < t30_enc::kPolyN; start += 2 * len) {
            const int32_t zeta = EncModQ(-zetas.GetValue(static_cast<uint32_t>(zi)));
            zi -= 1;
            for (int32_t j = start; j < start + len; ++j) {
                const int32_t t = f.GetValue(static_cast<uint32_t>(j));
                const int32_t u = f.GetValue(static_cast<uint32_t>(j + len));
                f.SetValue(static_cast<uint32_t>(j), EncModQ(t + u));
                f.SetValue(static_cast<uint32_t>(j + len), EncModQ(zeta * (t - u)));
            }
        }
    }
    for (int32_t i = 0; i < t30_enc::kPolyN; ++i) {
        f.SetValue(static_cast<uint32_t>(i),
                   EncModQ(f.GetValue(static_cast<uint32_t>(i)) * t30_enc::kInttScale));
    }
}

__aicore__ inline void Alg11Mul(const AscendC::LocalTensor<int32_t> &f,
                                const AscendC::LocalTensor<int32_t> &g,
                                const AscendC::LocalTensor<int32_t> &gammas,
                                AscendC::LocalTensor<int32_t> &h)
{
    for (int32_t i = 0; i < t30_enc::kPolyN / 2; ++i) {
        const int32_t a0 = f.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t a1 = f.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t b0 = g.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t b1 = g.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t gamma = gammas.GetValue(static_cast<uint32_t>(i));
        const int32_t a1b1 = EncModQ(a1 * b1);
        h.SetValue(static_cast<uint32_t>(2 * i), EncModQ(a0 * b0 + a1b1 * gamma));
        h.SetValue(static_cast<uint32_t>(2 * i + 1), EncModQ(a0 * b1 + a1 * b0));
    }
}

__aicore__ inline void EncLoadPoly(AscendC::LocalTensor<int32_t> &dst,
                                   AscendC::GlobalTensor<int32_t> &srcGm, int32_t polyIdx)
{
    AscendC::DataCopy(dst, srcGm[static_cast<uint32_t>(polyIdx * t30_enc::kPolyN)],
                      static_cast<uint32_t>(t30_enc::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void EncStorePoly(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                    const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * t30_enc::kPolyN)], src,
                      static_cast<uint32_t>(t30_enc::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** ŷ ← NTT(y)。 */
__aicore__ inline void ForwardNttY(GM_ADDR ws)
{
    using namespace t30_enc;
    AscendC::GlobalTensor<int32_t> gmY;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmYHat;
    gmY.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS));
    gmYHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_HAT));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qPoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qZeta;
    pipe.InitBuffer(qPoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(qZeta, 1, static_cast<uint32_t>(kZetasBytes));
    AscendC::LocalTensor<int32_t> zetas = qZeta.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::LocalTensor<int32_t> poly = qPoly.AllocTensor<int32_t>();
    for (int32_t p = 0; p < kKem; ++p) {
        EncLoadPoly(poly, gmY, p);
        Alg9Fwd(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        EncStorePoly(gmYHat, p, poly);
    }
    qPoly.FreeTensor(poly);
    qZeta.FreeTensor(zetas);
}

/** û = Âᵀ∘ŷ；v̂ = ⟨t̂,ŷ⟩。 */
__aicore__ inline void AssembleUvNttDomain(GM_ADDR ws)
{
    using namespace t30_enc;
    AscendC::GlobalTensor<int32_t> gmA, gmY, gmT, gmGammas, gmUHat, gmVHat;
    gmA.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_A_HAT),
                        static_cast<uint32_t>(kKem * kKem * kPolyN));
    gmY.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_HAT),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmT.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_T_HAT),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmGammas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_GAMMAS),
                             static_cast<uint32_t>(kGammaN));
    gmUHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmVHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V_HAT),
                           static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queF, queG, queGamma;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queProd, queAcc;
    pipe.InitBuffer(queF, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queG, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queProd, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queAcc, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queGamma, 1, static_cast<uint32_t>(kGammasBytes));
    AscendC::LocalTensor<int32_t> f = queF.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> g = queG.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> prod = queProd.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> acc = queAcc.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> gammas = queGamma.AllocTensor<int32_t>();
    AscendC::DataCopy(gammas, gmGammas, static_cast<uint32_t>(kGammaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    for (int32_t i = 0; i < kKem; ++i) {
        for (int32_t c = 0; c < kPolyN; ++c) {
            acc.SetValue(static_cast<uint32_t>(c), 0);
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t j = 0; j < kKem; ++j) {
            const uint32_t aOff = static_cast<uint32_t>((j * kKem + i) * kPolyN);
            AscendC::DataCopy(f, gmA[aOff], static_cast<uint32_t>(kPolyN));
            AscendC::PipeBarrier<PIPE_ALL>();
            EncLoadPoly(g, gmY, j);
            Alg11Mul(f, g, gammas, prod);
            AscendC::PipeBarrier<PIPE_ALL>();
            for (int32_t c = 0; c < kPolyN; ++c) {
                acc.SetValue(static_cast<uint32_t>(c),
                             EncModQ(acc.GetValue(static_cast<uint32_t>(c)) +
                                     prod.GetValue(static_cast<uint32_t>(c))));
            }
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        EncStorePoly(gmUHat, i, acc);
    }

    for (int32_t c = 0; c < kPolyN; ++c) {
        acc.SetValue(static_cast<uint32_t>(c), 0);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kKem; ++i) {
        EncLoadPoly(f, gmT, i);
        EncLoadPoly(g, gmY, i);
        Alg11Mul(f, g, gammas, prod);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t c = 0; c < kPolyN; ++c) {
            acc.SetValue(static_cast<uint32_t>(c),
                         EncModQ(acc.GetValue(static_cast<uint32_t>(c)) +
                                 prod.GetValue(static_cast<uint32_t>(c))));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    AscendC::DataCopy(gmVHat, acc, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    queF.FreeTensor(f);
    queG.FreeTensor(g);
    queProd.FreeTensor(prod);
    queAcc.FreeTensor(acc);
    queGamma.FreeTensor(gammas);
}

/** u = INTT(û)+e₁；v = INTT(v̂)+e₂+μ。 */
__aicore__ inline void InttAddNoiseUv(GM_ADDR ws)
{
    using namespace t30_enc;
    AscendC::GlobalTensor<int32_t> gmUHat, gmVHat, gmE1, gmE2, gmMu, gmZetas, gmU, gmV;
    gmUHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmVHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V_HAT),
                           static_cast<uint32_t>(kPolyN));
    gmE1.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_E1),
                         static_cast<uint32_t>(kKem * kPolyN));
    gmE2.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_E2),
                         static_cast<uint32_t>(kPolyN));
    gmMu.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_MU),
                         static_cast<uint32_t>(kPolyN));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS),
                            static_cast<uint32_t>(kZetaN));
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly, queNoise, queZeta, queMu;
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queNoise, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queZeta, 1, static_cast<uint32_t>(kZetasBytes));
    pipe.InitBuffer(queMu, 1, static_cast<uint32_t>(kPolyBytes));
    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> noise = queNoise.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> zetas = queZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> muLoc = queMu.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(muLoc, gmMu, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    for (int32_t i = 0; i < kKem; ++i) {
        EncLoadPoly(poly, gmUHat, i);
        Alg10Inv(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        EncLoadPoly(noise, gmE1, i);
        for (int32_t c = 0; c < kPolyN; ++c) {
            poly.SetValue(static_cast<uint32_t>(c),
                          EncModQ(poly.GetValue(static_cast<uint32_t>(c)) +
                                  noise.GetValue(static_cast<uint32_t>(c))));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        EncStorePoly(gmU, i, poly);
    }

    AscendC::DataCopy(poly, gmVHat, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    Alg10Inv(poly, zetas);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(noise, gmE2, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t c = 0; c < kPolyN; ++c) {
        poly.SetValue(static_cast<uint32_t>(c),
                      EncModQ(poly.GetValue(static_cast<uint32_t>(c)) +
                              noise.GetValue(static_cast<uint32_t>(c)) +
                              muLoc.GetValue(static_cast<uint32_t>(c))));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmV, poly, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    quePoly.FreeTensor(poly);
    queNoise.FreeTensor(noise);
    queZeta.FreeTensor(zetas);
    queMu.FreeTensor(muLoc);
}

// -------------------- Compress / ByteEncode / pack --------------------

__aicore__ inline int32_t Compress5(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(t30_enc::kQ)) {
        x = static_cast<uint32_t>(t30_enc::kQ) - 1u;
    }
    const uint32_t d0 = x * 1290176u;
    return static_cast<int32_t>(((d0 + (1u << 26)) >> 27) & 0x1fu);
}

__aicore__ inline int32_t Compress11(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(t30_enc::kQ)) {
        x = static_cast<uint32_t>(t30_enc::kQ) - 1u;
    }
    uint64_t d0 = static_cast<uint64_t>(x) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<int32_t>(d0 & 0x7ffu);
}

__aicore__ inline void ByteEncode5(AscendC::LocalTensor<uint8_t> &out,
                                   const AscendC::LocalTensor<int32_t> &in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = static_cast<uint8_t>(in.GetValue(static_cast<uint32_t>(8 * i + j)) & 0x1F);
        }
        const uint32_t base = static_cast<uint32_t>(i * 5);
        out.SetValue(base + 0, static_cast<uint8_t>(0xFF & ((t[0] >> 0) | (t[1] << 5))));
        out.SetValue(base + 1, static_cast<uint8_t>(0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))));
        out.SetValue(base + 2, static_cast<uint8_t>(0xFF & ((t[3] >> 1) | (t[4] << 4))));
        out.SetValue(base + 3, static_cast<uint8_t>(0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))));
        out.SetValue(base + 4, static_cast<uint8_t>(0xFF & ((t[6] >> 2) | (t[7] << 3))));
    }
}

__aicore__ inline void ByteEncode11(AscendC::LocalTensor<uint8_t> &out,
                                    const AscendC::LocalTensor<int32_t> &in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        uint16_t t[8];
        for (int32_t k = 0; k < 8; ++k) {
            t[k] = static_cast<uint16_t>(in.GetValue(static_cast<uint32_t>(8 * j + k)) & 0x7FF);
        }
        const uint32_t base = static_cast<uint32_t>(11 * j);
        out.SetValue(base + 0, static_cast<uint8_t>((t[0] >> 0) & 0xFF));
        out.SetValue(base + 1, static_cast<uint8_t>((t[0] >> 8) | ((t[1] << 3) & 0xFF)));
        out.SetValue(base + 2, static_cast<uint8_t>((t[1] >> 5) | ((t[2] << 6) & 0xFF)));
        out.SetValue(base + 3, static_cast<uint8_t>((t[2] >> 2) & 0xFF));
        out.SetValue(base + 4, static_cast<uint8_t>((t[2] >> 10) | ((t[3] << 1) & 0xFF)));
        out.SetValue(base + 5, static_cast<uint8_t>((t[3] >> 7) | ((t[4] << 4) & 0xFF)));
        out.SetValue(base + 6, static_cast<uint8_t>((t[4] >> 4) | ((t[5] << 7) & 0xFF)));
        out.SetValue(base + 7, static_cast<uint8_t>((t[5] >> 1) & 0xFF));
        out.SetValue(base + 8, static_cast<uint8_t>((t[5] >> 9) | ((t[6] << 2) & 0xFF)));
        out.SetValue(base + 9, static_cast<uint8_t>((t[6] >> 6) | ((t[7] << 5) & 0xFF)));
        out.SetValue(base + 10, static_cast<uint8_t>(t[7] >> 3));
    }
}

/** u,v → Compress₁₁/₅ + ByteEncode → c[1568]（ws + cOut）。 */
__aicore__ inline void PackCiphertext(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cWs, GM_ADDR cOut)
{
    using namespace t30_enc;
    constexpr int32_t kDu = 11;
    constexpr int32_t kDv = 5;
    constexpr int32_t kC1PolyBytes = (kPolyN * kDu) / 8;
    constexpr int32_t kC2PolyBytes = (kPolyN * kDv) / 8;
    constexpr int32_t kC1Bytes = kK * kC1PolyBytes;
    constexpr int32_t kC2Bytes = kC2PolyBytes;

    AscendC::GlobalTensor<int32_t> gmU, gmV;
    AscendC::GlobalTensor<uint8_t> gmCWs, gmCOut;
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uGm), static_cast<uint32_t>(kK * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vGm), static_cast<uint32_t>(kPolyN));
    gmCWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cWs), static_cast<uint32_t>(kCBytes));
    gmCOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cOut), static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queComp, queBytes;
    pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queComp, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queBytes, 1, static_cast<uint32_t>(kC1PolyBytes));
    AscendC::LocalTensor<int32_t> inLocal = queIn.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> compLocal = queComp.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> encLocal = queBytes.AllocTensor<uint8_t>();

    for (int32_t p = 0; p < kK; ++p) {
        AscendC::DataCopy(inLocal, gmU[static_cast<uint32_t>(p * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            compLocal.SetValue(static_cast<uint32_t>(i),
                               Compress11(inLocal.GetValue(static_cast<uint32_t>(i))));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        ByteEncode11(encLocal, compLocal, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        const uint32_t off = static_cast<uint32_t>(p * kC1PolyBytes);
        AscendC::DataCopy(gmCWs[off], encLocal, static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmCOut[off], encLocal, static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    AscendC::DataCopy(inLocal, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kPolyN; ++i) {
        compLocal.SetValue(static_cast<uint32_t>(i),
                           Compress5(inLocal.GetValue(static_cast<uint32_t>(i))));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    ByteEncode5(encLocal, compLocal, kPolyN);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmCWs[static_cast<uint32_t>(kC1Bytes)], encLocal,
                      static_cast<uint32_t>(kC2Bytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmCOut[static_cast<uint32_t>(kC1Bytes)], encLocal,
                      static_cast<uint32_t>(kC2Bytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    queIn.FreeTensor(inLocal);
    queComp.FreeTensor(compLocal);
    queBytes.FreeTensor(encLocal);
}

} // namespace rb_t30

#endif
