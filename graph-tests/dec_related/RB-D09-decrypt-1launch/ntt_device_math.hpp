/**
 * @file ntt_device_math.hpp
 * @brief RB-D09：û←NTT(u)；ŵ←⟨ŝ,û⟩（MultiplyNTTs 求和）。
 *
 * 契约：Alg.9 ForwardNTT + Alg.11 MultiplyNTTs；poly-batch 整 poly；禁 Gather。
 * 自研标量 UB 路径；数学对齐 FIPS 203，非抄 decrypt/encrypt 设备源码。
 */
#ifndef RB_D09_NTT_DEVICE_MATH_HPP
#define RB_D09_NTT_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_d09 {

__aicore__ inline int32_t ModQ(int32_t x)
{
    int32_t r = x % d09::kQ;
    if (r < 0) {
        r += d09::kQ;
    }
    return r;
}

/** FIPS 203 Alg.9 正向 NTT（就地）。 */
__aicore__ inline void ForwardNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 1;
    for (int32_t length = 128; length >= 2; length /= 2) {
        for (int32_t start = 0; start < d09::kPolyN; start += 2 * length) {
            const int32_t zeta = zetas.GetValue(static_cast<uint32_t>(zi));
            zi += 1;
            for (int32_t j = start; j < start + length; ++j) {
                const int32_t t = ModQ(zeta * f.GetValue(static_cast<uint32_t>(j + length)));
                const int32_t uj = f.GetValue(static_cast<uint32_t>(j));
                f.SetValue(static_cast<uint32_t>(j + length), ModQ(uj - t));
                f.SetValue(static_cast<uint32_t>(j), ModQ(uj + t));
            }
        }
    }
}

/** Alg.11 MultiplyNTTs。 */
__aicore__ inline void MultiplyNTTs(const AscendC::LocalTensor<int32_t> &f,
                                    const AscendC::LocalTensor<int32_t> &g,
                                    const AscendC::LocalTensor<int32_t> &gammas,
                                    AscendC::LocalTensor<int32_t> &h)
{
    for (int32_t i = 0; i < d09::kPolyN / 2; ++i) {
        const int32_t a0 = f.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t a1 = f.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t b0 = g.GetValue(static_cast<uint32_t>(2 * i));
        const int32_t b1 = g.GetValue(static_cast<uint32_t>(2 * i + 1));
        const int32_t gamma = gammas.GetValue(static_cast<uint32_t>(i));
        const int32_t a1b1 = ModQ(a1 * b1);
        h.SetValue(static_cast<uint32_t>(2 * i), ModQ(a0 * b0 + a1b1 * gamma));
        h.SetValue(static_cast<uint32_t>(2 * i + 1), ModQ(a0 * b1 + a1 * b0));
    }
}

__aicore__ inline void CopyPolyIn(AscendC::LocalTensor<int32_t> &dst,
                                  AscendC::GlobalTensor<int32_t> &srcGm, int32_t polyIdx)
{
    AscendC::DataCopy(dst, srcGm[static_cast<uint32_t>(polyIdx * d09::kPolyN)],
                      static_cast<uint32_t>(d09::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CopyPolyOut(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                   const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * d09::kPolyN)], src,
                      static_cast<uint32_t>(d09::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0：û←NTT(u)；ŵ←Σⱼ ŝⱼ∘ûⱼ。
 * @param ws 统一 workspace（读 U/S_HAT/ZETAS/GAMMAS；写 U_HAT/W_HAT）
 */
__aicore__ inline void ComputeNttAndSuDot(GM_ADDR ws)
{
    using namespace d09;

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

    // û[j] ← NTT(u[j])
    for (int32_t j = 0; j < kKem; ++j) {
        CopyPolyIn(poly, gmU, j);
        ForwardNTT(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        CopyPolyOut(gmUHat, j, poly);
    }

    // ŵ ← Σⱼ MultiplyNTTs(ŝ[j], û[j])
    for (int32_t c = 0; c < kPolyN; ++c) {
        acc.SetValue(static_cast<uint32_t>(c), 0);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t j = 0; j < kKem; ++j) {
        CopyPolyIn(sLoc, gmSHat, j);
        CopyPolyIn(poly, gmUHat, j);
        MultiplyNTTs(sLoc, poly, gammas, prod);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t c = 0; c < kPolyN; ++c) {
            const int32_t s = acc.GetValue(static_cast<uint32_t>(c)) +
                              prod.GetValue(static_cast<uint32_t>(c));
            acc.SetValue(static_cast<uint32_t>(c), ModQ(s));
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

} // namespace rb_d09

#endif
