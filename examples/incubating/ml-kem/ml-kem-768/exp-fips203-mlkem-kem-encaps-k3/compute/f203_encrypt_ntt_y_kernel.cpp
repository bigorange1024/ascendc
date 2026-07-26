/**
 * @file f203_encrypt_ntt_y_kernel.cpp
 * @brief Alg.14 行 16–17 单段：r̂ ← NTT(r)，MIX k=3（独立 launch）。
 *
 * 流水线：AIV Split(y) → AIC 四路 MMAD(NTT LUT) → AIV Pack → RouteA → yHat GM。
 * Golden I/O：input/r.bin → output/r_hat.bin（与 gen_data / mlkem_ref NTT 对拍）。
 * 本段不含内积与 INTT；CPU 三 launch 与 SIM 分段调试共用本核。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/** CrossCore FSM：AIV 完成 S1 后 AIC MMAD，再通知 AIV Pack/Merge */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1, /**< AIV Stage1 写 S0 完成 */
    ST_AIC_MMAD = 2,  /**< 保留语义；本核 AIC 在 Wait(SPLIT) 后直接算 */
    ST_AIV_PACK = 3,  /**< AIC 四路临时写完，AIV 可 Pack */
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_ntt_y_mix_pass = 3;
#endif

/** 等待对端 CrossCore 标志；前后 PIPE_ALL 保证可见性 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore 标志，通知对端 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * MIX 核：r̂ ← NTT(r)。
 * @param yHat 输出 [kK,N] int32；@param ySrc 输入 r [kK,N]；@param ws workspace（LUT+S0+mat_c）
 * @param tiling tileLength=N、kPolys=kK
 * 前置：ws 已由 host 填入 NTT even/odd LUT；KERNEL_TYPE_MIX_AIC_1_2
 */
extern "C" __global__ __aicore__ void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        // —— AIC：等 AIV S1 → 四次 MMAD（lo/hi × even/odd NTT LUT）→ 通知 Pack ——
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
        // —— AIV：S1 Split → SET → 等 Pack → 平面打包 → RouteA 写 yHat ——
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
/** Host 侧 launch 包装：blockDim / stream / tiling 指针下发 */
extern "C" void f203_encrypt_ntt_y_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *yHat, uint8_t *ySrc,
                                      uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_ntt_y<<<blockDim, l2ctrl, stream>>>(yHat, ySrc, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
