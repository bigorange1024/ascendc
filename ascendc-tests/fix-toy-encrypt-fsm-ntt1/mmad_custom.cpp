/**
 * @file mmad_custom.cpp
 * @brief fix-toy-encrypt-fsm-ntt1：单段 CrossCore flag 1/3（同构 Encrypt NTT 握手）。
 *
 * 图谱：Q-TOY-NTT；形态见 F-NTT-FLAGS / F-CC-CHANNEL / F-MIX-TYPE。
 * 握手（禁止 SyncAll@AIC-Wait、禁止自造 SoftSync）：
 *   双 AIV：桩哈希写 S0 半片 → SET(1)
 *   AIC：WAIT(1) → 极轻 MMAD 16×32×32 → SET(3)
 *   双 AIV：WAIT(3) → 写完成标记
 *
 * 不对算法正确性；验收只认 SIM 进程正常结束。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM：与 Encrypt NTT(y) 同构字面量。
 * ST_AIV_SPLIT=1：AIV→AIC；ST_AIV_PACK=3：AIC→AIV；2 保留未 Wait。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2, /**< 保留编号，本玩具不单独 Wait */
    ST_AIV_PACK = 3,
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2> 与 F-CC-CHANNEL 一致。
 * @param st 期望 flag（1 或 3）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 向对端广播 FSM 完成。
 * @param st 要置位的 flag
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block。
 * @param out [out] 完成标记（每 AIV 4B）
 * @param ws  [in/out] S0/LUT/MAT_C；LUT 由 host 预填单位阵
 * @param tiling 占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // GetSubBlockNum()==1 → AIC；否则 AIV，subBlockIdx 区分 0/1
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        // ---- AIC：等双 AIV SET(1) → 极轻 Cube → SET(3) ----
        // 关键：AIC 仍 Wait 时禁止对 AIV SyncAll（D-NO-SYNCALL-WHILE-AIC-WAIT）
        st = ST_AIV_SPLIT;
        FsmWait(st);

        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();

        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        // ---- AIV：桩哈希写半片 → SET(1)；等 SET(3) → 写完成标记 ----
        {
            st = ST_AIV_SPLIT;
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(st); // 双 AIV 均 SET(1)，同构 Encrypt NTT
        }

        st = ST_AIV_PACK;
        FsmWait(st);

        AivDoneMark mark(subBlockID);
        mark.Init(out);
        mark.Process();
        KYBER_PIPE_ALL();
    }
}
