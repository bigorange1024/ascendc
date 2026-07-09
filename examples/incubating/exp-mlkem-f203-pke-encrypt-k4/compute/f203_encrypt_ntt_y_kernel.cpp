/**
 * @file f203_encrypt_ntt_y_kernel.cpp
 * @brief 行 18 单段：ŷ ← NTT(y)，MIX k=4（与 stage123 同构，独立 launch）。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_ntt_y_mix_pass = 3;
#endif

__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

extern "C" __global__ __aicore__ void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        st = ST_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_NTT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_NTT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_NTT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_NTT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        {
            st = ST_AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, ySrc);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(st);
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        {
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(yHat, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_ntt_y_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *yHat, uint8_t *ySrc,
                                      uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_ntt_y<<<blockDim, l2ctrl, stream>>>(yHat, ySrc, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
