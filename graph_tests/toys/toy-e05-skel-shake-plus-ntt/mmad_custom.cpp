/**
 * @file mmad_custom.cpp
 * @brief E05：E04 壳 + L1 真 SHAKE256 + L2 真单 poly NTT + SET(4)。
 *
 * 背景：图谱 D-exp-e05 — 在 E04 2-launch+真 NTT 壳上把 L1 假采样换成真 SHAKE256。
 * 结论：L1=AIV0 跑短向量 SHAKE256("abc"→32B)；L2 仍 AIV Split→AIC Mmad×2→AIV Merge+Barrett，
 *       壳层 CrossCore flag 4（与 NTT 内部 flag 1/2/3 不冲突）。
 * 未采用：抄 Encrypt；改 E01–E04；改 shared 原文件；SoftSync；Tag5T。
 *
 * 语义声明：本 NTT golden ≠ F203 Tag5T；SHAKE 短向量 = hashlib.shake_256。
 * TRACE 号段见 TRACE.md。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "shake_l1_ub.hpp"

/** NTT 内部 CrossCore（与 ntt256 一致；占用 1/2/3）。 */
enum NttState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT = 1,
    NTT_AIC_MMAD = 2,
    NTT_AIV_MERGE = 3,
};

/** 壳层 SET(4)：L2 入口齐步（E03/E04 合同；占用 4，避让 NTT 1/2/3）。 */
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
    // ---- 壳层 SET(4)：等双 AIV 完成 Merge 后置位 ----
    TraceDigit(401); // 将 Wait(4)
    FsmWait(ST_SET4);
    TraceDigit(402); // Wait(4) 成功
}

/**
 * L2 AIV：真 NTT Split+Merge；假点积/INTT TRACE；最后 SET(4)。
 * @param subBlockID 0 或 1
 * @param dst NTT 输出 int32[n]
 * @param src 输入 poly int32[n]
 * @param ws  workspace
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

    // ---- Merge + Barrett → dst ----
    {
        AivMerge merge(subBlockID, n, 3329);
        merge.Init(dst + static_cast<size_t>(subBlockID) * (static_cast<size_t>(n) / 2) * sizeof(int32_t),
                   ws + A0, ws + A1, ws + A2, ws + A3);
        merge.CopyIn();
        merge.Compute();
        merge.CopyOut();
    }

    // 假点积 / 假 INTT：仅 TRACE，保持 E03 阶段可读性
    if (subBlockID == 0) {
        TraceDigit(530);
        TraceDigit(540);
        FsmSet(ST_SET4);
        TraceDigit(502);
    } else {
        TraceDigit(531);
        TraceDigit(541);
        FsmSet(ST_SET4);
        TraceDigit(512);
    }
}

/**
 * MIX kernel：phase=0 L1 真 SHAKE；phase=1 L2 真 NTT + SET(4)。
 * @param out [in/out] L1 写 SHAKE y 前缀 32B；L2 写 NTT 结果 int32[256]
 * @param src [in]  输入 poly
 * @param ws  [in/out] ntt256 workspace（Host 预填 M4）
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

    // ========== L2：真单 poly NTT + 壳层 Wait/SET(4) ==========
    if (aic) {
        L2RealNttAic(ws, n);
    } else {
        L2RealNttAiv(subBlockID, out, src, ws, n);
    }
}
