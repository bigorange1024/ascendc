/**
 * @file mmad_custom.cpp
 * @brief Encrypt 任务链骨架 toy：单 MIX launch 串起 stub 链 + CrossCore 握手。
 *
 * 形态（不对 ML-KEM 正确性）——默认路径（SKEL_SKIPNTT=0）：
 *   1. stub_hash/prep（Duplicate 常量）
 *   2. NTT-like：AIV SET(1) → AIC WAIT(1) → MMAD → SET(3) → AIV WAIT(3)
 *   3. stub_inner（Adds/Muls）
 *   4. [SKEL_GATE=1] GATE：双 AIV SET(4) → AIC WAIT(4)→SET(8) → 双 AIV WAIT(8)
 *   5. INTT-like：同构 flag 1/3 + MMAD
 *   6. stub_encode：写 magic 到 out
 *
 * skipNtt 路径（SKEL_SKIPNTT=1，对齐 l18 skipNtt 入口 Wait(4)）：
 *   AIC：**入口即** WAIT(4)，再跑若干 Cube 1/3（简化，不再做中段 GATE）
 *   AIV：stub_hash → [μ 前缀] → SET(4)（除非 OMIT）→ Cube 1/3 → magic
 *   μ 前缀（TASK-005）：
 *     SKEL_HOST_MU=0 → 设备 AIV0 StubPrefixEmbedMu（GM↔UB 形态）
 *     SKEL_HOST_MU=1 → Host 已写折 μ 占位；设备跳过 μ-stub，尽快 SET(4)
 *   故障注入：OMIT_SET4=1 时 AIV 不 SET(4) → AIC 死等，预期 SIM 超时 124
 *
 * 编译开关（run.sh → cmake -D）：
 *   SKEL_GATE=0/1（默认 1；仅 SKIPNTT=0 时生效）
 *   SKEL_HEAVY=0/1（默认 0）
 *   SKEL_SKIPNTT=0/1（默认 0；TASK-004）
 *   SKEL_OMIT_SET4=0/1（默认 0；仅与 SKIPNTT=1 联用）
 *   SKEL_HOST_MU=0/1（默认 1；仅与 SKIPNTT=1 联用；TASK-005）
 *
 * 禁止：AIC Wait 期间 SyncAll；自造 SoftSync 双向汇合；真 SHAKE；碎写 GM / 滥 launch。
 * Host：1 个 MIX launch（KERNEL_TYPE_MIX_AIC_1_2）。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

#ifndef SKEL_GATE
#define SKEL_GATE 1
#endif

#ifndef SKEL_HEAVY
#define SKEL_HEAVY 0
#endif

#ifndef SKEL_SKIPNTT
#define SKEL_SKIPNTT 0
#endif

#ifndef SKEL_OMIT_SET4
#define SKEL_OMIT_SET4 0
#endif

#ifndef SKEL_HOST_MU
#define SKEL_HOST_MU 1
#endif

/**
 * CrossCore 事件：
 *   flag 1 / 3 — 与 Encrypt NTT/INTT 同构；
 *   flag 4 / 8 — GATE 或 skipNtt 入口齐步（AIV→AIC / AIC→AIV）。
 * flag 2 保留语义位，本 toy 不发不收。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,   /**< AIV 填完左矩阵，通知 AIC */
    ST_AIC_MMAD = 2,    /**< 保留；与 Encrypt 枚举对齐 */
    ST_AIV_PACK = 3,    /**< AIC MMAD 完成，通知 AIV */
    ST_IP_AIV_DONE = 4, /**< GATE / skipNtt：AIV 齐步 → AIC Wait(4) */
    ST_AT_JP_GATE = 8,  /**< GATE：AIC 放行 → AIV 开下一段（INTT） */
};

/** 等待对端 CrossCore；PIPE_MTE2 与既有探针一致。禁止在 Wait 期间 SyncAll。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore，通知对端。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * AIC：一轮「WAIT(1) → 轻量/加压 MMAD → SET(3)」Cube 握手。
 * 维度由 tiling::kRows/kDim/kCols 决定（HEAVY→16×64×64，否则 16×32×32）。
 */
__aicore__ inline void AicOneCubeRound(GM_ADDR ws)
{
    FsmState st = ST_AIV_SPLIT;
    FsmWait(st);
    {
        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();
    }
    st = ST_AIV_PACK;
    FsmSet(st);
}

/**
 * AIV：一轮「填 A → SET(1) → WAIT(3)」；可选轻量 pack 读 MAT_C（仅首轮做）。
 * @param tag 写入左矩阵的轮次偏移（区分多轮）
 * @param doLightPack 为 true 时 AIV0 读 MAT_C 首段到 STUB（非真 RouteA）
 */
__aicore__ inline void AivOneCubeRound(int32_t subBlockID, GM_ADDR ws, int8_t tag, bool doLightPack)
{
    FillLeftMatrixA(subBlockID, ws, tag);
    FsmState st = ST_AIV_SPLIT;
    FsmSet(st);
    st = ST_AIV_PACK;
    FsmWait(st);
    if (doLightPack && subBlockID == 0) {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
        // 32B 对齐：至少搬 8 个 int32
        pipe.InitBuffer(inQ, 1, 8 * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> ub = inQ.AllocTensor<int32_t>();
        AscendC::GlobalTensor<int32_t> cGm;
        cGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAT_C), tiling::kRows * tiling::kCols);
        AscendC::DataCopy(ub, cGm, 8);
        inQ.EnQue(ub);
        ub = inQ.DeQue<int32_t>();
        AscendC::GlobalTensor<int32_t> stubGm;
        stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);
        AscendC::DataCopy(stubGm, ub, 8);
        inQ.FreeTensor(ub);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX kernel 入口：1 AIC + 2 AIV，单趟完成 Encrypt 形态骨架。
 * @param out [out] magic 输出，长度 64B
 * @param src [in]  占位输入（stub 不依赖内容）
 * @param ws  [in/out] workspace：S0 / LUT / MAT_C / STUB
 * @param tiling Host 参数（本核未用字段，保留壳对齐）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    (void)src;
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

#if SKEL_SKIPNTT
    // ========== skipNtt 拓扑（TASK-004/005）：AIC 入口即 Wait(4) ==========
    // 背景：图谱 J-empty-trace-aic-wait4 / J-real-aiv0-before-mu-mark —
    //   l18 skipNtt 下 AIC 立刻 Wait(4)；AIV0 先 PrefixEmbedMu 再 SET(4)。
    // 结论：本路径复现 Wait(4)↔SET(4)；HOST_MU 对照「Host 折 μ vs 设备 μ-stub」。
    // 未采用：中段 GATE 4↔8；真 μ 编解码；改 stable Encaps。
    if (aic) {
        // 入口即死等 flag 4（对齐 Encrypt skipNtt）；无 SyncAll
        st = ST_IP_AIV_DONE;
        FsmWait(st);

        // 齐步后跑简化 Cube（保留 1 轮 1/3 即可；HEAVY 仍可加压）
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            AicOneCubeRound(ws);
        }
    } else {
        // AIV：stub_hash → [μ 前缀] →（可选）SET(4) → Cube → magic
        StubHashPrep(subBlockID, ws);

#if SKEL_HOST_MU
        // Host 已在 launch 前写折 μ 占位；设备跳过 PrefixEmbed 形态，仅极短 barrier
        AscendC::PipeBarrier<PIPE_ALL>();
#else
        // 设备 μ-stub：模仿 PrefixEmbedMu 的 GM↔UB 往返（AIV0）；再双 AIV 齐步
        StubPrefixEmbedMu(subBlockID, ws);
#endif

#if !SKEL_OMIT_SET4
        // 正常路径：双 AIV 均 SET(4)，与 Encrypt 双 AIV 齐步同构
        st = ST_IP_AIV_DONE;
        FsmSet(st);
#else
        // 故障注入：故意不 SET(4) → AIC 入口 Wait(4) 挂死（预期 SIM timeout 124）
        AscendC::PipeBarrier<PIPE_ALL>();
#endif

        // 后续 Cube + magic（OMIT 时 AIC 已挂，本侧可能跑完或一并卡住，由预算截断）
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            AivOneCubeRound(subBlockID, ws, /*tag=*/static_cast<int8_t>(r), /*doLightPack=*/(r == 0));
        }
        StubEncodeMagic(subBlockID, out,
                        /*markB8=*/(SKEL_HOST_MU != 0) ? tiling::kMagicHostMuMark
                                                       : tiling::kMagicSkipNttMark);
    }
#else
    // ========== 默认路径：NTT → [GATE] → INTT ==========
    if (aic) {
        // ---- NTT-like：kCubeRoundsPerPhase 轮 Cube（HEAVY=2，基线=1）----
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            AicOneCubeRound(ws);
        }

#if SKEL_GATE
        // ---- GATE（对齐 Encrypt l18 at_jp）：WAIT(4) → SET(8)；无 SoftSync / SyncAll ----
        st = ST_IP_AIV_DONE;
        FsmWait(st);
        st = ST_AT_JP_GATE;
        FsmSet(st);
#endif

        // ---- INTT-like：再 kCubeRoundsPerPhase 轮（合计 2 或 4）----
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            AicOneCubeRound(ws);
        }
    } else {
        // ---- 1. stub_hash/prep ----
        StubHashPrep(subBlockID, ws);

        // ---- 2. NTT-like 多轮 ----
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            // 仅第一轮做 light pack，避免反复覆盖 STUB 干扰 stub_inner 可读性
            AivOneCubeRound(subBlockID, ws, /*tag=*/static_cast<int8_t>(r), /*doLightPack=*/(r == 0));
        }

        // ---- 3. stub_inner ----
        StubInner(subBlockID, ws);

#if SKEL_GATE
        // ---- 4. GATE：双 AIV 均 SET(4)，再 WAIT(8) ----
        st = ST_IP_AIV_DONE;
        FsmSet(st);
        st = ST_AT_JP_GATE;
        FsmWait(st);
        AscendC::PipeBarrier<PIPE_ALL>();
#endif

        // ---- 5. INTT-like 多轮 ----
        for (int r = 0; r < kCubeRoundsPerPhase; ++r) {
            AivOneCubeRound(subBlockID, ws, /*tag=*/static_cast<int8_t>(10 + r), /*doLightPack=*/false);
        }

        // ---- 6. stub_encode magic ----
        StubEncodeMagic(subBlockID, out,
                        /*markB8=*/(SKEL_GATE != 0) ? tiling::kMagicGateMark : tiling::kMagicFill);
    }
#endif // SKEL_SKIPNTT
}
