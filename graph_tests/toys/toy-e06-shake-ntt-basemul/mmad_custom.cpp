/**
 * @file mmad_custom.cpp
 * @brief E06：E05 壳 + L1 真 SHAKE256 + L2 真单 poly NTT + 真 basemul + SET(4)。
 *
 * 背景：图谱 D-exp-e06 — 在 E05 壳上于 NTT 之后接入真 MultiplyNTTs/basemul。
 * 结论：L1=AIV0 SHAKE256("abc"→32B)；L2=AIV Split→AIC Mmad×2→AIV Merge+Barrett，
 *       再双 AIV 半区标量 Alg.11/12；壳层 CrossCore flag 4（避让 NTT 1/2/3）。
 * 未采用：抄 Encrypt；改 E01–E05；改原 multiplyntts 探针；向量 Gather 路径。
 *
 * 语义：NTT ≠ Tag5T；basemul γ=kMlkemGammas；out=ĥ。
 * TRACE 号段见 TRACE.md。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "shake_l1_ub.hpp"
#include "basemul_half_ub.hpp"

/** NTT 内部 CrossCore（与 ntt256 一致；占用 1/2/3）。 */
enum NttState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT = 1,
    NTT_AIC_MMAD = 2,
    NTT_AIV_MERGE = 3,
};

/** 壳层 SET(4)：L2 入口齐步（E03/E04/E05 合同；占用 4）。 */
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
 * L1 真 SHAKE256：仅 AIV0 跑短向量；AIC/AIV1 仅 barrier。
 * @param shakeYGm [out] 写 32B SHAKE 输出（复用 out 缓冲前缀；Host 在 L1 Sync 后 D2H）
 *
 * TRACE：200 入 → 210 真 SHAKE 开始 → 211 完成 → 212 UB 对拍 PASS / 213 FAIL → 203 返回。
 */
__aicore__ inline void L1RealShake(bool aic, int32_t subBlockID, GM_ADDR shakeYGm)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    if (!aic && subBlockID == 0) {
        TraceDigit(200); // 进入 L1
        TraceDigit(210); // 真 SHAKE256 开始（非 stub）
        const uint32_t pass = ShakeL1Toy::RunShake256AbcUb(reinterpret_cast<__gm__ uint8_t *>(shakeYGm));
        TraceDigit(211); // SHAKE 完成
        if (pass != 0U) {
            TraceDigit(212); // UB golden PASS
        } else {
            TraceDigit(213); // UB golden FAIL
        }
        TraceDigit(203); // L1 将返回
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L2 AIC：真 NTT 的 Cube 段（Wait Split → Mmad×2 → Set Mmad），再 Wait(4)。
 * @param ws workspace（含 M 肢与 A 缓冲）
 * @param n  poly 长度（256）
 */
__aicore__ inline void L2RealNttAic(GM_ADDR ws, int32_t n)
{
    using namespace tiling;
    TraceDigit(400); // L2 AIC 入口
    // ---- 真 NTT：AIC 段（Init 先于 Wait，与 ntt256 一致）----
    AicMmad mmad(2, static_cast<uint16_t>(n), static_cast<uint16_t>(n));
    mmad.Init();
    NttWait(NTT_AIV_SPLIT);
    mmad.Process(ws + A0, ws + S0, ws + M0);
    mmad.Process(ws + A1, ws + S0, ws + M1);
    NttSet(NTT_AIC_MMAD);
    // ---- 壳层 SET(4)：等双 AIV 完成 Merge+basemul 后置位 ----
    TraceDigit(401); // 将 Wait(4)
    FsmWait(ST_SET4);
    TraceDigit(402); // Wait(4) 成功
}

/**
 * L2 AIV：真 NTT Split+Merge；真 basemul 半区；假 INTT TRACE；最后 SET(4)。
 * @param subBlockID 0 或 1
 * @param dst [in/out] 先写 NTT 半区，再原地写 basemul 半区
 * @param src 输入 poly
 * @param ws  workspace（含 ĝ @ G0）
 * @param n   256
 */
__aicore__ inline void L2RealNttAiv(int32_t subBlockID, GM_ADDR dst, GM_ADDR src, GM_ADDR ws, int32_t n)
{
    using namespace tiling;
    if (subBlockID == 0) {
        TraceDigit(500); // AIV0 入
        TraceDigit(520); // 真 NTT 开始（非 stub）
    } else {
        TraceDigit(510);
        TraceDigit(521);
    }

    // ---- Split：半 poly → 4 路 int8 肢 ----
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

    // ---- Merge + Barrett → dst 半区（仍为 NTT 结果）----
    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(dst + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
                   ws + A0, ws + A1, ws + A2, ws + A3);
        merge.CopyIn();
        merge.Compute();
        merge.CopyOut();
    }

    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 真 basemul：本半区 MultiplyNTTs(f̂=NTT, ĝ) 原地写回 dst ----
    {
        const int32_t pairStart = subBlockID * (n / 4); // 0 或 64
        const int32_t pairEnd = pairStart + (n / 4);     // 64 或 128
        const size_t halfOff = static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t);
        if (subBlockID == 0) {
            TraceDigit(530); // 真 basemul 开始（非 stub）
        } else {
            TraceDigit(531);
        }
        toy_e06_basemul::MultiplyNttsHalfGm(dst + halfOff, dst + halfOff, ws + G0 + halfOff, pairStart, pairEnd,
                                            static_cast<uint32_t>(n / 2));
        if (subBlockID == 0) {
            TraceDigit(532); // 真 basemul 完成
        } else {
            TraceDigit(533);
        }
    }

    // 假 INTT：仅 TRACE，保持阶段可读性（本 TASK 不接真 INTT）
    if (subBlockID == 0) {
        TraceDigit(540);
        FsmSet(ST_SET4);
        TraceDigit(502);
    } else {
        TraceDigit(541);
        FsmSet(ST_SET4);
        TraceDigit(512);
    }
}

/**
 * MIX kernel：phase=0 L1 真 SHAKE；phase=1 L2 真 NTT + 真 basemul + SET(4)。
 * @param out [in/out] L1 写 SHAKE y 前缀 32B；L2 写 basemul ĥ int32[256]
 * @param src [in]  输入 poly
 * @param ws  [in/out] ntt256 workspace + G0=ĝ
 * @param tiling phase + tileLength
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;
    const int32_t n = tiling.tileLength;

    if (phase == tiling::kPhaseLaunch1) {
        L1RealShake(aic, subBlockID, out);
        return;
    }

    // ========== L2：真单 poly NTT + 真 basemul + 壳层 Wait/SET(4) ==========
    if (aic) {
        L2RealNttAic(ws, n);
    } else {
        L2RealNttAiv(subBlockID, out, src, ws, n);
    }
}
