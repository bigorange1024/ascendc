/**
 * @file mmad_custom.cpp
 * @brief T01：单段 CrossCore flag 1/3（同构 Encrypt NTT 握手）+ TRACE 编号。
 *
 * 图谱：D-EXP-T01 → Q-REPRO-ON-SIM / D-NEAR-TOYS。
 * 握手（禁止 SyncAll@AIC-Wait、禁止自造 SoftSync、禁止 GATE/INTT）：
 *   双 AIV：桩写 S0 半片 → TRACE(SET1) → SET(1)
 *   AIC：WAIT(1) → TRACE → 极轻 MMAD 16×32×32 → TRACE → SET(3)
 *   双 AIV：WAIT(3) → TRACE → 写完成标记
 *
 * TRACE：GT-4 式 DataCopy 写槽（禁 AIC 标量写 GM）；Host 按码表打印 KB 编号。
 * 不对算法正确性；验收只认 SIM 进程正常结束 + TRACE 可见。
 *
 * AscendC API：CrossCoreSetFlag/WaitFlag、DataCopy、Duplicate(int32)、Mmad/Fixpipe/Nd2Nz
 * 均复用查阅索引 GT-20260903-1 / GT-4 记录；本刀无新增 API（硬约束禁止改索引目录）。
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
 * TRACE 写槽（GT-4）：LocalTensor 赋值后 DataCopy 到 GM；禁止 `__gm__ int32` 直写。
 *
 * 背景：AIC 标量写 TRACE 槽 Host 假空（J-FAIL-AIC-SCALAR-TRACE / KB X9）。
 * 布局：逻辑槽 slot → GM 偏移 slot*8 个 int32（32B 对齐块）；块首元素置 1。
 * 路径：
 *   - AIV：VECOUT UB 上 Duplicate(0)+SetValue(0,1) → DataCopy 到槽块（双 AIV 各写本核槽）
 *   - AIC：Cube 无 UB Duplicate；从 ws+TRACE_ONES（Host 预填全 1）DataCopy→A1→槽块
 * 未采用：标量 `trace[slot]=1`；AIC Wait 期间 SyncAll。
 *
 * @param traceGm TRACE GM 基址（可空）
 * @param ws      workspace（AIC 读 TRACE_ONES；AIV 不用）
 * @param slot    逻辑槽（见 ToyTraceSlot）
 * @param aic     true=AIC
 * @param subBlockID AIV 子块号（本函数不按 sub 过滤；调用方选对槽）
 */
__aicore__ inline void ToyTraceMark(GM_ADDR traceGm, GM_ADDR ws, ToyTraceSlot slot, const bool aic,
                                    int32_t /*subBlockID*/)
{
    if (traceGm == nullptr) {
        return;
    }

    constexpr uint32_t kAlign = static_cast<uint32_t>(tiling::kTraceAlignInts);
    const uint32_t slotOffInts = static_cast<uint32_t>(slot) * kAlign;

    AscendC::GlobalTensor<int32_t> dstGm;
    dstGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm) + slotOffInts, kAlign);

    AscendC::TPipe pipe;
    if (aic) {
        // Cube：L1(A1) 中转；源为 Host 预填的全 1 模板
        AscendC::GlobalTensor<int32_t> onesGm;
        onesGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + tiling::TRACE_ONES), kAlign);

        AscendC::TQue<AscendC::TPosition::A1, 1> a1Q;
        pipe.InitBuffer(a1Q, 1, kAlign * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> t = a1Q.AllocTensor<int32_t>();
        AscendC::DataCopy(t, onesGm, kAlign);
        a1Q.EnQue(t);
        t = a1Q.DeQue<int32_t>();
        AscendC::DataCopy(dstGm, t, kAlign);
        a1Q.FreeTensor(t);
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        // Vector：UB(VECOUT) 写 1 再搬到槽块
        AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
        pipe.InitBuffer(outQ, 1, kAlign * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> t = outQ.AllocTensor<int32_t>();
        AscendC::Duplicate(t, static_cast<int32_t>(0), kAlign);
        t.SetValue(0, static_cast<int32_t>(1));
        outQ.EnQue(t);
        t = outQ.DeQue<int32_t>();
        AscendC::DataCopy(dstGm, t, kAlign);
        outQ.FreeTensor(t);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block。
 * @param out   [out] 完成标记（每 AIV 32B）
 * @param ws    [in/out] S0/LUT/MAT_C/TRACE_ONES；LUT 与 TRACE_ONES 由 host 预填
 * @param trace [out] TRACE 槽块；Host 预清零
 * @param tiling 占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace,
                                                    TilingData tiling)
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
        ToyTraceMark(trace, ws, TR_AIC_WAIT1, aic, subBlockID); // 401

        {
            AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                         static_cast<uint16_t>(tiling::kCols));
            mmad.Init();
            mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
            KYBER_PIPE_ALL();
        }

        ToyTraceMark(trace, ws, TR_AIC_SET3, aic, subBlockID); // 402
        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        // ---- AIV：桩哈希写半片 → SET(1)；等 SET(3) → 写完成标记 ----
        // 关键：StubHashSplit 的 TPipe 须先析构，再开 ToyTraceMark 局部 TPipe
        // （CPU 孪生：同核连续两个 VECOUT TPipe 会 set_flag 双置 abort）
        {
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_AIV_SPLIT;
            // TRACE SET1：AIV0→201，AIV1→301
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SET1 : TR_AIV1_SET1, aic, subBlockID);
            FsmSet(st); // 双 AIV 均 SET(1)，同构 Encrypt NTT
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        // TRACE WAIT3 后：AIV0→203，AIV1→303
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_WAIT3 : TR_AIV1_WAIT3, aic, subBlockID);

        {
            AivDoneMark mark(subBlockID);
            mark.Init(out);
            mark.Process();
            KYBER_PIPE_ALL();
        }
    }
}
