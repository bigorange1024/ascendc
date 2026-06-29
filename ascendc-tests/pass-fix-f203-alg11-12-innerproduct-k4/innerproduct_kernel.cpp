/*
 * polyvec NTT 域内积（Alg.13 行 18，无 ê）— 默认 4×4×1（ML-KEM K=4 KeyGen）
 *
 *   t̂[p] = mod_q( Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) )    p = 0..P_OUT-1
 *
 * GM 与 alg13 一致（行主序 a_hat）：
 *   a_hat[(p*K+j)*N + c]   s_hat[j*N + c]   t_hat[p*N + c]
 *
 * 单 AIV、blockDim=1；j→p 外循环，ŝ[j] 跨 p 复用；Â[p,j] 按行主序逐行搬入。
 */
#include "kernel_operator.h"
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "alg11_rom_tables.cpp"
#endif
#include "innerproduct_layout.h"
#include "innerproduct_mod.hpp"
#include "innerproduct_tiling.h"
#if defined(ASCENDC_CPU_DEBUG)
#include "innerproduct_ub_scalar.hpp"
#endif
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

constexpr int32_t kN = innerproduct_tiling::kN;
constexpr int32_t kPOut = innerproduct_tiling::kPOut;
constexpr int32_t kSVec = innerproduct_tiling::kSVec;
constexpr int32_t kRomPairs = innerproduct_tiling::kRomPairCount;
constexpr int32_t kUseCores = innerproduct_tiling::kBlockDim;
constexpr int32_t kHatQ = innerproduct_tiling::kHatQ;

class KernelHatInnerProduct {
public:
    __aicore__ inline KernelHatInnerProduct() {}

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void Init(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat)
    {
        aHatGm_ = aHat;
        sHatGm_ = sHat;
        tHatGm_ = tHat;

        aGm_.SetGlobalBuffer((__gm__ int32_t *)aHat, kPOut * kSVec * kN);
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sHat, kSVec * kN);
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kPOut * kN);

#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(innerproduct_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(innerproduct_tiling::kVecWsInts * sizeof(int32_t)));
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

    __aicore__ inline void ProcessFullPoly()
    {
        LocalTensor<int32_t> row = bufI32(innerproduct_tiling::kOffRow, kN);
        LocalTensor<int32_t> modT2 = bufI32(innerproduct_tiling::kOffModT2, kN);
        LocalTensor<int32_t> outLine = bufI32(innerproduct_tiling::kOffOutLine, kPOut * kN);
        LocalTensor<int32_t> fLoc = bufI32(innerproduct_tiling::kOffAcc, kN);

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

        for (int32_t j = 0; j < kSVec; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, sGm_[innerproduct_layout::s_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (int32_t p = 0; p < kPOut; ++p) {
                const uint32_t lineOff = static_cast<uint32_t>(p) * static_cast<uint32_t>(kN);
                LocalTensor<int32_t> lineP = outLine[lineOff];

                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                DataCopy(fPoly, aGm_[innerproduct_layout::a_hat_offset(p, j)], kN);
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();

                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();

                if (j == 0) {
                    DataCopy(lineP, row, kN);
                    ALG11_PIPE_MTE2();
                } else {
                    Add(lineP, lineP, row, kN);
                    ALG11_PIPE_ALL();
                }
            }

            inQueueG_.FreeTensor(gPoly);
        }

        for (int32_t p = 0; p < kPOut; ++p) {
            LocalTensor<int32_t> lineP = outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kN)];
#if defined(ASCENDC_CPU_DEBUG)
            hat_ip::mod_q_final_vec(lineP, kHatQ, kN);
#else
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, modT2, kN);
#endif
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        DataCopy(tGm_[0], outLine, kPOut * kN);
        ALG11_PIPE_MTE2();
    }

    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::innerproduct_scalar_a_hat(aHatGm_, sHatGm_, tHatGm_);
#else
        ProcessFullPoly();
#endif
    }

private:
    GM_ADDR aHatGm_{nullptr};
    GM_ADDR sHatGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
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
    GlobalTensor<int32_t> aGm_;
    GlobalTensor<int32_t> sGm_;
    GlobalTensor<int32_t> tGm_;
};

extern "C" __global__ __aicore__ void hat_innerproduct_k4_custom(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
    KernelHatInnerProduct op;
    op.Init(aHat, sHat, tHat);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void hat_innerproduct_k4_custom_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aHat, uint8_t *sHat,
                                   uint8_t *tHat)
{
    hat_innerproduct_k4_custom<<<blockDim, l2ctrl, stream>>>(aHat, sHat, tHat);
}
#endif
