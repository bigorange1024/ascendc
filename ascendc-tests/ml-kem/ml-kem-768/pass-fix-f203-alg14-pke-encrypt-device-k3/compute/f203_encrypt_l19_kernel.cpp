/**
 * @file f203_encrypt_l19_kernel.cpp
 * @brief Alg.14 行 18–19 组合段（无 NTT y）：û←Âᵀ∘ŷ；u←INTT(û)+e₁。
 *
 * FSM：双 AIV halfrows 内积写 uNtt → AIV0 SET IP_DONE → AIC 等 → INTT S1/MMAD/Pack → +e₁。
 * 本段不含 NTT(y)；输入 y_hat 须已由 ntt_y 或 host 写满。
 * Golden：y_hat + a_hat + e1 → u_ntt + u（CPU 三 launch 中段）。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_encrypt_at_jp.hpp"
#include "f203_l18_l19_tiling.h"
#include "f203_mod_q/mod_q_add.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum FsmState : uint16_t {
    ST_IP_AIV_DONE = 4,     /**< 双 AIV 内积写完 uNtt（仅 AIV0 Set） */
    ST_INTT_AIV_SPLIT = 5,  /**< INTT Stage1 完成 */
    ST_INTT_AIC_MMAD = 6,   /**< 保留 */
    ST_INTT_AIV_PACK = 7,   /**< AIC INTT MMAD 完成，可 Pack */
};

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

/**
 * MIX：内积 + INTT + e₁。
 * @param uOut u；@param yHat ŷ；@param uNtt û 中间；@param aHat Â；@param e1；@param ws
 * INTT LUT 使用 LUT_INTT_*（与融合核 workspace 尾部一致）。
 */
extern "C" __global__ __aicore__ void f203_encrypt_l19(GM_ADDR uOut, GM_ADDR yHat, GM_ADDR uNtt, GM_ADDR aHat,
                                                       GM_ADDR e1, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        // AIC 空等内积 → 等 INTT S1 → 四路 INTT MMAD → SET Pack
        st = ST_IP_AIV_DONE;
        FsmWait(st);
        st = ST_INTT_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(nttMRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_INTT_EVEN_STACKED);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_INTT_ODD_STACKED);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_INTT_EVEN_STACKED + n * lutPlanarCols);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_INTT_ODD_STACKED + n * lutPlanarCols);
        KYBER_PIPE_ALL();
        st = ST_INTT_AIV_PACK;
        FsmSet(st);
    } else {
        // AIV：k3 行区间内积写 uNtt；AIV0 写 0..1，AIV1 写 2。
        const int32_t pBegin = (subBlockID == 0) ? 0 : 2;
        const int32_t pEnd = (subBlockID == 0) ? 2 : 3;
        encrypt_at_jp::innerproduct_halfrows_to_gm(aHat, yHat, uNtt, pBegin, pEnd);
        KYBER_PIPE_ALL();
        if (subBlockID == 0) {
            st = ST_IP_AIV_DONE;
            FsmSet(st);
        }
        KYBER_PIPE_ALL();

        // INTT S1(uNtt) → Pack → RouteA → +e₁
        st = ST_INTT_AIV_SPLIT;
        {
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, uNtt);
            split.Process();
            KYBER_PIPE_ALL();
        }
        FsmSet(st);

        st = ST_INTT_AIV_PACK;
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
            merge.Init(uOut, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
        f203_mod_q::mod_q_add_gm_polyrows(uOut, uOut, e1, encrypt_at_jp::kQ, pBegin, pEnd, encrypt_at_jp::kN);
        KYBER_PIPE_ALL();
    }
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_l19_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *yHat,
                                    uint8_t *uNtt, uint8_t *aHat, uint8_t *e1, uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_l19<<<blockDim, l2ctrl, stream>>>(uOut, yHat, uNtt, aHat, e1, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
