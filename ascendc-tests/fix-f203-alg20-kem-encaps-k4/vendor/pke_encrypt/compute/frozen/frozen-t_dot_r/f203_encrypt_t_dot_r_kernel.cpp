/**
 * @file f203_encrypt_t_dot_r_kernel.cpp
 * @brief G3 L5：t̂·r̂ 单 poly 内积（Alg.14 v 路径，NTT 域）。
 */
#include "kernel_operator.h"
#include "f203_encrypt_t_dot_r_layout.h"
#include "innerproduct_mod.hpp"
#include "innerproduct_tiling.h"
#if defined(ASCENDC_CPU_DEBUG)
#include "f203_encrypt_t_dot_r_ub_scalar.hpp"
#endif
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

constexpr int32_t kN = innerproduct_tiling::kN;
constexpr int32_t kK = innerproduct_tiling::kSVec;
constexpr int32_t kRomPairs = innerproduct_tiling::kRomPairCount;
constexpr int32_t kUseCores = innerproduct_tiling::kBlockDim;
constexpr int32_t kHatQ = innerproduct_tiling::kHatQ;

class KernelTDotR {
public:
    __aicore__ inline KernelTDotR() {}

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void Init(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat)
    {
        tHatGm_ = tHat;
        rHatGm_ = rHat;
        trHatGm_ = trHat;

        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kK * kN);
        rGm_.SetGlobalBuffer((__gm__ int32_t *)rHat, kK * kN);
        outGm_.SetGlobalBuffer((__gm__ int32_t *)trHat, kN);

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

        for (int32_t j = 0; j < kK; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, rGm_[f203_t_dot_r_layout::polyvec_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            DataCopy(fPoly, tGm_[f203_t_dot_r_layout::polyvec_offset(j)], kN);
            inQueueF_.EnQue(fPoly);
            fPoly = inQueueF_.DeQue<int32_t>();

            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            ALG11_PIPE_ALL();

            LocalTensor<int32_t> line0 = outLine[0];
            if (j == 0) {
                DataCopy(line0, row, kN);
                ALG11_PIPE_MTE2();
            } else {
                Add(line0, line0, row, kN);
                ALG11_PIPE_ALL();
            }
            inQueueG_.FreeTensor(gPoly);
        }

        {
            LocalTensor<int32_t> line0 = outLine[0];
            hat_ip::mod_q_final_vec(line0, kHatQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        DataCopy(outGm_[0], outLine, kN);
        ALG11_PIPE_MTE2();
    }

    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::t_dot_r_scalar(tHatGm_, rHatGm_, trHatGm_);
#else
        ProcessFullPoly();
#endif
    }

private:
    GM_ADDR tHatGm_{nullptr};
    GM_ADDR rHatGm_{nullptr};
    GM_ADDR trHatGm_{nullptr};
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
    GlobalTensor<int32_t> tGm_;
    GlobalTensor<int32_t> rGm_;
    GlobalTensor<int32_t> outGm_;
};

extern "C" __global__ __aicore__ void f203_encrypt_t_dot_r(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
    KernelTDotR op;
    op.Init(tHat, rHat, trHat);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_t_dot_r_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tHat, uint8_t *rHat,
                             uint8_t *trHat)
{
    f203_encrypt_t_dot_r<<<blockDim, l2ctrl, stream>>>(tHat, rHat, trHat);
}
#endif
