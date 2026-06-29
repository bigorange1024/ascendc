/*
 * Toy Alg.11/12 — AscendC shell (Add-custom pattern).
 * GM → UB (DataCopy) → UB compute (ALG11_IMPL=0 C / 1 vector) → GM.
 */
#include "kernel_operator.h"
#if ALG11_IMPL == 1 && ALG11_MEM_OPS == 1 && !defined(ASCENDC_CPU_DEBUG)
#include "alg11_rom_tables.cpp"
#endif
#include "multiply_ntts_ub.hpp"
#include "tiling.h"

using namespace AscendC;

constexpr int32_t BUFFER_NUM = 1;
constexpr int32_t TILE_NUM = 1;
constexpr int32_t TILE_LENGTH = alg11_tiling::kN;
constexpr int32_t USE_CORE_NUM = alg11_tiling::kBlockDim;
#if ALG11_IMPL == 1
constexpr int32_t VEC_WS_LENGTH = alg11_tiling::kVecWsInts;
constexpr int32_t GAMMA_LUT_LENGTH = alg11_tiling::kPairCount;
constexpr int32_t GATHER_IDX_LENGTH = alg11_tiling::kPairCount;
constexpr int32_t INTERLEAVE_IDX_LENGTH = alg11_tiling::kInterleaveReorderCount;
#endif

class KernelMultiplyNTTs {
public:
    __aicore__ inline KernelMultiplyNTTs() {}

    __aicore__ inline void Init(GM_ADDR f, GM_ADDR g, GM_ADDR h)
    {
        fGm.SetGlobalBuffer((__gm__ int32_t *)f + GetBlockIdx() * TILE_LENGTH, TILE_LENGTH);
        gGm.SetGlobalBuffer((__gm__ int32_t *)g + GetBlockIdx() * TILE_LENGTH, TILE_LENGTH);
        hGm.SetGlobalBuffer((__gm__ int32_t *)h + GetBlockIdx() * TILE_LENGTH, TILE_LENGTH);
        pipe.InitBuffer(inQueueF, BUFFER_NUM, TILE_LENGTH * sizeof(int32_t));
        pipe.InitBuffer(inQueueG, BUFFER_NUM, TILE_LENGTH * sizeof(int32_t));
        pipe.InitBuffer(outQueueH, BUFFER_NUM, TILE_LENGTH * sizeof(int32_t));
#if ALG11_IMPL == 1 && !defined(ASCENDC_CPU_DEBUG)
        pipe.InitBuffer(wsQue, BUFFER_NUM, VEC_WS_LENGTH * sizeof(int32_t));
        pipe.InitBuffer(gammaLutQue, BUFFER_NUM, GAMMA_LUT_LENGTH * sizeof(int32_t));
#if ALG11_MEM_OPS == 1
        pipe.InitBuffer(gatherEvenQue, BUFFER_NUM, GATHER_IDX_LENGTH * sizeof(int32_t));
        pipe.InitBuffer(gatherOddQue, BUFFER_NUM, GATHER_IDX_LENGTH * sizeof(int32_t));
        pipe.InitBuffer(interleaveReorderQue, BUFFER_NUM, INTERLEAVE_IDX_LENGTH * sizeof(int32_t));
#endif
        LocalTensor<int32_t> gammaLocal = gammaLutQue.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
#if ALG11_MEM_OPS == 1
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
#endif
        alg11_ub::init_rom_luts_ub(romUb_, GAMMA_LUT_LENGTH);
        gammaLutQue.EnQue(gammaLocal);
#if ALG11_MEM_OPS == 1
        gatherEvenQue.EnQue(gatherEvenLocal);
        gatherOddQue.EnQue(gatherOddLocal);
        interleaveReorderQue.EnQue(interleaveLocal);
#endif
#endif
    }

    __aicore__ inline void Process()
    {
        constexpr int32_t loopCount = TILE_NUM * BUFFER_NUM;
        for (int32_t i = 0; i < loopCount; ++i) {
            CopyIn(i);
            Compute(i);
            CopyOut(i);
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t progress)
    {
        LocalTensor<int32_t> fLocal = inQueueF.AllocTensor<int32_t>();
        LocalTensor<int32_t> gLocal = inQueueG.AllocTensor<int32_t>();
        DataCopy(fLocal, fGm[progress * TILE_LENGTH], TILE_LENGTH);
        DataCopy(gLocal, gGm[progress * TILE_LENGTH], TILE_LENGTH);
        inQueueF.EnQue(fLocal);
        inQueueG.EnQue(gLocal);
    }

    __aicore__ inline void Compute(int32_t progress)
    {
        (void)progress;
        LocalTensor<int32_t> fLocal = inQueueF.DeQue<int32_t>();
        LocalTensor<int32_t> gLocal = inQueueG.DeQue<int32_t>();
        LocalTensor<int32_t> hLocal = outQueueH.AllocTensor<int32_t>();

#if ALG11_IMPL == 1 && !defined(ASCENDC_CPU_DEBUG)
        LocalTensor<int32_t> wsLocal = wsQue.AllocTensor<int32_t>();
        LocalTensor<int32_t> gammaLocal = gammaLutQue.DeQue<int32_t>();
        alg11_vec::RomUbLuts rom;
        rom.gammaV = gammaLocal;
#if ALG11_MEM_OPS == 1
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue.DeQue<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue.DeQue<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue.DeQue<int32_t>();
        rom.gatherEvenByte = gatherEvenLocal;
        rom.gatherOddByte = gatherOddLocal;
        rom.interleaveReorderByte = interleaveLocal;
#endif
        alg11_ub::compute_on_ub(hLocal, fLocal, gLocal, wsLocal, rom);
        gammaLutQue.EnQue(gammaLocal);
#if ALG11_MEM_OPS == 1
        gatherEvenQue.EnQue(gatherEvenLocal);
        gatherOddQue.EnQue(gatherOddLocal);
        interleaveReorderQue.EnQue(interleaveLocal);
#endif
        wsQue.FreeTensor(wsLocal);
#elif ALG11_IMPL == 1
        alg11_ub::compute_on_ub_scalar(hLocal, fLocal, gLocal);
#else
        alg11_ub::compute_on_ub(hLocal, fLocal, gLocal);
#endif

        outQueueH.EnQue(hLocal);
        inQueueF.FreeTensor(fLocal);
        inQueueG.FreeTensor(gLocal);
    }

    __aicore__ inline void CopyOut(int32_t progress)
    {
        LocalTensor<int32_t> hLocal = outQueueH.DeQue<int32_t>();
        DataCopy(hGm[progress * TILE_LENGTH], hLocal, TILE_LENGTH);
        outQueueH.FreeTensor(hLocal);
    }

private:
    TPipe pipe;
    TQue<QuePosition::VECIN, BUFFER_NUM> inQueueF;
    TQue<QuePosition::VECIN, BUFFER_NUM> inQueueG;
    TQue<QuePosition::VECOUT, BUFFER_NUM> outQueueH;
#if ALG11_IMPL == 1
    TQue<QuePosition::VECIN, BUFFER_NUM> wsQue;
    TQue<QuePosition::VECIN, BUFFER_NUM> gammaLutQue;
#if ALG11_MEM_OPS == 1
    TQue<QuePosition::VECIN, BUFFER_NUM> gatherEvenQue;
    TQue<QuePosition::VECIN, BUFFER_NUM> gatherOddQue;
    TQue<QuePosition::VECIN, BUFFER_NUM> interleaveReorderQue;
#endif
    alg11_vec::RomUbLuts romUb_;
#endif
    GlobalTensor<int32_t> fGm;
    GlobalTensor<int32_t> gGm;
    GlobalTensor<int32_t> hGm;
};

extern "C" __global__ __aicore__ void multiply_ntts_custom(GM_ADDR f, GM_ADDR g, GM_ADDR h)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= USE_CORE_NUM) {
        return;
    }
    KernelMultiplyNTTs op;
    op.Init(f, g, h);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void multiply_ntts_custom_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *f, uint8_t *g, uint8_t *h)
{
    multiply_ntts_custom<<<blockDim, l2ctrl, stream>>>(f, g, h);
}
#endif
