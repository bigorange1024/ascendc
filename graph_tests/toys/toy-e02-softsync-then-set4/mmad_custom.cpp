/**
 * @file mmad_custom.cpp
 * @brief E02 极简 MIX：L2 双 AIV 先 SoftSyncArrive 再 SET(4)；AIC Wait(4)+TRACE。
 *
 * 背景：图谱 D-exp-e02 — SoftSync（skel 单向定式）→ SET(4) 在新目录 ≥3 轮 SIM 可绿。
 * 结论：本核只做 SoftSync + CrossCore 握手与三位数字 TRACE；禁止 Encrypt/NTT/μ。
 * 未采用：双向 SoftSync、GATE alone、双 Cube、OMIT_SET4 发现实验。
 *
 * SoftSyncArrive（跟 decrypt skel）：AIV0 写哨兵 s[0]=1；AIV1 while(s[0]==0) 自旋。
 * OMIT_SOFTSYNC=1：跳过 SoftSync，直接 SET(4)（可选 weaken 对照，非 FAIL）。
 */
#include "kernel_operator.h"
#include "tiling.h"

#ifndef OMIT_SOFTSYNC
#define OMIT_SOFTSYNC 0
#endif

/** CrossCore flag：仅用 4 = AIV→AIC 齐步（L2 入口）。 */
enum FsmState : uint16_t {
    ST_SET4 = 4,
};

/** 设备侧 TRACE：只打三位十进制数字（知识库 §6）。 */
__aicore__ inline void TraceDigit(int code)
{
    AscendC::printf("%d\n", code);
}

/** 等待对端 CrossCore。禁止在 Wait 期间 SyncAll。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** 置位 CrossCore。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * SoftSyncArrive：生产单向定式 — AIV0 写哨兵 1，AIV1 自旋等非 0。
 * @param softSyncGm int32[2]；本核只用 slot0
 * @param slot 0（本 toy）
 * @param subBlockID AIV 编号
 * 背景：跟 pass-decrypt-skel SoftSyncArrive；禁止双向汇合 / SyncAll 替代。
 * OMIT_SOFTSYNC=1：两侧空操作后返回（测 SoftSync 对本骨架是否必要）。
 */
__aicore__ inline void SoftSyncArrive(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
#if OMIT_SOFTSYNC
    (void)softSyncGm;
    (void)slot;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    return;
#else
    auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
    if (subBlockID == 0) {
        s[slot] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        while (s[slot] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
#endif
}

/**
 * SoftSyncClear：仅 AIV0 清哨兵，供下一轮 L2 复用同一 slot。
 * @param softSyncGm / slot / subBlockID 同 Arrive
 */
__aicore__ inline void SoftSyncClear(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
#if OMIT_SOFTSYNC
    (void)softSyncGm;
    (void)slot;
    (void)subBlockID;
#else
    if (subBlockID == 0) {
        reinterpret_cast<__gm__ int32_t *>(softSyncGm)[slot] = 0;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
#endif
}

/**
 * AIV0 写极简 magic（证明 L2 跑完）；AIV1 / AIC 不写。
 * UB 填常量后 DataCopy → GM（对齐 32B；64B=2 block）。
 * @param out GM 输出 64B
 */
__aicore__ inline void WriteMagic(GM_ADDR out)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kOutBytes);
    AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
    for (uint32_t i = 0; i < 8; ++i) {
        ub.SetValue(i, static_cast<uint8_t>(tiling::kMagicPrefix[i]));
    }
    ub.SetValue(8, tiling::kMagicMark);
    for (uint32_t i = 9; i < tiling::kOutBytes; ++i) {
        ub.SetValue(i, tiling::kMagicFill);
    }
    outQ.EnQue(ub);
    ub = outQ.DeQue<uint8_t>();
    AscendC::GlobalTensor<uint8_t> outGm;
    outGm.SetGlobalBuffer((__gm__ uint8_t *)out, tiling::kOutBytes);
    AscendC::DataCopy(outGm, ub, tiling::kOutBytes);
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX kernel：1 AIC + 2 AIV；phase=0 L1 stub；phase=1 SoftSync→SET(4)。
 * @param out        [out] L2 写 magic
 * @param src        [in]  占位（未用）
 * @param ws         [in/out] 占位 workspace
 * @param softSyncGm [in/out] int32[2] SoftSync 哨兵；Host 每轮 L2 前须清零
 * @param tiling     phase 选 L1/L2
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR softSyncGm,
                                                  TilingData tiling)
{
    (void)src;
    (void)ws;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;

    if (phase == tiling::kPhaseLaunch1) {
        // L1 stub：几乎空，仅 barrier 后返回（无 Wait(4)、无 SoftSync、无 Cube）
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }

    // ========== L2：双 AIV SoftSyncArrive → SET(4)；AIC Wait(4) ==========
    if (aic) {
        TraceDigit(400); // L2 AIC 入口
        TraceDigit(401); // 将 Wait(4)
        FsmWait(ST_SET4);
        TraceDigit(402); // Wait(4) 后 = SET 配对成功
    } else {
        // 双 AIV：入口 500/510 → SoftSync 后 503/513 → SET 后 502/512
        if (subBlockID == 0) {
            TraceDigit(500);
        } else {
            TraceDigit(510);
        }

        SoftSyncArrive(softSyncGm, /*slot=*/0, subBlockID);

        if (subBlockID == 0) {
            TraceDigit(503); // SoftSyncArrive 完成（AIV0 已写哨兵）
        } else {
            TraceDigit(513); // SoftSyncArrive 完成（AIV1 自旋结束）
        }

        FsmSet(ST_SET4);
        if (subBlockID == 0) {
            TraceDigit(502);
        } else {
            TraceDigit(512);
        }

        SoftSyncClear(softSyncGm, /*slot=*/0, subBlockID);

        if (subBlockID == 0) {
            WriteMagic(out);
        }
    }
}
