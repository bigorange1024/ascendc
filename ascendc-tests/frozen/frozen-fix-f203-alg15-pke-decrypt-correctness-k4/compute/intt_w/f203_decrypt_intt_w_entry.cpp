/**
 * @file f203_decrypt_intt_w_entry.cpp
 * @brief G4 INTT：u_hat/tr_hat NTT 域 → 时域 u/v 前段（mixPass=3，INTT LUT 由 host 写入 ws）。
 *
 * 算法与 G2 NTT 同 Stage1–3 骨架；差异仅在 workspace 前缀 LUT 为 kMlkemLimb6Intt_T_i8。
 * 输入 src [k,256] int32（NTT 域）；输出 dst [k,256] int32（时域 canonical）。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_decrypt_intt_w_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum InttMachineState : uint16_t {
    INTT_IDLE = 0,
    INTT_AIV_SPLIT,
    INTT_AIC_MMAD,
    INTT_AIV_PACK,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_intt_w_mix_pass = 3;
#endif

__aicore__ inline void intt_wait(InttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

__aicore__ inline void intt_set(InttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

#define INTT_WAIT intt_wait(state, AIC, subBlockID);
#define INTT_SET intt_set(state, AIC, subBlockID);

extern "C" __global__ __aicore__ void f203_decrypt_intt_w(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);

#ifdef ASCENDC_CPU_DEBUG
    const int32_t mixPass = g_f203_decrypt_intt_w_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif

    const bool runS1 = (mixPass == 0 || mixPass == 3);
    const bool runS2 = (mixPass == 1 || mixPass == 3);
    const bool runS3 = (mixPass == 2 || mixPass == 3);
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    InttMachineState state;

    if (AIC) {
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            state = INTT_AIV_SPLIT;
            INTT_WAIT
        }
        state = INTT_AIC_MMAD;
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2Pack) {
            state = INTT_AIV_PACK;
            INTT_SET
        }
    } else {
        if (runS1) {
            state = INTT_AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                INTT_SET
            }
        }
        if (runS2) {
            state = INTT_AIV_PACK;
            INTT_WAIT
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        if (runS3) {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(dst, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}
