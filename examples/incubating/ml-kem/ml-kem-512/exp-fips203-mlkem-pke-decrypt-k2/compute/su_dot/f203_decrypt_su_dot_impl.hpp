/**
 * @file f203_decrypt_su_dot_impl.hpp
 * @brief Alg.15：ŵ ← ⟨ŝ, û⟩（NTT 域内积，Alg.11 MultiplyNTTs + 累加 mod q）。
 *
 * 流水线位置：NTT(u') 之后、INTT(ŵ) 之前；仅 AIV0 执行。
 * CPU：标量 Barrett + MultiplyNTTs；SIM/NPU：向量 alg11_ub::compute_on_ub。
 * 随后 pad_w_hat_for_intt：ŵ → k=2 polyvec 前缀（余下清零）供 INTT Stage1。
 */
#ifndef F203_DECRYPT_SU_DOT_IMPL_HPP
#define F203_DECRYPT_SU_DOT_IMPL_HPP

#include "kernel_operator.h"

/* MIX 单 launch：ROM 仅 AIV + CPU 链接一次，避免 aic/aiv merge duplicate symbol。 */
#if defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__)
#define gAlg11GammasGm                gSuDotGammasGm
#define gAlg11GatherEvenByteGm        gSuDotGatherEvenByteGm
#define gAlg11GatherOddByteGm         gSuDotGatherOddByteGm
#define gAlg11InterleaveReorderByteGm gSuDotInterleaveReorderByteGm
#include "alg11_rom_tables.cpp"
#endif

#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "innerproduct_mod.hpp"
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

namespace decrypt_g4 {

constexpr int32_t kSuDotN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kSuDotK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kSuDotQ = static_cast<int32_t>(F203_DECRYPT_Q);
constexpr int32_t kSuDotRomPairs = kSuDotN / 2;
constexpr int32_t kSuDotVecWsInts = 8 * kSuDotRomPairs;
constexpr int32_t kSuDotScratchInts = 4 * kSuDotN;

#if defined(ASCENDC_CPU_DEBUG)
#include "alg11_gammas.h"

/** CPU 孪生：单系数 Barrett。 */
__aicore__ inline int32_t su_dot_barrett_red(int32_t x)
{
    const int32_t q = kSuDotQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/** CPU：标量 MultiplyNTTs。 */
__aicore__ inline void su_dot_multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g)
{
    for (int32_t i = 0; i < kSuDotN / 2; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[i * 2];
        const int32_t a1 = f[i * 2 + 1];
        const int32_t b0 = g[i * 2];
        const int32_t b1 = g[i * 2 + 1];
        const int32_t a1b1 = su_dot_barrett_red(a1 * b1);
        h[i * 2] = su_dot_barrett_red(a0 * b0 + a1b1 * gamma);
        h[i * 2 + 1] = su_dot_barrett_red(a0 * b1 + a1 * b0);
    }
}

/** CPU：ŵ = Σ_j MultiplyNTTs(ŝ_j, û_j)。 */
__aicore__ inline void su_dot_scalar_impl(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    const auto *sGm = reinterpret_cast<const __gm__ int32_t *>(sHatGm);
    const auto *uGm = reinterpret_cast<const __gm__ int32_t *>(uHatGm);
    auto *wGm = reinterpret_cast<__gm__ int32_t *>(wHatGm);
    int32_t acc[kSuDotN];
    int32_t prod[kSuDotN];
    for (int32_t c = 0; c < kSuDotN; ++c) {
        acc[c] = 0;
    }
    for (int32_t j = 0; j < kSuDotK; ++j) {
        su_dot_multiply_ntts_scalar(prod, sGm + j * kSuDotN, uGm + j * kSuDotN);
        for (int32_t c = 0; c < kSuDotN; ++c) {
            acc[c] += prod[c];
        }
    }
    for (int32_t c = 0; c < kSuDotN; ++c) {
        int32_t x = acc[c];
        x %= kSuDotQ;
        if (x < 0) {
            x += kSuDotQ;
        }
        wGm[c] = x;
    }
}
#endif

class KernelSuDot {
public:
    __aicore__ inline KernelSuDot() {}

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void Init(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
    {
        sAddr_ = sHatGm;
        uAddr_ = uHatGm;
        wAddr_ = wHatGm;
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sHatGm, kSuDotK * kSuDotN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uHatGm, kSuDotK * kSuDotN);
        wGm_.SetGlobalBuffer((__gm__ int32_t *)wHatGm, kSuDotN);
#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(kSuDotScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(kSuDotVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kSuDotN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kSuDotN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, kSuDotRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kSuDotRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kSuDotRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kSuDotN * sizeof(int32_t));
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(romUb_, kSuDotRomPairs);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
#endif
    }

    /**
     * 计算 ŵ = Σ_j MultiplyNTTs(ŝ_j, û_j) mod q，写回 wGm_。
     * CPU：标量路径；设备：k 次向量 MultiplyNTTs 累加后再一次性 mod q。
     */
    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        su_dot_scalar_impl(sAddr_, uAddr_, wAddr_);
#else
        /* scratch 分区：row / mod 临时 / 累加线 / mod 辅助 */
        LocalTensor<int32_t> row = bufI32(0, kSuDotN);
        LocalTensor<int32_t> modT2 = bufI32(kSuDotN, kSuDotN);
        LocalTensor<int32_t> accLine = bufI32(2 * kSuDotN, kSuDotN);
        LocalTensor<int32_t> fLoc = bufI32(3 * kSuDotN, kSuDotN);

        LocalTensor<int32_t> wsLocal = wsQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.DeQue<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.DeQue<int32_t>();
        alg11_vec::RomUbLuts rom;
        rom.gammaV = gammaLocal;
        rom.gatherEvenByte = gatherEvenLocal;
        rom.gatherOddByte = gatherOddLocal;
        rom.interleaveReorderByte = interleaveLocal;

        /* 对每个 poly 对 (ŝ_j, û_j) 做 Alg.11，累加到 accLine（尚未最终 mod） */
        for (int32_t j = 0; j < kSuDotK; ++j) {
            LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(fPoly, sGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kSuDotN)], kSuDotN);
            DataCopy(gPoly, uGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kSuDotN)], kSuDotN);
            inQueueF_.EnQue(fPoly);
            inQueueG_.EnQue(gPoly);
            fPoly = inQueueF_.DeQue<int32_t>();
            gPoly = inQueueG_.DeQue<int32_t>();
            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            inQueueG_.FreeTensor(gPoly);
            if (j == 0) {
                DataCopy(accLine, row, kSuDotN);
            } else {
                Add(accLine, accLine, row, kSuDotN);
            }
        }
        /* 累加和 → [0,q)；写 ŵ */
        hat_ip::mod_q_final_vec(accLine, kSuDotQ, fLoc, modT2, kSuDotN);
        DataCopy(wGm_[0], accLine, kSuDotN);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);
#endif
    }

private:
    GM_ADDR sAddr_{nullptr};
    GM_ADDR uAddr_{nullptr};
    GM_ADDR wAddr_{nullptr};
    TPipe pipe_;
    TBuf<TPosition::VECCALC> scratch_;
    TQue<QuePosition::VECIN, 1> wsQue_;
    TQue<QuePosition::VECIN, 1> inQueueF_;
    TQue<QuePosition::VECIN, 1> inQueueG_;
    TQue<QuePosition::VECIN, 1> gammaLutQue_;
    TQue<QuePosition::VECIN, 1> gatherEvenQue_;
    TQue<QuePosition::VECIN, 1> gatherOddQue_;
    TQue<QuePosition::VECIN, 1> interleaveReorderQue_;
    alg11_vec::RomUbLuts romUb_;
    GlobalTensor<int32_t> sGm_;
    GlobalTensor<int32_t> uGm_;
    GlobalTensor<int32_t> wGm_;
};

/**
 * 设备入口：ŵ[N] ← Σ_j MultiplyNTTs(ŝ[j], û[j]) mod q（向量或 CPU 标量）。
 * @param sHatGm ŝ [k×N]；@param uHatGm û [k×N]；@param wHatGm ŵ [N]
 * 前置：仅 AIV0。
 */
__aicore__ inline void su_dot_impl(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    KernelSuDot op;
    op.Init(sHatGm, uHatGm, wHatGm);
    op.Process();
}

/**
 * 生产 pad：ŵ[n] → wPadded[k×n]，仅 slot0 有值，其余 0（供 INTT Stage1 DataCopy）。
 * 背景：单 kernel 内禁止标量写 GM 再 MTE 读（Encrypt R2）；须 Duplicate+DataCopy。
 * 注：2026-07-09 UB 驻留实验曾尝试跳过本函数写盘；SIM 无 tick 收益已回滚（见 STATUS §UB）。
 */
__aicore__ inline void pad_w_hat_for_intt(GM_ADDR wPaddedGm, GM_ADDR wHatGm)
{
    using AscendC::DataCopy;
    using AscendC::Duplicate;
    constexpr uint32_t kTotalInts =
        static_cast<uint32_t>(::tiling::dstFileBytes / sizeof(int32_t)); /* 2×256 */

    AscendC::GlobalTensor<int32_t> gmSrc;
    AscendC::GlobalTensor<int32_t> gmDst;
    gmSrc.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(wHatGm), static_cast<uint32_t>(kSuDotN));
    gmDst.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(wPaddedGm), kTotalInts);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufPad;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufW;
    pipe.InitBuffer(bufPad, kTotalInts * sizeof(int32_t));
    pipe.InitBuffer(bufW, static_cast<uint32_t>(kSuDotN) * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> padLocal = bufPad.Get<int32_t>();
    AscendC::LocalTensor<int32_t> wLocal = bufW.Get<int32_t>();

    Duplicate(padLocal, 0, kTotalInts);
    AscendC::PipeBarrier<PIPE_V>();
    DataCopy(wLocal, gmSrc, static_cast<uint32_t>(kSuDotN));
    AscendC::PipeBarrier<PIPE_ALL>();
    DataCopy(padLocal, wLocal, static_cast<uint32_t>(kSuDotN));
    AscendC::PipeBarrier<PIPE_ALL>();
    DataCopy(gmDst, padLocal, kTotalInts);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace decrypt_g4

#endif
