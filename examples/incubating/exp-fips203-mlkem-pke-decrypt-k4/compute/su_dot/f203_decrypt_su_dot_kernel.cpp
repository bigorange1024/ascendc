/**
 * @file f203_decrypt_su_dot_kernel.cpp
 * @brief G3：w_hat ← Σ_j MultiplyNTTs(ŝ[j], û[j])（单 poly 输出）。
 */
#include "kernel_operator.h"

#define gAlg11GammasGm                gSuDotGammasGm
#define gAlg11GatherEvenByteGm        gSuDotGatherEvenByteGm
#define gAlg11GatherOddByteGm         gSuDotGatherOddByteGm
#define gAlg11InterleaveReorderByteGm gSuDotInterleaveReorderByteGm
#include "alg11_rom_tables.cpp"

#include "f203_decrypt_layout.h"
#include "innerproduct_mod.hpp"
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

namespace {

constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kHatQ = static_cast<int32_t>(F203_DECRYPT_Q);
constexpr int32_t kRomPairs = kN / 2;
constexpr int32_t kVecWsInts = 8 * kRomPairs;
constexpr int32_t kScratchInts = 4 * kN;

#if defined(ASCENDC_CPU_DEBUG)
#include "alg11_gammas.h"

__aicore__ inline int32_t barrett_red(int32_t x)
{
    const int32_t q = kHatQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g)
{
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[i * 2];
        const int32_t a1 = f[i * 2 + 1];
        const int32_t b0 = g[i * 2];
        const int32_t b1 = g[i * 2 + 1];
        const int32_t a1b1 = barrett_red(a1 * b1);
        h[i * 2] = barrett_red(a0 * b0 + a1b1 * gamma);
        h[i * 2 + 1] = barrett_red(a0 * b1 + a1 * b0);
    }
}

__aicore__ inline void su_dot_scalar(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    const auto *sGm = reinterpret_cast<const __gm__ int32_t *>(sHatGm);
    const auto *uGm = reinterpret_cast<const __gm__ int32_t *>(uHatGm);
    auto *wGm = reinterpret_cast<__gm__ int32_t *>(wHatGm);
    int32_t acc[kN];
    int32_t prod[kN];
    for (int32_t c = 0; c < kN; ++c) {
        acc[c] = 0;
    }
    for (int32_t j = 0; j < kK; ++j) {
        multiply_ntts_scalar(prod, sGm + j * kN, uGm + j * kN);
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] += prod[c];
        }
    }
    for (int32_t c = 0; c < kN; ++c) {
        int32_t x = acc[c];
        x %= kHatQ;
        if (x < 0) {
            x += kHatQ;
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
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sHatGm, kK * kN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uHatGm, kK * kN);
        wGm_.SetGlobalBuffer((__gm__ int32_t *)wHatGm, kN);
#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kN * sizeof(int32_t));
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(romUb_, kRomPairs);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
#endif
    }

    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        su_dot_scalar(sAddr_, uAddr_, wAddr_);
#else
        LocalTensor<int32_t> row = bufI32(0, kN);
        LocalTensor<int32_t> modT2 = bufI32(kN, kN);
        LocalTensor<int32_t> accLine = bufI32(2 * kN, kN);
        LocalTensor<int32_t> fLoc = bufI32(3 * kN, kN);

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

        for (int32_t j = 0; j < kK; ++j) {
            LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(fPoly, sGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], kN);
            DataCopy(gPoly, uGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], kN);
            inQueueF_.EnQue(fPoly);
            inQueueG_.EnQue(gPoly);
            fPoly = inQueueF_.DeQue<int32_t>();
            gPoly = inQueueG_.DeQue<int32_t>();
            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            inQueueG_.FreeTensor(gPoly);
            if (j == 0) {
                DataCopy(accLine, row, kN);
            } else {
                Add(accLine, accLine, row, kN);
            }
        }
        hat_ip::mod_q_final_vec(accLine, kHatQ, fLoc, modT2, kN);
        DataCopy(wGm_[0], accLine, kN);
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

} // namespace

extern "C" __global__ __aicore__ void f203_decrypt_su_dot(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    KernelSuDot op;
    op.Init(sHatGm, uHatGm, wHatGm);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void f203_decrypt_su_dot_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *sHatGm, uint8_t *uHatGm,
                            uint8_t *wHatGm)
{
    f203_decrypt_su_dot<<<blockDim, l2ctrl, stream>>>(sHatGm, uHatGm, wHatGm);
}
#endif
