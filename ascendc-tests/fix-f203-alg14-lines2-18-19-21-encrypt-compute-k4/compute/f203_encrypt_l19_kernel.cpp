/**
 * @file f203_encrypt_l19_kernel.cpp
 * @brief 行 19 单段：û←Âᵀ∘ŷ；u←INTT(û)+e₁。
 *
 * 双 AIV halfrows 内积（读 GM 全量 ŷ）→ SET IP_DONE → AIC INTT MMAD → AIV S3 → +e₁。
 * 本段不含 NTT(y)；输入 y_hat 须已由行 18 写满 GM。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_encrypt_at_jp_scalar.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum FsmState : uint16_t {
    ST_IP_AIV_DONE = 4,
    ST_INTT_AIV_SPLIT = 5,
    ST_INTT_AIC_MMAD = 6,
    ST_INTT_AIV_PACK = 7,
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

__aicore__ inline void AddE1Halfrows(GM_ADDR uOut, GM_ADDR uTime, GM_ADDR e1, int32_t subBlockID)
{
    constexpr int32_t kN = encrypt_at_jp::kN;
    constexpr int32_t kQ = encrypt_at_jp::kQ;
    const int32_t pBegin = subBlockID * 2;
    const int32_t pEnd = pBegin + 2;
    auto *uGm = reinterpret_cast<__gm__ int32_t *>(uOut);
    const auto *tGm = reinterpret_cast<const __gm__ int32_t *>(uTime);
    const auto *eGm = reinterpret_cast<const __gm__ int32_t *>(e1);
    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const uint32_t off = static_cast<uint32_t>(p) * static_cast<uint32_t>(kN) + static_cast<uint32_t>(c);
            int32_t v = tGm[off] + eGm[off];
            v %= kQ;
            if (v < 0) {
                v += kQ;
            }
            uGm[off] = v;
        }
    }
}

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
        st = ST_IP_AIV_DONE;
        FsmWait(st);
        st = ST_INTT_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
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
        const int32_t pBegin = subBlockID * 2;
        const int32_t pEnd = pBegin + 2;
        encrypt_at_jp::innerproduct_halfrows_scalar(aHat, yHat, uNtt, pBegin, pEnd);
        KYBER_PIPE_ALL();
        if (subBlockID == 0) {
            st = ST_IP_AIV_DONE;
            FsmSet(st);
        }
        KYBER_PIPE_ALL();

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
        AddE1Halfrows(uOut, uOut, e1, subBlockID);
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
