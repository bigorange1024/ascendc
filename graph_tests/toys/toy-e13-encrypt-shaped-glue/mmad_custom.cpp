/**
 * @file mmad_custom.cpp
 * @brief E13：E12 积木 + Encrypt 形态粘合 — L1 采样 / L2 代数+压码 → c=c1||c2。
 *
 * 背景：图谱 D-exp-e13 — 在 k=2 真积木上呈现 Encrypt 两段角色与 c 形输出。
 * 结论：L1=SHAKE→CBD(u×2+v×1)；L2=u 路×2（无 μ）+ v 路×1（含 Decompress_1(μ)）+ SET(4)。
 * 未采用：抄 Encrypt/Encaps；改 E01–E12。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "shake_l1_ub.hpp"
#include "cbd_l1_ub.hpp"
#include "basemul_half_ub.hpp"
#include "decompress_l2_ub.hpp"
#include "compress_l2_ub.hpp"
#include "byteencode_l2_ub.hpp"

enum NttState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT = 1,
    NTT_AIC_MMAD = 2,
};

enum InttState : uint16_t {
    INTT_AIV_SPLIT = 5,
    INTT_AIC_MMAD = 6,
};

enum FsmState : uint16_t {
    ST_SET4 = 4,
};

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
 * L1 采样角色：真 SHAKE + CBD(u×2→src) + CBD(v×1→ws[E0])。
 */
__aicore__ inline void L1EncryptSampling(bool aic, int32_t subBlockID, GM_ADDR shakeYGm, GM_ADDR prfGm,
                                         GM_ADDR srcGm, GM_ADDR e2Gm)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    if (!aic && subBlockID == 0) {
        TraceDigit(200);
        TraceDigit(210);
        const uint32_t pass = ShakeL1Toy::RunShake256AbcUb(reinterpret_cast<__gm__ uint8_t *>(shakeYGm));
        TraceDigit(211);
        if (pass != 0U) {
            TraceDigit(212);
        } else {
            TraceDigit(213);
        }

        TraceDigit(220);
        CbdL1Toy::RunCbdEta2UPolys(reinterpret_cast<__gm__ const uint8_t *>(prfGm),
                                   reinterpret_cast<__gm__ int32_t *>(srcGm));
        TraceDigit(222);

        CbdL1Toy::RunCbdEta2VPoly(reinterpret_cast<__gm__ const uint8_t *>(prfGm),
                                  reinterpret_cast<__gm__ int32_t *>(e2Gm));
        TraceDigit(223); // v 路 e2 采样完成

        TraceDigit(221);
        TraceDigit(203);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void L2RealNttBasemulInttAicOnePoly(GM_ADDR ws, int32_t n, uint32_t polyIdx)
{
    using namespace tiling;
    if (polyIdx == 0U) {
        TraceDigit(400);
    } else if (polyIdx == 1U) {
        TraceDigit(410);
    } else {
        TraceDigit(420);
    }
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

    if (polyIdx == 0U) {
        TraceDigit(401);
    } else if (polyIdx == 1U) {
        TraceDigit(411);
    } else {
        TraceDigit(421);
    }
}

/**
 * L2 AIV：单 poly 真链；embedMu=false 为 u 路（跳过 μ）；true 为 v 路。
 */
__aicore__ inline void L2RealChainOnePolyAiv(int32_t subBlockID, GM_ADDR workGm, GM_ADDR srcGm, GM_ADDR ws,
                                             GM_ADDR gGm, GM_ADDR muGm, GM_ADDR encOutGm, int32_t n,
                                             uint32_t polyIdx, bool embedMu)
{
    using namespace tiling;
    const int32_t baseTrace = (polyIdx == 0U) ? 500 : ((polyIdx == 1U) ? 600 : 700);

    if (subBlockID == 0) {
        TraceDigit(baseTrace);
        TraceDigit(baseTrace + 20);
    } else {
        TraceDigit(baseTrace + 10);
        TraceDigit(baseTrace + 21);
    }

    {
        AivSplit split(subBlockID, n / 2);
        size_t src_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        size_t dst_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int8_t);
        split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, ws + S2 + dst_offset, ws + S3 + dst_offset,
                   srcGm + src_offset);
        split.CopyIn();
        split.Compute();
        split.CopyOut();
        NttSet(NTT_AIV_SPLIT);
    }

    NttWait(NTT_AIC_MMAD);

    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(workGm + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
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
            TraceDigit(baseTrace + 30);
        } else {
            TraceDigit(baseTrace + 31);
        }
        toy_e06_basemul::MultiplyNttsHalfGm(workGm + halfOff, workGm + halfOff, gGm + halfOff, pairStart, pairEnd,
                                            static_cast<uint32_t>(n / 2));
        if (subBlockID == 0) {
            TraceDigit(baseTrace + 32);
        } else {
            TraceDigit(baseTrace + 33);
        }
    }

    AscendC::PipeBarrier<PIPE_ALL>();

    if (subBlockID == 0) {
        TraceDigit(baseTrace + 40);
    } else {
        TraceDigit(baseTrace + 41);
    }
    {
        AivSplit split(subBlockID, n / 2);
        size_t src_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        size_t dst_offset = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int8_t);
        split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, ws + S2 + dst_offset, ws + S3 + dst_offset,
                   workGm + src_offset);
        split.CopyIn();
        split.Compute();
        split.CopyOut();
        InttSet(INTT_AIV_SPLIT);
    }

    InttWait(INTT_AIC_MMAD);

    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(workGm + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
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
        gm.SetGlobalBuffer((__gm__ int32_t *)(workGm + halfOff));
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
        TraceDigit(baseTrace + 42);
    } else {
        TraceDigit(baseTrace + 43);
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    if (embedMu) {
        const size_t halfOff = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        const uint32_t halfLen = static_cast<uint32_t>(n / 2);
        const uint32_t coeffOffset = static_cast<uint32_t>(subBlockID) * halfLen;
        if (subBlockID == 0) {
            TraceDigit(baseTrace + 44);
        } else {
            TraceDigit(baseTrace + 45);
        }
        DecompressL2Toy::DecompressMuAddHalfInPlace(workGm + halfOff, muGm, coeffOffset, halfLen);
        if (subBlockID == 0) {
            TraceDigit(baseTrace + 46);
        } else {
            TraceDigit(baseTrace + 47);
        }
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    {
        const size_t halfOff = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        const uint32_t halfLen = static_cast<uint32_t>(n / 2);
        if (subBlockID == 0) {
            TraceDigit(baseTrace + 50);
        } else {
            TraceDigit(baseTrace + 51);
        }
        CompressL2Toy::CompressHalfInPlace(workGm + halfOff, halfLen);
        if (subBlockID == 0) {
            TraceDigit(baseTrace + 52);
        } else {
            TraceDigit(baseTrace + 53);
        }
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    if (subBlockID == 0) {
        TraceDigit(baseTrace + 60);
        ByteEncodeL2Toy::EncodeFullPoly(workGm, encOutGm);
        TraceDigit(baseTrace + 62);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    if (subBlockID == 0) {
        TraceDigit(baseTrace + 3);
    } else {
        TraceDigit(baseTrace + 13);
    }
}

/** L2 AIC：u×2 + v×1 串行 NTT/INTT；末 poly 后 Wait(4)。 */
__aicore__ inline void L2EncryptGlueAic(GM_ADDR ws, int32_t n)
{
    for (uint32_t p = 0; p < tiling::k; ++p) {
        L2RealNttBasemulInttAicOnePoly(ws, n, p);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    L2RealNttBasemulInttAicOnePoly(ws, n, 2U);
    AscendC::PipeBarrier<PIPE_ALL>();
    TraceDigit(402); // AIC 三 poly 完成，即将 Wait(4)
    FsmWait(ST_SET4);
}

/**
 * L2 代数+压码：c1=u0∥u1（无 μ）；c2=v（含 μ）；一次 SET(4)。
 */
__aicore__ inline void L2EncryptGlueAiv(int32_t subBlockID, GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR muGm,
                                        int32_t n)
{
    using namespace tiling;
    const size_t polyBytes = static_cast<size_t>(n) * sizeof(int32_t);

    for (uint32_t p = 0; p < k; ++p) {
        const size_t workGm = W0 + static_cast<size_t>(p) * polyBytes;
        const size_t srcGm = static_cast<size_t>(p) * polyBytes;
        const size_t gGm = G0 + static_cast<size_t>(p) * polyBytes;
        const size_t encGm = static_cast<size_t>(p) * kEncodeBytesPerPoly;
        L2RealChainOnePolyAiv(subBlockID, ws + workGm, src + srcGm, ws, ws + gGm, muGm, out + encGm, n, p, false);
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    {
        const size_t encGm = kC1Bytes;
        L2RealChainOnePolyAiv(subBlockID, ws + W2, ws + E0, ws, ws + G2, muGm, out + encGm, n, 2U, true);
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    if (subBlockID == 0) {
        TraceDigit(502);
    } else {
        TraceDigit(512);
    }
    FsmSet(ST_SET4);
}

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;
    const int32_t n = tiling.tileLength;

    if (phase == tiling::kPhaseLaunch1) {
        L1EncryptSampling(aic, subBlockID, out, ws + tiling::P0, src, ws + tiling::E0);
        return;
    }

    if (aic) {
        L2EncryptGlueAic(ws, n);
    } else {
        L2EncryptGlueAiv(subBlockID, out, src, ws, ws + tiling::MU0, n);
    }
}
