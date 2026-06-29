/*
 * ⛔ FROZEN SNAPSHOT — 勿编译、勿 fork。
 * 2026-06-17 冻结前完整 kernel（含 INNERPRODUCT_HALF_BATCH 二期路径）。
 * 判决书：FROZEN.md
 */
/*
 * 2×2×1 polyvec 内积：t̂[p] = mod( Σ_j Â[p,j]∘ŝ[j] )，p=0..1。
 * 单 AIV、单次 launch。
 * 一期 ProcessFullPoly（默认）；二期 ProcessHalfBatch（INNERPRODUCT_HALF_BATCH=1）。
 */
#include "kernel_operator.h"
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "alg11_rom_tables.cpp"
#endif
#include "hat_innerproduct_batch.hpp"
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
constexpr int32_t kHalfLen = innerproduct_tiling::kHalfLen;
constexpr int32_t kHalfPairs = innerproduct_tiling::kHalfPairCount;
constexpr int32_t kRomPairs = innerproduct_tiling::kRomPairCount;
constexpr int32_t kUseCores = innerproduct_tiling::kBlockDim;
constexpr int32_t kHatQ = innerproduct_tiling::kHatQ;

/** 1=二期 half 批处理；0=一期全 poly（CMake 默认 0） */
#ifndef INNERPRODUCT_HALF_BATCH
#define INNERPRODUCT_HALF_BATCH 0
#endif

class KernelHatInnerProduct {
public:
    __aicore__ inline KernelHatInnerProduct() {}

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void Init(GM_ADDR aCol, GM_ADDR sCol, GM_ADDR tHat)
    {
        aColGm_ = aCol;
        sColGm_ = sCol;
        tHatGm_ = tHat;

        aGm_.SetGlobalBuffer((__gm__ int32_t *)aCol, kPOut * kSVec * kN);
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sCol, kSVec * kN);
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kPOut * kN);

#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(innerproduct_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(innerproduct_tiling::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));
#if INNERPRODUCT_HALF_BATCH
        pipe_.InitBuffer(aBlockQue_, 1, static_cast<uint32_t>(kPOut * kN * sizeof(int32_t)));
#endif
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

#if INNERPRODUCT_HALF_BATCH
    __aicore__ inline void ProcessHalfBatch()
    {
        LocalTensor<int32_t> outLine = bufI32(innerproduct_tiling::kOffHalfOutLine, kPOut * kN);
        LocalTensor<int32_t> sB0 = bufI32(innerproduct_tiling::kOffSB0, kHalfPairs);
        LocalTensor<int32_t> sB1 = bufI32(innerproduct_tiling::kOffSB1, kHalfPairs);
        LocalTensor<int32_t> gammaSlice = bufI32(innerproduct_tiling::kOffGammaSlice, kHalfPairs);
        LocalTensor<int32_t> prod = bufI32(innerproduct_tiling::kOffHalfModT2, kHalfLen);
        LocalTensor<int32_t> sHalfBuf = bufI32(innerproduct_tiling::kOffHalfRow, kHalfLen);

        LocalTensor<int32_t> wsLocal = wsQue_.AllocTensor<int32_t>();
        alg11_vec::VecWs w;
        alg11_vec::bind_vec_ws(wsLocal, w, kRomPairs, romUb_);

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
            const uint32_t sOff = static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
            const uint32_t aColBase =
                static_cast<uint32_t>(j) * static_cast<uint32_t>(kPOut) * static_cast<uint32_t>(kN);

            LocalTensor<int32_t> sFull = inQueueG_.AllocTensor<int32_t>();
            DataCopy(sFull, sGm_[sOff], kN);
            inQueueG_.EnQue(sFull);
            sFull = inQueueG_.DeQue<int32_t>();

            LocalTensor<int32_t> aBlock = aBlockQue_.AllocTensor<int32_t>();
            DataCopy(aBlock, aGm_[aColBase], kPOut * kN);
            aBlockQue_.EnQue(aBlock);
            aBlock = aBlockQue_.DeQue<int32_t>();

            for (int32_t halfIdx = 0; halfIdx < 2; ++halfIdx) {
                const uint32_t subOff = static_cast<uint32_t>(halfIdx) * static_cast<uint32_t>(kHalfLen);
                const int32_t gammaOff = halfIdx * kHalfPairs;

                DataCopy(sHalfBuf, sFull[subOff], kHalfLen);
                ALG11_PIPE_MTE2();
                hat_ip::cache_s_half_blanes(sB0, sB1, sHalfBuf, w);

                for (int32_t p = 0; p < kPOut; ++p) {
                    const uint32_t lineOff =
                        static_cast<uint32_t>(p) * static_cast<uint32_t>(kN) + subOff;
                    const uint32_t aRowOff =
                        static_cast<uint32_t>(p) * static_cast<uint32_t>(kN) + subOff;

                    LocalTensor<int32_t> fHalf = inQueueF_.AllocTensor<int32_t>();
                    DataCopy(fHalf, aBlock[aRowOff], kHalfLen);
                    inQueueF_.EnQue(fHalf);
                    fHalf = inQueueF_.DeQue<int32_t>();

                    hat_ip::basemul_half_cached_s(prod, fHalf, sB0, sB1, w, rom, gammaSlice, gammaOff);
                    inQueueF_.FreeTensor(fHalf);
                    ALG11_PIPE_ALL();

                    if (j == 0) {
                        DataCopy(outLine[lineOff], prod, kHalfLen);
                        ALG11_PIPE_MTE2();
                    } else {
                        Add(outLine[lineOff], outLine[lineOff], prod, kHalfLen);
                        ALG11_PIPE_ALL();
                    }
                }
            }

            inQueueG_.FreeTensor(sFull);
            aBlockQue_.FreeTensor(aBlock);
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        LocalTensor<int32_t> fLoc = bufI32(innerproduct_tiling::kOffHalfRow, kN);
        LocalTensor<int32_t> gLoc = bufI32(innerproduct_tiling::kOffAPair, kN);
        for (int32_t p = 0; p < kPOut; ++p) {
            LocalTensor<int32_t> lineP = outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kN)];
#if defined(ASCENDC_CPU_DEBUG)
            hat_ip::mod_q_final_vec(lineP, kHatQ, kN);
#else
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, gLoc, kN);
#endif
            ALG11_PIPE_ALL();
        }

        DataCopy(tGm_[0], outLine, kPOut * kN);
        ALG11_PIPE_MTE2();
    }
#endif

    __aicore__ inline void ProcessFullPoly()
    {
        LocalTensor<int32_t> acc = bufI32(innerproduct_tiling::kOffAcc, kN);
        LocalTensor<int32_t> row = bufI32(innerproduct_tiling::kOffRow, kN);
        LocalTensor<int32_t> modT2 = bufI32(innerproduct_tiling::kOffModT2, kN);
        LocalTensor<int32_t> outLine = bufI32(innerproduct_tiling::kOffOutLine, kPOut * kN);

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

        for (int32_t p = 0; p < kPOut; ++p) {
            Duplicate(acc, static_cast<int32_t>(0), kN);
            ALG11_PIPE_ALL();

            for (int32_t j = 0; j < kSVec; ++j) {
                const uint32_t aOff = static_cast<uint32_t>(j) * static_cast<uint32_t>(kPOut) *
                                           static_cast<uint32_t>(kN) +
                                       static_cast<uint32_t>(p) * static_cast<uint32_t>(kN);
                const uint32_t sOff = static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);

                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
                DataCopy(fPoly, aGm_[aOff], kN);
                DataCopy(gPoly, sGm_[sOff], kN);
                inQueueF_.EnQue(fPoly);
                inQueueG_.EnQue(gPoly);
                fPoly = inQueueF_.DeQue<int32_t>();
                gPoly = inQueueG_.DeQue<int32_t>();

                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                inQueueG_.FreeTensor(gPoly);
                ALG11_PIPE_ALL();

                Add(acc, acc, row, kN);
                ALG11_PIPE_ALL();
            }

#if defined(ASCENDC_CPU_DEBUG)
            hat_ip::mod_q_final_vec(acc, kHatQ, kN);
#else
            hat_ip::mod_q_final_vec(acc, kHatQ, row, modT2, kN);
#endif
            ALG11_PIPE_ALL();
            DataCopy(outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kN)], acc, kN);
            ALG11_PIPE_MTE2();
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
        hat_ip::innerproduct_scalar_a_col(aColGm_, sColGm_, tHatGm_);
#else
#if INNERPRODUCT_HALF_BATCH
        ProcessHalfBatch();
#else
        ProcessFullPoly();
#endif
#endif
    }

private:
    GM_ADDR aColGm_{nullptr};
    GM_ADDR sColGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
    TPipe pipe_;
    TBuf<TPosition::VECCALC> scratch_;
    TQue<QuePosition::VECIN, 1> wsQue_;
    TQue<QuePosition::VECIN, 1> inQueueF_;
    TQue<QuePosition::VECIN, 1> inQueueG_;
#if INNERPRODUCT_HALF_BATCH
    TQue<QuePosition::VECIN, 1> aBlockQue_;
#endif
    TQue<QuePosition::VECIN, 1> gammaLutQue_;
    TQue<QuePosition::VECIN, 1> gatherEvenQue_;
    TQue<QuePosition::VECIN, 1> gatherOddQue_;
    TQue<QuePosition::VECIN, 1> interleaveReorderQue_;
    alg11_vec::RomUbLuts romUb_;
    GlobalTensor<int32_t> aGm_;
    GlobalTensor<int32_t> sGm_;
    GlobalTensor<int32_t> tGm_;
};

extern "C" __global__ __aicore__ void hat_innerproduct_k4_custom(GM_ADDR aCol, GM_ADDR sCol, GM_ADDR tHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
    KernelHatInnerProduct op;
    op.Init(aCol, sCol, tHat);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void hat_innerproduct_k4_custom_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aCol, uint8_t *sCol,
                                 uint8_t *tHat)
{
    hat_innerproduct_k4_custom<<<blockDim, l2ctrl, stream>>>(aCol, sCol, tHat);
}
#endif
