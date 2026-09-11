/**
 * @file dot_encode_device_math.hpp
 * @brief RB-K03 设备侧：Alg.13 行 17–21 —— Â∘ŝ̂+ê̂ → ByteEncode₁₂ → ek‖ρ / dk。
 *
 * 流水线位置：MIX AIV0 在握手 Wait(3) 后执行本文件。
 * 数学契约（FIPS 203 / docs/notes F203-innerproduct）：
 *   t̂[p] = mod_q( Σ_{j=0}^{k-1} MultiplyNTTs(Â[p,j], ŝ̂[j]) + ê̂[p] )
 *   ek_pke = ByteEncode₁₂(t̂) ‖ ρ ； dk_pke = ByteEncode₁₂(ŝ̂)
 * Â 行主序 flat(p,j,*)=(p·k+j)·N；γ 表 Host 预喂 kMlkemGammas。
 *
 * 实现形态：AIV 标量 + LocalTensor（UB）；非向量化/非 Cube Stage1–3 性能路径。
 * 写出：UB+DataCopy（X12）；禁 GlobalTensor::SetValue 写业务 GM。
 *
 * **禁止** 抄 KeyGen / Encaps / Decrypt / frozen 设备源码；契约对齐 D02 MultiplyNTTs。
 */
#ifndef RB_K03_DOT_ENCODE_DEVICE_MATH_HPP
#define RB_K03_DOT_ENCODE_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_k03 {

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
 * @param h  输出 LocalTensor[256]
 * @param f  Â[p,j]（NTT 域）
 * @param g  ŝ̂[j]（NTT 域）
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
 * FIPS 203 Alg.5 ByteEncode₁₂：2 系数 → 3 字节（低 12 bit）。
 * @param dst  UB uint8[384]
 * @param src  LocalTensor[256] 系数（须已 ∈[0,q)）
 */
__aicore__ inline void ByteEncode12Poly(AscendC::LocalTensor<uint8_t> &dst,
                                        const AscendC::LocalTensor<int32_t> &src)
{
    for (int32_t i = 0; i < tiling::kPolyN / 2; ++i) {
        const int32_t v0 = src.GetValue(static_cast<uint32_t>(2 * i)) & 0xFFF;
        const int32_t v1 = src.GetValue(static_cast<uint32_t>(2 * i + 1)) & 0xFFF;
        dst.SetValue(static_cast<uint32_t>(3 * i + 0), static_cast<uint8_t>(v0 & 0xFF));
        dst.SetValue(static_cast<uint32_t>(3 * i + 1),
                     static_cast<uint8_t>(((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)));
        dst.SetValue(static_cast<uint32_t>(3 * i + 2), static_cast<uint8_t>((v1 >> 4) & 0xFF));
    }
}

/**
 * 从 Â GM 拷一整 poly：行主序下标 (p·k+j)·N。
 */
__aicore__ inline void CopyAHatPolyIn(AscendC::LocalTensor<int32_t> &dst,
                                      AscendC::GlobalTensor<int32_t> &aHatGm, int32_t p,
                                      int32_t j)
{
    const uint32_t off =
        static_cast<uint32_t>((p * tiling::kKem + j) * tiling::kPolyN);
    AscendC::DataCopy(dst, aHatGm[off], static_cast<uint32_t>(tiling::kPolyN));
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
 * AIV0：t̂←Â∘ŝ̂+ê̂；ByteEncode₁₂ → ek‖ρ、dk；写出 ws 与独立 out。
 *
 * 循环：外层输出行 p，内层列 j 累加 MultiplyNTTs；再加 ê̂[p]。
 * BE：逐 poly 编码后 DataCopy 到 ek/dk；ρ 拷到 ek[1536:1568)。
 *
 * @param ws     共享 workspace
 * @param ekOut  独立 ek_pke[1568] 落盘缓冲
 * @param dkOut  独立 dk_pke[1536] 落盘缓冲
 */
__aicore__ inline void ComputeDotEncode(GM_ADDR ws, GM_ADDR ekOut, GM_ADDR dkOut)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmAHat;
    AscendC::GlobalTensor<int32_t> gmSNtt;
    AscendC::GlobalTensor<int32_t> gmENtt;
    AscendC::GlobalTensor<int32_t> gmGammas;
    AscendC::GlobalTensor<int32_t> gmTHatWs;
    AscendC::GlobalTensor<uint8_t> gmRho;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmDkWs;
    AscendC::GlobalTensor<uint8_t> gmEkOut;
    AscendC::GlobalTensor<uint8_t> gmDkOut;

    gmAHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_A_HAT));
    gmSNtt.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_NTT));
    gmENtt.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_E_NTT));
    gmGammas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_GAMMAS));
    gmTHatWs.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_T_HAT));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK));
    gmDkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_DK));
    gmEkOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekOut));
    gmDkOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(dkOut));

    AscendC::TPipe pipe;
    // VECIN：UB 驻留（标量 Get/Set）；对齐 K02/D02，勿用 VECCALC
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qA;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qS;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qE;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qProd;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qAcc;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qGamma;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qBe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qRho;

    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kGammaB = static_cast<uint32_t>(kGammasBytes);
    constexpr uint32_t kBeB = static_cast<uint32_t>(kBe12PolyBytes);
    constexpr uint32_t kRhoB = static_cast<uint32_t>(kRhoBytes);

    pipe.InitBuffer(qA, 1, kPolyB);
    pipe.InitBuffer(qS, 1, kPolyB);
    pipe.InitBuffer(qE, 1, kPolyB);
    pipe.InitBuffer(qProd, 1, kPolyB);
    pipe.InitBuffer(qAcc, 1, kPolyB);
    pipe.InitBuffer(qGamma, 1, kGammaB);
    pipe.InitBuffer(qBe, 1, kBeB);
    pipe.InitBuffer(qRho, 1, kRhoB);

    AscendC::LocalTensor<int32_t> gammas = qGamma.AllocTensor<int32_t>();
    AscendC::DataCopy(gammas, gmGammas, static_cast<uint32_t>(kGammaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> aPoly = qA.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> sPoly = qS.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> ePoly = qE.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> prod = qProd.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> acc = qAcc.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> be = qBe.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> rhoUb = qRho.AllocTensor<uint8_t>();

    // ---------- 行 17–18：t̂[p] = Σ_j Â[p,j]∘ŝ̂[j] + ê̂[p] ----------
    // 背景：全 poly 累加（P-inner-1）；lazy 每步 mod（K=4 足够小）。
    for (int32_t p = 0; p < kKem; ++p) {
        for (int32_t i = 0; i < kPolyN; ++i) {
            acc.SetValue(static_cast<uint32_t>(i), 0);
        }
        AscendC::PipeBarrier<PIPE_ALL>();

        for (int32_t j = 0; j < kKem; ++j) {
            CopyAHatPolyIn(aPoly, gmAHat, p, j);
            AscendC::DataCopy(sPoly, gmSNtt[static_cast<uint32_t>(j * kPolyN)],
                              static_cast<uint32_t>(kPolyN));
            AscendC::PipeBarrier<PIPE_ALL>();
            MultiplyNTTs(prod, aPoly, sPoly, gammas);
            AscendC::PipeBarrier<PIPE_ALL>();
            for (int32_t i = 0; i < kPolyN; ++i) {
                const int32_t sum = ModQ(acc.GetValue(static_cast<uint32_t>(i)) +
                                         prod.GetValue(static_cast<uint32_t>(i)));
                acc.SetValue(static_cast<uint32_t>(i), sum);
            }
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        // 加噪声 ê̂[p]
        AscendC::DataCopy(ePoly, gmENtt[static_cast<uint32_t>(p * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t sum = ModQ(acc.GetValue(static_cast<uint32_t>(i)) +
                                     ePoly.GetValue(static_cast<uint32_t>(i)));
            acc.SetValue(static_cast<uint32_t>(i), sum);
        }
        AscendC::PipeBarrier<PIPE_ALL>();

        CopyPolyOut(gmTHatWs, p, acc);

        // ---------- 行 19：ByteEncode₁₂(t̂[p]) → ek 前 1536B ----------
        ByteEncode12Poly(be, acc);
        AscendC::PipeBarrier<PIPE_ALL>();
        const uint32_t ekOff = static_cast<uint32_t>(p) * static_cast<uint32_t>(kBe12PolyBytes);
        AscendC::DataCopy(gmEkWs[ekOff], be, static_cast<uint32_t>(kBe12PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmEkOut[ekOff], be, static_cast<uint32_t>(kBe12PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ρ → ek[1536:1568)
    AscendC::DataCopy(rhoUb, gmRho, kRhoB);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmEkWs[static_cast<uint32_t>(kBe12VecBytes)], rhoUb, kRhoB);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmEkOut[static_cast<uint32_t>(kBe12VecBytes)], rhoUb, kRhoB);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 行 20–21：dk = ByteEncode₁₂(ŝ̂) ----------
    for (int32_t j = 0; j < kKem; ++j) {
        AscendC::DataCopy(sPoly, gmSNtt[static_cast<uint32_t>(j * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        // 系数已在 [0,q)；仍 &0xFFF 于 ByteEncode12Poly
        ByteEncode12Poly(be, sPoly);
        AscendC::PipeBarrier<PIPE_ALL>();
        const uint32_t dkOff = static_cast<uint32_t>(j) * static_cast<uint32_t>(kBe12PolyBytes);
        AscendC::DataCopy(gmDkWs[dkOff], be, static_cast<uint32_t>(kBe12PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmDkOut[dkOff], be, static_cast<uint32_t>(kBe12PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    qA.FreeTensor(aPoly);
    qS.FreeTensor(sPoly);
    qE.FreeTensor(ePoly);
    qProd.FreeTensor(prod);
    qAcc.FreeTensor(acc);
    qGamma.FreeTensor(gammas);
    qBe.FreeTensor(be);
    qRho.FreeTensor(rhoUb);
}

} // namespace rb_k03

#endif
