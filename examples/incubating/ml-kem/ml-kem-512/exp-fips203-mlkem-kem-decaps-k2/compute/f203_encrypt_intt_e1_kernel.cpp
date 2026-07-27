/**
 * @file f203_encrypt_intt_e1_kernel.cpp
 * @brief Alg.14 行 19 时域段（CPU 三 launch）：u ← INTT(û) + e₁。
 *
 * 流水线：AIV Split(uNtt) → AIC INTT LUT MMAD → Pack → RouteA → mod_q_add(e₁)。
 * 注意：本核 LUT 偏移用 LUT_NTT_*（与独立 INTT 探针 workspace 布局一致）；
 * 融合单 launch 的 INTT 见 f203_encrypt_l18_l19_kernel（LUT_INTT_* + batch4）。
 *
 * Golden I/O：input/u_ntt.bin + e1.bin → output/u.bin。
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
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
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
 * MIX：INTT(û)+e₁（k=2，仅 CPU 分段辅助，不生成 v）。
 * @param uOut 输出 u；@param uNtt 输入 û；@param e1 时域噪声；@param ws workspace
 */
extern "C" __global__ __aicore__ void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws,
                                                           TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        // AIC：等 S1 → 四路 MMAD（本路径 LUT 落在 LUT_NTT_* 槽）→ SET Pack
        st = ST_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(nttMRowsLogic), coeffN, static_cast<uint16_t>(halfN));
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
        // AIV：S1(uNtt) → Pack → RouteA → 半行加 e₁（mod q）
        st = ST_AIV_SPLIT;
        {
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, uNtt);
            split.Process();
            KYBER_PIPE_ALL();
        }
        FsmSet(st);

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
            merge.Init(uOut, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
        // 行 19：u ← INTT(û) + e₁；k2 分片为 AIV0 行 0..1、AIV1 无语义行。
        {
            const int32_t pBegin = (subBlockID == 0) ? 0 : 2;
            const int32_t pEnd = (subBlockID == 0) ? 2 : 2;
            if (pBegin < pEnd) {
                f203_mod_q::mod_q_add_gm_polyrows(uOut, uOut, e1, encrypt_at_jp::kQ, pBegin, pEnd, encrypt_at_jp::kN);
            }
        }
        KYBER_PIPE_ALL();
    }
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_intt_e1_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *uNtt,
                                        uint8_t *e1, uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_intt_e1<<<blockDim, l2ctrl, stream>>>(uOut, uNtt, e1, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
