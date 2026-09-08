/**
 * @file uv_device_math.hpp
 * @brief RB-T15 设备侧拓扑数学：MultiplyNTTs / Âᵀ∘ŷ / ⟨t̂,ŷ⟩ / InverseNTT + 加噪。
 *
 * 流水线位置：Launch2 MIX AIV0 — GATE 前写 û/v̂，INTT 握手后写时域 u/v；随后 pack→c。
 * 数学契约对齐 T08 Host 孪生（FIPS Alg.11/10 + Encrypt 行 17–19 拓扑）；
 * **禁止** 抄 alg14 / encrypt / encaps / decaps / frozen 设备源码。
 *
 * 实现形态：AIV 标量 + LocalTensor（UB），非向量化性能路径；本刀验收为 I/O 对拍。
 */
#ifndef RB_T15_UV_DEVICE_MATH_HPP
#define RB_T15_UV_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t15 {

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
 * FIPS 203 Alg.11 MultiplyNTTs：偶奇对 + γ·BaseCaseMultiply。
 * @param f / g 长度 256 的 NTT 域 poly（LocalTensor）
 * @param gammas LocalTensor[128]（已从 GM 拷入 UB）
 * @param h 输出 LocalTensor[256]
 */
__aicore__ inline void MultiplyNTTs(const AscendC::LocalTensor<int32_t> &f,
                                    const AscendC::LocalTensor<int32_t> &g,
                                    const AscendC::LocalTensor<int32_t> &gammas,
                                    AscendC::LocalTensor<int32_t> &h)
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
 * FIPS 203 Alg.10 InverseNTT（就地）。
 * 背景：ζ^{−BitRev7(i)} ≡ −zetas[i] (mod q)；末尾 × 128^{-1}。
 * @param f LocalTensor[256] NTT 域 → 时域
 * @param zetas LocalTensor[128]
 */
__aicore__ inline void InverseNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 127;
    for (int32_t length = 2; length <= 128; length *= 2) {
        for (int32_t start = 0; start < tiling::kPolyN; start += 2 * length) {
            const int32_t zeta = ModQ(-zetas.GetValue(static_cast<uint32_t>(zi)));
            zi -= 1;
            for (int32_t j = start; j < start + length; ++j) {
                const int32_t t = f.GetValue(static_cast<uint32_t>(j));
                const int32_t u = f.GetValue(static_cast<uint32_t>(j + length));
                f.SetValue(static_cast<uint32_t>(j), ModQ(t + u));
                f.SetValue(static_cast<uint32_t>(j + length), ModQ(zeta * (t - u)));
            }
        }
    }
    for (int32_t i = 0; i < tiling::kPolyN; ++i) {
        const int32_t x = f.GetValue(static_cast<uint32_t>(i));
        f.SetValue(static_cast<uint32_t>(i), ModQ(x * tiling::kInttScale));
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

/** 整 poly LocalTensor → GM。 */
__aicore__ inline void CopyPolyOut(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                   const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * tiling::kPolyN)], src,
                      static_cast<uint32_t>(tiling::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * NTT 域拼装：û = Âᵀ ∘ ŷ，v̂ = ⟨t̂, ŷ⟩；写出 OFF_U_HAT / OFF_V_HAT。
 *
 * 布局：a_hat 行主序 flat(p,j,*)=(p*kKem+j)*N；
 *   û[i] = Σ_j MultiplyNTTs(Â[j,i], ŷ[j])（因 Âᵀ[i,j]=Â[j,i]）
 * lazy int64 累加后一次 ModQ（kKem=4 足够小）。
 *
 * @param ws 共享 workspace 基址
 */
__aicore__ inline void ComputeNttDomainUv(GM_ADDR ws)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmA;
    AscendC::GlobalTensor<int32_t> gmY;
    AscendC::GlobalTensor<int32_t> gmT;
    AscendC::GlobalTensor<int32_t> gmGammas;
    AscendC::GlobalTensor<int32_t> gmUHat;
    AscendC::GlobalTensor<int32_t> gmVHat;
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
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queF;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queG;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queProd;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queAcc;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queGamma;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kTabB = static_cast<uint32_t>(kGammasBytes);
    pipe.InitBuffer(queF, 1, kPolyB);
    pipe.InitBuffer(queG, 1, kPolyB);
    pipe.InitBuffer(queProd, 1, kPolyB);
    pipe.InitBuffer(queAcc, 1, kPolyB);
    pipe.InitBuffer(queGamma, 1, kTabB);

    AscendC::LocalTensor<int32_t> f = queF.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> g = queG.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> prod = queProd.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> acc = queAcc.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> gammas = queGamma.AllocTensor<int32_t>();
    AscendC::DataCopy(gammas, gmGammas, static_cast<uint32_t>(kGammaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- û[i] = Σ_j Â[j,i] ∘ ŷ[j] ----------
    for (int32_t i = 0; i < kKem; ++i) {
        // 清零累加器（用 SetValue；避免依赖 Duplicate 行为差异）
        for (int32_t c = 0; c < kPolyN; ++c) {
            acc.SetValue(static_cast<uint32_t>(c), 0);
        }
        AscendC::PipeBarrier<PIPE_ALL>();

        for (int32_t j = 0; j < kKem; ++j) {
            // Â 行主序：Â[j,i] 偏移 (j*kKem+i)*N
            const uint32_t aOff =
                static_cast<uint32_t>((j * kKem + i) * kPolyN);
            AscendC::DataCopy(f, gmA[aOff], static_cast<uint32_t>(kPolyN));
            AscendC::PipeBarrier<PIPE_ALL>();
            CopyPolyIn(g, gmY, j);
            MultiplyNTTs(f, g, gammas, prod);
            AscendC::PipeBarrier<PIPE_ALL>();
            // 每步 ModQ：acc、prod ∈[0,q) → 和 < 2q，一次规约即可
            for (int32_t c = 0; c < kPolyN; ++c) {
                const int32_t s = acc.GetValue(static_cast<uint32_t>(c)) +
                                  prod.GetValue(static_cast<uint32_t>(c));
                acc.SetValue(static_cast<uint32_t>(c), ModQ(s));
            }
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        CopyPolyOut(gmUHat, i, acc);
    }

    // ---------- v̂ = Σ_i t̂[i] ∘ ŷ[i] ----------
    for (int32_t c = 0; c < kPolyN; ++c) {
        acc.SetValue(static_cast<uint32_t>(c), 0);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kKem; ++i) {
        CopyPolyIn(f, gmT, i);
        CopyPolyIn(g, gmY, i);
        MultiplyNTTs(f, g, gammas, prod);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t c = 0; c < kPolyN; ++c) {
            const int32_t s = acc.GetValue(static_cast<uint32_t>(c)) +
                              prod.GetValue(static_cast<uint32_t>(c));
            acc.SetValue(static_cast<uint32_t>(c), ModQ(s));
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

/**
 * 时域收尾：u = INTT(û)+e₁，v = INTT(v̂)+e₂+μ；写 ws 内 OFF_U / OFF_V。
 * @param ws 共享 workspace
 */
__aicore__ inline void ComputeInttAddNoise(GM_ADDR ws)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmUHat;
    AscendC::GlobalTensor<int32_t> gmVHat;
    AscendC::GlobalTensor<int32_t> gmE1;
    AscendC::GlobalTensor<int32_t> gmE2;
    AscendC::GlobalTensor<int32_t> gmMu;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
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
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queNoise;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queZeta;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queMu;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kZetaB = static_cast<uint32_t>(kZetasBytes);
    pipe.InitBuffer(quePoly, 1, kPolyB);
    pipe.InitBuffer(queNoise, 1, kPolyB);
    pipe.InitBuffer(queZeta, 1, kZetaB);
    pipe.InitBuffer(queMu, 1, kPolyB);

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> noise = queNoise.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> zetas = queZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> muLoc = queMu.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(muLoc, gmMu, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    // u[i] = INTT(û[i]) + e1[i]
    for (int32_t i = 0; i < kKem; ++i) {
        CopyPolyIn(poly, gmUHat, i);
        InverseNTT(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        CopyPolyIn(noise, gmE1, i);
        for (int32_t c = 0; c < kPolyN; ++c) {
            const int32_t s = ModQ(poly.GetValue(static_cast<uint32_t>(c)) +
                                  noise.GetValue(static_cast<uint32_t>(c)));
            poly.SetValue(static_cast<uint32_t>(c), s);
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        CopyPolyOut(gmU, i, poly);
    }

    // v = INTT(v̂) + e2 + μ
    AscendC::DataCopy(poly, gmVHat, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    InverseNTT(poly, zetas);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(noise, gmE2, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t c = 0; c < kPolyN; ++c) {
        const int32_t s = ModQ(poly.GetValue(static_cast<uint32_t>(c)) +
                              noise.GetValue(static_cast<uint32_t>(c)) +
                              muLoc.GetValue(static_cast<uint32_t>(c)));
        poly.SetValue(static_cast<uint32_t>(c), s);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmV, poly, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    quePoly.FreeTensor(poly);
    queNoise.FreeTensor(noise);
    queZeta.FreeTensor(zetas);
    queMu.FreeTensor(muLoc);
}

} // namespace rb_t15

#endif
