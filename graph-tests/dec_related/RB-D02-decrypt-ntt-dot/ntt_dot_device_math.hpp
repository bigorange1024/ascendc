/**
 * @file ntt_dot_device_math.hpp
 * @brief RB-D02 设备侧：Alg.9 正向 NTT(u) + Alg.11 Σ MultiplyNTTs(ŝ,û)→ŵ。
 *
 * 流水线位置：MIX AIV0 在握手 Wait(3) 后执行本文件。
 * 数学契约：
 *   - û[j] ← NTT(u[j])，j=0..k-1（poly-batch 整 poly）
 *   - ŵ ← Σ_j MultiplyNTTs(ŝ[j], û[j])（NTT 域单 poly）
 * 表：Host 预喂 kMlkemZetas / kMlkemGammas（FIPS Appendix A）。
 *
 * 实现形态：AIV 标量 + LocalTensor（UB）；非向量化/非 Cube Stage1–3 性能路径。
 * 契约约束（KB §A3）：
 *   - 每个 poly 完整 256 系数同核处理（禁 hi/lo limbsplit）
 *   - 本路径无 Gather；若后刀改走 S1–S3 仍禁 Gather
 * 写出：UB+DataCopy（X12）；禁 GlobalTensor::SetValue 写业务 GM。
 *
 * **禁止** 抄 alg15 / T25 / decrypt / encaps / frozen 设备源码。
 */
#ifndef RB_D02_NTT_DOT_DEVICE_MATH_HPP
#define RB_D02_NTT_DOT_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_d02 {

/**
 * 规范到 [0, q)。
 * @param x 任意 int32（可负）
 */
__aicore__ inline int32_t ModQ(int32_t x)
{
    int32_t r = x % tiling::kQ;
    if (r < 0) {
        r += tiling::kQ;
    }
    return r;
}

/**
 * FIPS 203 Alg.9 正向 NTT（就地）。
 * 背景：ζ^{BitRev7(i)} 取自 Host 预喂 zetas[128]；i 从 1 递增到 127。
 * @param f LocalTensor[256] 时域 → NTT 域
 * @param zetas LocalTensor[128]
 */
__aicore__ inline void ForwardNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 1;
    for (int32_t length = 128; length >= 2; length /= 2) {
        for (int32_t start = 0; start < tiling::kPolyN; start += 2 * length) {
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

/**
 * FIPS 203 Alg.11 MultiplyNTTs：偶奇对 + γ·BaseCaseMultiply。
 * @param h  输出 LocalTensor[256]
 * @param f  ŝ[j]（NTT 域）
 * @param g  û[j]（NTT 域）
 * @param gammas Host 预喂 kMlkemGammas[128]
 */
__aicore__ inline void MultiplyNTTs(AscendC::LocalTensor<int32_t> &h,
                                    const AscendC::LocalTensor<int32_t> &f,
                                    const AscendC::LocalTensor<int32_t> &g,
                                    const AscendC::LocalTensor<int32_t> &gammas)
{
    for (int32_t i = 0; i < tiling::kPolyN / 2; ++i) {
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

/**
 * 从 GM 拷一整 poly 到 LocalTensor。
 * @param dst UB；srcGm 起点；polyIdx 在向量中的 poly 下标（0..kKem-1）
 */
__aicore__ inline void CopyPolyIn(AscendC::LocalTensor<int32_t> &dst,
                                  AscendC::GlobalTensor<int32_t> &srcGm, int32_t polyIdx)
{
    AscendC::DataCopy(dst, srcGm[static_cast<uint32_t>(polyIdx * tiling::kPolyN)],
                      static_cast<uint32_t>(tiling::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** 整 poly LocalTensor → GM（X12 DataCopy）。 */
__aicore__ inline void CopyPolyOut(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                   const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * tiling::kPolyN)], src,
                      static_cast<uint32_t>(tiling::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0：û←NTT(u)；ŵ←Σ MultiplyNTTs(ŝ,û)；写出 ws 与独立 out。
 *
 * poly-batch：循环按 poly 索引；每次握完整 256 系数（含概念上的 hi+lo）；
 * 不按 limb 跨核切分；不调用 Gather。
 *
 * @param ws       共享 workspace（含 U / S_HAT / ZETAS / GAMMAS / U_HAT / W_HAT）
 * @param uHatOut  独立 û 落盘缓冲
 * @param wHatOut  独立 ŵ 落盘缓冲
 */
__aicore__ inline void ComputeNttAndSuDot(GM_ADDR ws, GM_ADDR uHatOut, GM_ADDR wHatOut)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmGammas;
    AscendC::GlobalTensor<int32_t> gmUHatWs;
    AscendC::GlobalTensor<int32_t> gmWHatWs;
    AscendC::GlobalTensor<int32_t> gmUHatOut;
    AscendC::GlobalTensor<int32_t> gmWHatOut;
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U));
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_HAT));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS));
    gmGammas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_GAMMAS));
    gmUHatWs.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U_HAT));
    gmWHatWs.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W_HAT));
    gmUHatOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uHatOut));
    gmWHatOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(wHatOut));

    AscendC::TPipe pipe;
    // VECIN：UB 驻留（标量 Get/Set）；对齐 toys/T12 契约，勿用 VECCALC
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qPoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qSHat;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qProd;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qAcc;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qZeta;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qGamma;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kZetaB = static_cast<uint32_t>(kZetasBytes);
    constexpr uint32_t kGammaB = static_cast<uint32_t>(kGammasBytes);
    pipe.InitBuffer(qPoly, 1, kPolyB);
    pipe.InitBuffer(qSHat, 1, kPolyB);
    pipe.InitBuffer(qProd, 1, kPolyB);
    pipe.InitBuffer(qAcc, 1, kPolyB);
    pipe.InitBuffer(qZeta, 1, kZetaB);
    pipe.InitBuffer(qGamma, 1, kGammaB);

    AscendC::LocalTensor<int32_t> zetas = qZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> gammas = qGamma.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::DataCopy(gammas, gmGammas, static_cast<uint32_t>(kGammaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> poly = qPoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> sHat = qSHat.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> prod = qProd.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> acc = qAcc.AllocTensor<int32_t>();

    // 累加器清零（ŵ 初值）
    for (int32_t i = 0; i < kPolyN; ++i) {
        acc.SetValue(static_cast<uint32_t>(i), 0);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    for (int32_t p = 0; p < kKem; ++p) {
        // ---------- 完整 poly：拷入 256 系数，就地 NTT，写 û ----------
        // 背景：KB §A3 poly-batch；禁 limbsplit / Gather。
        CopyPolyIn(poly, gmU, p);
        ForwardNTT(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        CopyPolyOut(gmUHatWs, p, poly);
        CopyPolyOut(gmUHatOut, p, poly);

        // ---------- MultiplyNTTs(ŝ[p], û[p]) 累加到 ŵ ----------
        // 结论：Decrypt Alg.15 内积段 Σ_j；本刀只到 ŵ，不做 INTT。
        CopyPolyIn(sHat, gmSHat, p);
        MultiplyNTTs(prod, sHat, poly, gammas);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t sum = ModQ(acc.GetValue(static_cast<uint32_t>(i)) +
                                     prod.GetValue(static_cast<uint32_t>(i)));
            acc.SetValue(static_cast<uint32_t>(i), sum);
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ŵ → ws 与独立 out（X12 DataCopy）
    AscendC::DataCopy(gmWHatWs, acc, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmWHatOut, acc, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    qPoly.FreeTensor(poly);
    qSHat.FreeTensor(sHat);
    qProd.FreeTensor(prod);
    qAcc.FreeTensor(acc);
    qZeta.FreeTensor(zetas);
    qGamma.FreeTensor(gammas);
}

} // namespace rb_d02

#endif
