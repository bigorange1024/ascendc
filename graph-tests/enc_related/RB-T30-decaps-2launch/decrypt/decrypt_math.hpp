/**
 * @file decrypt_math.hpp
 * @brief RB-T30 Decrypt 侧设备数学（FIPS 203 Alg.15）：unpack → NTT+⟨ŝ,û⟩ → INTT+Compress₁→m'。
 *
 * 本文件在流水线中的位置：L1 融合核 AIV0 调用；与 golden 仅 I/O 对拍。
 * 业务写出一律 UB + DataCopy；禁 GM SetValue 写 ŝ/u/v/û/ŵ/w/m。
 * 自研按 FIPS 重写；可 #include 共享 ByteDecode₁₂。
 */
#ifndef RB_T30_DECRYPT_MATH_HPP
#define RB_T30_DECRYPT_MATH_HPP

#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t30 {

// ========================= 公共模约 =========================

__aicore__ inline int32_t DecModQ(int32_t x)
{
    int32_t r = x % t30_dec::kQ;
    if (r < 0) {
        r += t30_dec::kQ;
    }
    return r;
}

/** UB 字节流第 bitIdx 位（字节内 LSB-first）。 */
__aicore__ inline int32_t BitLsbFromUb(const AscendC::LocalTensor<uint8_t> &bytes, int32_t bitIdx)
{
    const uint32_t byteI = static_cast<uint32_t>(bitIdx / 8);
    const uint32_t bitI = static_cast<uint32_t>(bitIdx % 8);
    return static_cast<int32_t>((bytes.GetValue(byteI) >> bitI) & 1u);
}

/**
 * FIPS 203 Alg.6 ByteDecode_d：UB 上 B^{32d} → Z_{2^d}^{256}。
 * @param coeffs [out] 256 系数
 * @param bytes  [in]  已在 UB 的打包字节
 * @param d      位宽（11 或 5）
 */
__aicore__ inline void DecodeBitsD(AscendC::LocalTensor<int32_t> &coeffs,
                                   const AscendC::LocalTensor<uint8_t> &bytes, int32_t d)
{
    for (int32_t i = 0; i < t30_dec::kPolyN; ++i) {
        int32_t v = 0;
        for (int32_t j = 0; j < d; ++j) {
            v |= BitLsbFromUb(bytes, i * d + j) << j;
        }
        coeffs.SetValue(static_cast<uint32_t>(i), v);
    }
}

/** Decompress_d：(c·q + 2^{d-1}) >> d。 */
__aicore__ inline int32_t DecompressBits(int32_t c, int32_t d)
{
    const int32_t bias = 1 << (d - 1);
    return (c * t30_dec::kQ + bias) >> d;
}

/**
 * Compress₁：统一整数式 (C·u + 2^{35}) >> 36 & 1；C=41285357。
 * 背景：与 liboqs / 本仓统一整数总结一致；未采用学校式 (2x+q/2)/q。
 */
__aicore__ inline int32_t CompressBit1(int32_t u)
{
    const int64_t s =
        t30_dec::kCompressC * static_cast<int64_t>(u) + t30_dec::kCompress1Bias;
    return static_cast<int32_t>(s >> t30_dec::kCompress1Shift) & 1;
}

/** FIPS 203 Alg.9 正向 NTT（就地；poly-batch 整 poly；禁 Gather）。 */
__aicore__ inline void Alg9ForwardNtt(AscendC::LocalTensor<int32_t> &f,
                                      const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 1;
    for (int32_t len = 128; len >= 2; len /= 2) {
        for (int32_t start = 0; start < t30_dec::kPolyN; start += 2 * len) {
            const int32_t zeta = zetas.GetValue(static_cast<uint32_t>(zi));
            zi += 1;
            for (int32_t j = start; j < start + len; ++j) {
                const int32_t t = DecModQ(zeta * f.GetValue(static_cast<uint32_t>(j + len)));
                const int32_t uj = f.GetValue(static_cast<uint32_t>(j));
                f.SetValue(static_cast<uint32_t>(j + len), DecModQ(uj - t));
                f.SetValue(static_cast<uint32_t>(j), DecModQ(uj + t));
            }
        }
    }
}

/** FIPS 203 Alg.10 InverseNTT（就地）；ζ^{−BitRev7(i)}≡−zetas[i]。 */
__aicore__ inline void Alg10InverseNtt(AscendC::LocalTensor<int32_t> &f,
                                       const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 127;
    for (int32_t len = 2; len <= 128; len *= 2) {
        for (int32_t start = 0; start < t30_dec::kPolyN; start += 2 * len) {
            const int32_t zeta = DecModQ(-zetas.GetValue(static_cast<uint32_t>(zi)));
            zi -= 1;
            for (int32_t j = start; j < start + len; ++j) {
                const int32_t t = f.GetValue(static_cast<uint32_t>(j));
                const int32_t u = f.GetValue(static_cast<uint32_t>(j + len));
                f.SetValue(static_cast<uint32_t>(j), DecModQ(t + u));
                f.SetValue(static_cast<uint32_t>(j + len), DecModQ(zeta * (t - u)));
            }
        }
    }
    for (int32_t i = 0; i < t30_dec::kPolyN; ++i) {
        const int32_t x = f.GetValue(static_cast<uint32_t>(i));
        f.SetValue(static_cast<uint32_t>(i), DecModQ(x * t30_dec::kInttScale));
    }
}

/** Alg.11 MultiplyNTTs。 */
__aicore__ inline void Alg11MulNtts(const AscendC::LocalTensor<int32_t> &f,
                                    const AscendC::LocalTensor<int32_t> &g,
                                    const AscendC::LocalTensor<int32_t> &gammas,
                                    AscendC::LocalTensor<int32_t> &h)
{
    for (int32_t i = 0; i < t30_dec::kPolyN / 2; ++i) {
        const int32_t a0 = f.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t a1 = f.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t b0 = g.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t b1 = g.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t gamma = gammas.GetValue(static_cast<uint32_t>(i));
        const int32_t a1b1 = DecModQ(a1 * b1);
        h.SetValue(static_cast<uint32_t>(2 * i), DecModQ(a0 * b0 + a1b1 * gamma));
        h.SetValue(static_cast<uint32_t>(2 * i + 1), DecModQ(a0 * b1 + a1 * b0));
    }
}

__aicore__ inline void DecLoadPoly(AscendC::LocalTensor<int32_t> &dst,
                                   AscendC::GlobalTensor<int32_t> &srcGm, int32_t polyIdx)
{
    AscendC::DataCopy(dst, srcGm[static_cast<uint32_t>(polyIdx * t30_dec::kPolyN)],
                      static_cast<uint32_t>(t30_dec::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void DecStorePoly(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                    const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * t30_dec::kPolyN)], src,
                      static_cast<uint32_t>(t30_dec::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * Alg.15 前半：直读 dkIn/cIn → ŝ/u/v（DataCopy 落 GM）。
 * @param dkIn H2D dk_pke[1536]
 * @param cIn  H2D c[1568]=c₁‖c₂
 * @param ws   Decrypt workspace（写 OFF_S_HAT/U/V）
 *
 * 背景：NPU 上业务 GM SetValue 不可靠。结论：直读 H2D + UB 写出。
 * 未采用：ws 镜像 dk/c。
 */
__aicore__ inline void UnpackDecryptInputs(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws)
{
    using namespace t30_dec;

    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmCIn;
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));
    gmCIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cIn),
                          static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queBytes;
    constexpr uint32_t kMaxCPolyBytes = static_cast<uint32_t>(32 * kDu);
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queBytes, 1, kMaxCPolyBytes);

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> bytes = queBytes.AllocTensor<uint8_t>();

    // ŝ ← ByteDecode₁₂(dk)
    for (int32_t p = 0; p < kKem; ++p) {
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(dkIn) + static_cast<uint32_t>(p * 384);
        f203_byte_codec::poly_byte_decode12_scalar_gm(poly, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmSHat[static_cast<uint32_t>(p * kPolyN)], poly,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // u ← Decompress₁₁(ByteDecode₁₁(c₁))
    constexpr int32_t kC1PolyBytes = 32 * kDu;
    for (int32_t p = 0; p < kKem; ++p) {
        const uint32_t off = static_cast<uint32_t>(p * kC1PolyBytes);
        AscendC::DataCopy(bytes, gmCIn[off], static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        DecodeBitsD(poly, bytes, kDu);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t c = poly.GetValue(static_cast<uint32_t>(i));
            poly.SetValue(static_cast<uint32_t>(i), DecompressBits(c, kDu));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmU[static_cast<uint32_t>(p * kPolyN)], poly,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // v ← Decompress₅(ByteDecode₅(c₂))
    {
        AscendC::DataCopy(bytes, gmCIn[static_cast<uint32_t>(kC1Bytes)],
                          static_cast<uint32_t>(kC2Bytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        DecodeBitsD(poly, bytes, kDv);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t c = poly.GetValue(static_cast<uint32_t>(i));
            poly.SetValue(static_cast<uint32_t>(i), DecompressBits(c, kDv));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmV, poly, static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    quePoly.FreeTensor(poly);
    queBytes.FreeTensor(bytes);
}

/**
 * AIV0：û←NTT(u)；ŵ←Σⱼ MultiplyNTTs(ŝⱼ,ûⱼ)。
 * @param ws Decrypt workspace
 */
__aicore__ inline void RunNttSuDot(GM_ADDR ws)
{
    using namespace t30_dec;

    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmGammas;
    AscendC::GlobalTensor<int32_t> gmUHat;
    AscendC::GlobalTensor<int32_t> gmWHat;
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS),
                            static_cast<uint32_t>(kZetaN));
    gmGammas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_GAMMAS),
                             static_cast<uint32_t>(kGammaN));
    gmUHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmWHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W_HAT),
                           static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queS;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queProd;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queAcc;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queZeta;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queGamma;
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queS, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queProd, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queAcc, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queZeta, 1, static_cast<uint32_t>(kZetasBytes));
    pipe.InitBuffer(queGamma, 1, static_cast<uint32_t>(kGammasBytes));

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> sLoc = queS.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> prod = queProd.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> acc = queAcc.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> zetas = queZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> gammas = queGamma.AllocTensor<int32_t>();

    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gammas, gmGammas, static_cast<uint32_t>(kGammaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    for (int32_t j = 0; j < kKem; ++j) {
        DecLoadPoly(poly, gmU, j);
        Alg9ForwardNtt(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        DecStorePoly(gmUHat, j, poly);
    }

    for (int32_t c = 0; c < kPolyN; ++c) {
        acc.SetValue(static_cast<uint32_t>(c), 0);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t j = 0; j < kKem; ++j) {
        DecLoadPoly(sLoc, gmSHat, j);
        DecLoadPoly(poly, gmUHat, j);
        Alg11MulNtts(sLoc, poly, gammas, prod);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t c = 0; c < kPolyN; ++c) {
            const int32_t s = acc.GetValue(static_cast<uint32_t>(c)) +
                              prod.GetValue(static_cast<uint32_t>(c));
            acc.SetValue(static_cast<uint32_t>(c), DecModQ(s));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    AscendC::DataCopy(gmWHat, acc, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    quePoly.FreeTensor(poly);
    queS.FreeTensor(sLoc);
    queProd.FreeTensor(prod);
    queAcc.FreeTensor(acc);
    queZeta.FreeTensor(zetas);
    queGamma.FreeTensor(gammas);
}

/**
 * AIV0：w←INTT(ŵ)；m←BE₁(Compress₁(v−w))，DataCopy 到 OFF_M 与 mOut。
 * @param ws   Decrypt workspace
 * @param mOut Host 可见 m'[32]
 */
__aicore__ inline void RunInttExtractM(GM_ADDR ws, GM_ADDR mOut)
{
    using namespace t30_dec;

    AscendC::GlobalTensor<int32_t> gmWHat;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmW;
    AscendC::GlobalTensor<uint8_t> gmMWs;
    AscendC::GlobalTensor<uint8_t> gmMOut;
    gmWHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W_HAT),
                           static_cast<uint32_t>(kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS),
                            static_cast<uint32_t>(kZetaN));
    gmW.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W),
                        static_cast<uint32_t>(kPolyN));
    gmMWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_M),
                          static_cast<uint32_t>(kMBytes));
    gmMOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(mOut),
                           static_cast<uint32_t>(kMBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queV;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queZeta;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queM;
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queV, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queZeta, 1, static_cast<uint32_t>(kZetasBytes));
    pipe.InitBuffer(queM, 1, static_cast<uint32_t>(kMBytes));

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> vLoc = queV.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> zetas = queZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> mLoc = queM.AllocTensor<uint8_t>();

    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(poly, gmWHat, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    Alg10InverseNtt(poly, zetas);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmW, poly, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::DataCopy(vLoc, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    // 先在 UB 组 BE₁，再 DataCopy（禁 uint8 GM SetValue）
    for (int32_t bi = 0; bi < static_cast<int32_t>(kMBytes); ++bi) {
        uint8_t byte = 0;
        for (int32_t b = 0; b < 8; ++b) {
            const int32_t i = bi * 8 + b;
            const int32_t diff =
                DecModQ(vLoc.GetValue(static_cast<uint32_t>(i)) -
                       poly.GetValue(static_cast<uint32_t>(i)));
            const int32_t bit = CompressBit1(diff);
            byte = static_cast<uint8_t>(byte | (static_cast<uint8_t>(bit) << b));
        }
        mLoc.SetValue(static_cast<uint32_t>(bi), byte);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMWs, mLoc, static_cast<uint32_t>(kMBytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMOut, mLoc, static_cast<uint32_t>(kMBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    quePoly.FreeTensor(poly);
    queV.FreeTensor(vLoc);
    queZeta.FreeTensor(zetas);
    queM.FreeTensor(mLoc);
}

} // namespace rb_t30

#endif
