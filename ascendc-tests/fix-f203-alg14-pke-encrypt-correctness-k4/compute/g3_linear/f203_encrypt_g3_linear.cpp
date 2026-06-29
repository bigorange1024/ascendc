/**
 * @file f203_encrypt_g3_linear.cpp
 * @brief G3 线性层：Âᵀ·r̂ + t̂·r̂（单 TPipe 顺序 L4→L5，SIM 上避免双 TPipe/双 launch 写回异常）。
 */
#include "kernel_operator.h"
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "alg11_rom_tables.cpp"
#endif
#include "f203_encrypt_at_r_layout.h"
#include "f203_encrypt_layout.h"
#include "f203_encrypt_t_dot_r_layout.h"
#include "innerproduct_mod.hpp"
#include "innerproduct_tiling.h"
#if defined(ASCENDC_CPU_DEBUG)
#include "f203_encrypt_g3_ub_scalar.hpp"
#endif
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

namespace {

constexpr int32_t kIpN = innerproduct_tiling::kN;
constexpr int32_t kIpPOut = innerproduct_tiling::kPOut;
constexpr int32_t kIpSVec = innerproduct_tiling::kSVec;
constexpr int32_t kIpRomPairs = innerproduct_tiling::kRomPairCount;
constexpr int32_t kIpUseCores = innerproduct_tiling::kBlockDim;
constexpr int32_t kIpHatQ = innerproduct_tiling::kHatQ;

/** 单 TPipe 承载 L4（Âᵀ·r̂）与 L5（t̂·r̂），ROM 只初始化一次。 */
class KernelG3Linear {
public:
    __aicore__ inline KernelG3Linear() {}

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void InitPipeOnce()
    {
#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(innerproduct_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(innerproduct_tiling::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kIpN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kIpN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, kIpRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kIpRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kIpRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kIpN * sizeof(int32_t));
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(romUb_, kIpRomPairs);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
#endif
    }

    __aicore__ inline void InitAtR(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR uHat)
    {
        aHatGm_ = aHat;
        rHatGm_ = rHat;
        uHatGm_ = uHat;
        aGm_.SetGlobalBuffer((__gm__ int32_t *)aHat, kIpPOut * kIpSVec * kIpN);
        rGm_.SetGlobalBuffer((__gm__ int32_t *)rHat, kIpSVec * kIpN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uHat, kIpPOut * kIpN);
#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        InitPipeOnce();
#endif
    }

    __aicore__ inline void InitTDotR(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat)
    {
        tHatGm_ = tHat;
        rHatGm_ = rHat;
        trHatGm_ = trHat;
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kIpSVec * kIpN);
        rGm_.SetGlobalBuffer((__gm__ int32_t *)rHat, kIpSVec * kIpN);
        trGm_.SetGlobalBuffer((__gm__ int32_t *)trHat, kIpN);
#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        InitPipeOnce();
#endif
    }

    __aicore__ inline void InitFull(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uHat, GM_ADDR trHat)
    {
        aHatGm_ = aHat;
        rHatGm_ = rHat;
        tHatGm_ = tHat;
        uHatGm_ = uHat;
        trHatGm_ = trHat;
        aGm_.SetGlobalBuffer((__gm__ int32_t *)aHat, kIpPOut * kIpSVec * kIpN);
        rGm_.SetGlobalBuffer((__gm__ int32_t *)rHat, kIpSVec * kIpN);
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kIpSVec * kIpN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uHat, kIpPOut * kIpN);
        trGm_.SetGlobalBuffer((__gm__ int32_t *)trHat, kIpN);
#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        InitPipeOnce();
#endif
    }

    __aicore__ inline void ProcessAtR()
    {
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::innerproduct_scalar_at_r(aHatGm_, rHatGm_, uHatGm_);
#else
        LocalTensor<int32_t> row = bufI32(innerproduct_tiling::kOffRow, kIpN);
        LocalTensor<int32_t> modT2 = bufI32(innerproduct_tiling::kOffModT2, kIpN);
        LocalTensor<int32_t> outLine = bufI32(innerproduct_tiling::kOffOutLine, kIpPOut * kIpN);
        LocalTensor<int32_t> fLoc = bufI32(innerproduct_tiling::kOffAcc, kIpN);
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
        for (int32_t j = 0; j < kIpSVec; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, rGm_[f203_at_r_layout::r_hat_offset(j)], kIpN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();
            for (int32_t p = 0; p < kIpPOut; ++p) {
                LocalTensor<int32_t> lineP = outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kIpN)];
                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                DataCopy(fPoly, aGm_[f203_at_r_layout::a_hat_offset_at(p, j)], kIpN);
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();
                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();
                if (j == 0) {
                    DataCopy(lineP, row, kIpN);
                    ALG11_PIPE_MTE2();
                } else {
                    Add(lineP, lineP, row, kIpN);
                    ALG11_PIPE_ALL();
                }
            }
            inQueueG_.FreeTensor(gPoly);
        }
        for (int32_t p = 0; p < kIpPOut; ++p) {
            LocalTensor<int32_t> lineP = outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kIpN)];
            hat_ip::mod_q_final_vec(lineP, kIpHatQ, fLoc, modT2, kIpN);
            ALG11_PIPE_ALL();
        }
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);
        DataCopy(uGm_[0], outLine, kIpPOut * kIpN);
        ALG11_PIPE_MTE2();
#endif
    }

    __aicore__ inline void ProcessTDotR()
    {
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::t_dot_r_scalar(tHatGm_, rHatGm_, trHatGm_);
#else
        LocalTensor<int32_t> row = bufI32(innerproduct_tiling::kOffRow, kIpN);
        LocalTensor<int32_t> modT2 = bufI32(innerproduct_tiling::kOffModT2, kIpN);
        LocalTensor<int32_t> trLine = bufI32(innerproduct_tiling::kOffOutLine, kIpN);
        LocalTensor<int32_t> fLoc = bufI32(innerproduct_tiling::kOffAcc, kIpN);
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
        for (int32_t j = 0; j < kIpSVec; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, rGm_[f203_t_dot_r_layout::polyvec_offset(j)], kIpN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();
            LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            DataCopy(fPoly, tGm_[f203_t_dot_r_layout::polyvec_offset(j)], kIpN);
            inQueueF_.EnQue(fPoly);
            fPoly = inQueueF_.DeQue<int32_t>();
            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            ALG11_PIPE_ALL();
            if (j == 0) {
                DataCopy(trLine, row, kIpN);
                ALG11_PIPE_MTE2();
            } else {
                Add(trLine, trLine, row, kIpN);
                ALG11_PIPE_ALL();
            }
            inQueueG_.FreeTensor(gPoly);
        }
        hat_ip::mod_q_final_vec(trLine, kIpHatQ, fLoc, modT2, kIpN);
        ALG11_PIPE_ALL();
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);
        DataCopy(trGm_[0], trLine, kIpN);
        ALG11_PIPE_MTE2();
#endif
    }

    __aicore__ inline void ProcessFull()
    {
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::g3_linear_scalar(aHatGm_, rHatGm_, tHatGm_, uHatGm_, trHatGm_);
#else
        ProcessAtR();
        ALG11_PIPE_ALL();
        ProcessTDotR();
#endif
    }

    /** 四 GM 输出布局：uTrOut = u_hat ‖ tr_hat（G5 SIM launch 用）。 */
    __aicore__ inline void InitFull4(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uTrOut)
    {
        InitFull(aHat, rHat, tHat, uTrOut, uTrOut + static_cast<int64_t>(F203_U_HAT_BYTES));
    }

private:
    GM_ADDR aHatGm_{nullptr};
    GM_ADDR rHatGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
    GM_ADDR uHatGm_{nullptr};
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
    GlobalTensor<int32_t> aGm_;
    GlobalTensor<int32_t> rGm_;
    GlobalTensor<int32_t> tGm_;
    GlobalTensor<int32_t> uGm_;
    GlobalTensor<int32_t> trGm_;
};

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_g3_linear(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uHat,
                                                             GM_ADDR trHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kIpUseCores) {
        return;
    }
    KernelG3Linear op;
    op.InitFull(aHat, rHat, tHat, uHat, trHat);
    op.ProcessFull();
}

/** 四 GM 指针变体：规避 SIM 五参 launch 507000（数学同 g3_linear）。 */
extern "C" __global__ __aicore__ void f203_encrypt_g3_linear4(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uTrOut)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kIpUseCores) {
        return;
    }
    KernelG3Linear op;
    op.InitFull4(aHat, rHat, tHat, uTrOut);
    op.ProcessFull();
}

extern "C" __global__ __aicore__ void f203_encrypt_at_r(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR uHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kIpUseCores) {
        return;
    }
    KernelG3Linear op;
    op.InitAtR(aHat, rHat, uHat);
    op.ProcessAtR();
}

extern "C" __global__ __aicore__ void f203_encrypt_t_dot_r(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kIpUseCores) {
        return;
    }
    KernelG3Linear op;
    op.InitTDotR(tHat, rHat, trHat);
    op.ProcessTDotR();
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_g3_linear_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aHat, uint8_t *rHat,
                               uint8_t *tHat, uint8_t *uHat, uint8_t *trHat)
{
    f203_encrypt_g3_linear<<<blockDim, l2ctrl, stream>>>(aHat, rHat, tHat, uHat, trHat);
}

void f203_encrypt_g3_linear4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aHat, uint8_t *rHat,
                                uint8_t *tHat, uint8_t *uTrOut)
{
    f203_encrypt_g3_linear4<<<blockDim, l2ctrl, stream>>>(aHat, rHat, tHat, uTrOut);
}

void f203_encrypt_at_r_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aHat, uint8_t *rHat, uint8_t *uHat)
{
    f203_encrypt_at_r<<<blockDim, l2ctrl, stream>>>(aHat, rHat, uHat);
}

void f203_encrypt_t_dot_r_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tHat, uint8_t *rHat,
                             uint8_t *trHat)
{
    f203_encrypt_t_dot_r<<<blockDim, l2ctrl, stream>>>(tHat, rHat, trHat);
}
#endif
