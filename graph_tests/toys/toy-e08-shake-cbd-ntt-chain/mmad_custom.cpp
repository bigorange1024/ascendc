/**
 * @file mmad_custom.cpp
 * @brief E08：E07 壳 + L1 真 SHAKE256 + 真 CBD(η=2) + L2 真 NTT+basemul+INTT + SET(4)。
 *
 * 背景：图谱 D-exp-e08 — 在 E07 壳上于 SHAKE 后接入真 CBD，再进 NTT→basemul→INTT。
 * 结论：L1=AIV0 SHAKE→CBD(prf→src)；L2=NTT(src)→MultiplyNTTs→INTT(Minv)+SET(4)。
 * 未采用：抄 Encrypt / Tag5T；改 E01–E07；复测 retracted；假 CBD TRACE stub。
 *
 * 语义：CBD=Alg.8 η=2（vendor 自包含）；NTT/INTT≠Tag5T；basemul γ=kMlkemGammas。
 * TRACE 号段见 TRACE.md。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "shake_l1_ub.hpp"
#include "cbd_l1_ub.hpp"
#include "basemul_half_ub.hpp"

/** NTT 内部 CrossCore（与 ntt256 一致；占用 1/2）。 */
enum NttState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT = 1,
    NTT_AIC_MMAD = 2,
};

/** INTT 内部 CrossCore（避让 NTT 1/2 与壳层 4；占用 5/6）。 */
enum InttState : uint16_t {
    INTT_AIV_SPLIT = 5,
    INTT_AIC_MMAD = 6,
};

/** 壳层 SET(4)：L2 入口齐步（E03–E07 合同；占用 4）。 */
enum FsmState : uint16_t {
    ST_SET4 = 4,
};

/** 设备侧 TRACE：只打三位十进制数字。 */
__aicore__ inline void TraceDigit(int code)
{
    AscendC::printf("%d\n", code);
}

__aicore__ inline void NttWait(NttState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
}

__aicore__ inline void NttSet(NttState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
}

__aicore__ inline void InttWait(InttState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
}

__aicore__ inline void InttSet(InttState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
}

__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L1：真 SHAKE256 → 真 CBD(η=2)；仅 AIV0 跑；AIC/AIV1 仅 barrier。
 * @param shakeYGm [out] 写 32B SHAKE 输出（out 前缀；Host L1 Sync 后 D2H）
 * @param prfGm    [in]  CBD PRF 128B（ws+P0）
 * @param srcGm    [out] CBD 采样 int32[256]（覆盖 src，供 L2 NTT）
 *
 * TRACE：200→210→211→212→220→221→203。
 */
__aicore__ inline void L1RealShakeThenCbd(bool aic, int32_t subBlockID, GM_ADDR shakeYGm, GM_ADDR prfGm,
                                          GM_ADDR srcGm)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    if (!aic && subBlockID == 0) {
        TraceDigit(200); // 进入 L1
        TraceDigit(210); // 真 SHAKE256 开始
        const uint32_t pass = ShakeL1Toy::RunShake256AbcUb(reinterpret_cast<__gm__ uint8_t *>(shakeYGm));
        TraceDigit(211); // SHAKE 完成
        if (pass != 0U) {
            TraceDigit(212); // UB golden PASS
        } else {
            TraceDigit(213); // UB golden FAIL
        }

        // ---- 真 CBD(η=2)：接在 SHAKE 后、返回前；写 src 供 L2 ----
        TraceDigit(220); // 真 CBD 开始（非 stub）
        CbdL1Toy::RunCbdEta2OnePoly(reinterpret_cast<__gm__ const uint8_t *>(prfGm),
                                    reinterpret_cast<__gm__ int32_t *>(srcGm));
        TraceDigit(221); // 真 CBD 完成

        TraceDigit(203); // L1 将返回
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L2 AIC：真 NTT Cube 段 → 真 INTT Cube 段（Minv）→ Wait(4)。
 */
__aicore__ inline void L2RealNttBasemulInttAic(GM_ADDR ws, int32_t n)
{
    using namespace tiling;
    TraceDigit(400); // L2 AIC 入口
    AicMmad mmad(2, static_cast<uint16_t>(n), static_cast<uint16_t>(n));
    mmad.Init();
    NttWait(NTT_AIV_SPLIT);
    mmad.Process(ws + A0, ws + S0, ws + M0);
    mmad.Process(ws + A1, ws + S0, ws + M1);
    NttSet(NTT_AIC_MMAD);

    InttWait(INTT_AIV_SPLIT);
    mmad.Process(ws + A0, ws + S0, ws + Minv0);
    mmad.Process(ws + A1, ws + S0, ws + Minv1);
    InttSet(INTT_AIC_MMAD);

    TraceDigit(401); // 将 Wait(4)
    FsmWait(ST_SET4);
    TraceDigit(402); // Wait(4) 成功
}

/**
 * L2 AIV：真 NTT Split+Merge；真 basemul；真 INTT；最后 SET(4)。
 * @param src 已由 L1 CBD 覆写的噪声 poly
 */
__aicore__ inline void L2RealNttBasemulInttAiv(int32_t subBlockID, GM_ADDR dst, GM_ADDR src, GM_ADDR ws, int32_t n)
{
    using namespace tiling;
    if (subBlockID == 0) {
        TraceDigit(500);
        TraceDigit(520);
    } else {
        TraceDigit(510);
        TraceDigit(521);
    }

    {
        AivSplit split(subBlockID, n / 2);
        size_t src_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        size_t dst_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int8_t);
        split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, ws + S2 + dst_offset, ws + S3 + dst_offset,
                   src + src_offset);
        split.CopyIn();
        split.Compute();
        split.CopyOut();
        NttSet(NTT_AIV_SPLIT);
    }

    NttWait(NTT_AIC_MMAD);

    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(dst + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
                   ws + A0, ws + A1, ws + A2, ws + A3);
        merge.CopyIn();
        merge.Compute();
        merge.CopyOut();
    }

    AscendC::PipeBarrier<PIPE_ALL>();

    {
        const int32_t pairStart = subBlockID * (n / 4);
        const int32_t pairEnd = pairStart + (n / 4);
        const size_t halfOff = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        if (subBlockID == 0) {
            TraceDigit(530);
        } else {
            TraceDigit(531);
        }
        toy_e06_basemul::MultiplyNttsHalfGm(dst + halfOff, dst + halfOff, ws + G0 + halfOff, pairStart, pairEnd,
                                            static_cast<uint32_t>(n / 2));
        if (subBlockID == 0) {
            TraceDigit(532);
        } else {
            TraceDigit(533);
        }
    }

    AscendC::PipeBarrier<PIPE_ALL>();

    if (subBlockID == 0) {
        TraceDigit(540);
    } else {
        TraceDigit(541);
    }
    {
        AivSplit split(subBlockID, n / 2);
        size_t src_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        size_t dst_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int8_t);
        split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, ws + S2 + dst_offset, ws + S3 + dst_offset,
                   dst + src_offset);
        split.CopyIn();
        split.Compute();
        split.CopyOut();
        InttSet(INTT_AIV_SPLIT);
    }

    InttWait(INTT_AIC_MMAD);

    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(dst + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
                   ws + A0, ws + A1, ws + A2, ws + A3);
        merge.CopyIn();
        merge.Compute();
        merge.CopyOut();
    }

    {
        const size_t halfOff = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        const uint32_t halfLen = static_cast<uint32_t>(n / 2);
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
        AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
        AscendC::GlobalTensor<int32_t> gm;
        gm.SetGlobalBuffer((__gm__ int32_t *)(dst + halfOff));
        pipe.InitBuffer(inQ, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(outQ, 1, halfLen * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> loc = inQ.AllocTensor<int32_t>();
        AscendC::DataCopy(loc, gm, halfLen);
        inQ.EnQue(loc);
        loc = inQ.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> out = outQ.AllocTensor<int32_t>();
        for (uint32_t i = 0; i < halfLen; ++i) {
            out.SetValue(i, toy_e06_basemul::BarrettRed(loc.GetValue(i)));
        }
        outQ.EnQue(out);
        inQ.FreeTensor(loc);
        out = outQ.DeQue<int32_t>();
        AscendC::DataCopy(gm, out, halfLen);
        outQ.FreeTensor(out);
    }

    if (subBlockID == 0) {
        TraceDigit(542);
        FsmSet(ST_SET4);
        TraceDigit(502);
    } else {
        TraceDigit(543);
        FsmSet(ST_SET4);
        TraceDigit(512);
    }
}

/**
 * MIX kernel：phase=0 L1 真 SHAKE+CBD；phase=1 L2 真 NTT+basemul+INTT+SET(4)。
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;
    const int32_t n = tiling.tileLength;

    if (phase == tiling::kPhaseLaunch1) {
        L1RealShakeThenCbd(aic, subBlockID, out, ws + tiling::P0, src);
        return;
    }

    if (aic) {
        L2RealNttBasemulInttAic(ws, n);
    } else {
        L2RealNttBasemulInttAiv(subBlockID, out, src, ws, n);
    }
}
