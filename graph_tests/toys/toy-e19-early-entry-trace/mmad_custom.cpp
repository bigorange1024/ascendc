/**
 * @file mmad_custom.cpp
 * @brief E19 stub：MIX 入口立刻打标，再极简握手；Host 可见 fused-trace 槽 0+15。
 *
 * 目标（EARLY_EMPTY_TRACE E19）：证明「入口标」可被 Host D2H 看到。
 *
 * ## 入口槽与 SIM 约束
 * - AIV0：任何 CrossCore Wait **之前** UB→DataCopy 写 `trace[15]=1`（Host 可见；E19b
 *   加固：NPU 多轮后标量 D2H 曾丢槽 15）。
 * - AIC：任何 CrossCore Wait **之前** 写 `trace[0]=1`（默认 DataCopy；NPU/真路径；**SIM 上
 *   AIC→GM 对 Host D2H 不可见**），并 `Set(1)` 宣告入口。
 * - AIV0：`Wait(1)` 后用 AIV 标量再写 `trace[0]=1`（SIM Host 桥：证明 AIC 已过入口 Set）。
 *
 * 然后 SET4：双 AIV Set(4) ↔ AIC Wait(4)；AIV0 写 magic。
 *
 * 背景：Encaps 挂时 0/16；本 stub 把入口标放到 Wait/业务前，供日后 NPU 区分 H-E3/H-E1。
 * 结论：无 Cube/NTT/PrefixEmbed/Encrypt。
 * 未采用：完整 E17；依赖 SIM 直读 AIC GM（已证不可行）。
 *
 * CrossCore：modeId=2、PIPE_MTE2；flag ∈ {1,4}；Wait 路径禁止 SyncAll。
 */
#include "kernel_operator.h"
#include "tiling.h"

/** CrossCore flag。 */
enum FsmState : uint16_t {
    ST_AIC_ENTER = 1, /**< AIC→AIV0：AIC 已过入口（写槽尝试 + Set） */
    ST_SET4 = 4,      /**< 双 AIV→AIC：握手齐 */
};

/**
 * 写 fused-trace 单槽为 1。
 *
 * 默认（E19b）：LocalTensor 填 1 → DataCopy 到 GM 指定槽，保证 UB→MTE→GM 可见性。
 * 背景：NPU-E19 第 4 轮起 Host D2H 丢失 AIV0 标量写的槽 15（设备仍报 504），槽 0 仍见。
 * 结论：改用 DataCopy 验证「观测管道/写回路径」假说；Encaps FusedTraceMark 同形标量须警惕。
 *
 * 调试：编译加 `-DTOY_E19_TRACE_SCALAR=1` 可回退 Encaps 同形标量直写 GM（对照用）。
 *
 * @param traceGm fused-trace GM 基址 int32[16]
 * @param slot    槽下标（0=AIC 入口，15=AIV0 入口）
 */
__aicore__ inline void TraceSlotStore(GM_ADDR traceGm, int32_t slot)
{
    if (traceGm == nullptr) {
        return;
    }
#if defined(TOY_E19_TRACE_SCALAR) && TOY_E19_TRACE_SCALAR
    // 对照路径：Encaps FusedTraceMark 同形标量 GM 写（NPU 多轮后槽 15 D2H 曾不稳定）
    auto *trace = reinterpret_cast<__gm__ int32_t *>(traceGm);
    trace[slot] = 1;
#else
    // AIC 核无 VECOUT 队列；SIM 亦不依赖 AIC→Host D2H（靠 AIV0 桥写槽 0）
    if (AscendC::GetSubBlockNum() == 1) {
        auto *trace = reinterpret_cast<__gm__ int32_t *>(traceGm);
        trace[slot] = 1;
        return;
    }
    // AIV：UB→DataCopy 整表 RMW（64B=16×int32，32B 对齐），单槽置 1 后写回 GM。
    // 背景：单元素/半表块写 SIM 可过，NPU 槽 15（偏移 60B）仍 D2H 不见；整表搬运对齐 WriteMagic。
    constexpr int32_t kTraceElems = static_cast<int32_t>(tiling::kTraceStages);
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(inQ, 1, static_cast<uint32_t>(kTraceElems) * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, static_cast<uint32_t>(kTraceElems) * sizeof(int32_t));
    AscendC::GlobalTensor<int32_t> traceGt;
    traceGt.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm));
    AscendC::LocalTensor<int32_t> ubIn = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(ubIn, traceGt, kTraceElems);
    inQ.EnQue(ubIn);
    ubIn = inQ.DeQue<int32_t>();
    AscendC::LocalTensor<int32_t> ubOut = outQ.AllocTensor<int32_t>();
    for (int32_t i = 0; i < kTraceElems; ++i) {
        const int32_t v = ubIn.GetValue(static_cast<uint32_t>(i));
        ubOut.SetValue(static_cast<uint32_t>(i), (i == slot) ? 1 : v);
    }
    outQ.EnQue(ubOut);
    inQ.FreeTensor(ubIn);
    ubOut = outQ.DeQue<int32_t>();
    AscendC::DataCopy(traceGt, ubOut, kTraceElems);
    outQ.FreeTensor(ubOut);
    AscendC::PipeBarrier<PIPE_ALL>();
#endif
}

/** 设备侧三位数字 TRACE（知识库 §6；辅证）。 */
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
 * AIV0 写极简 magic（证明 SET4 握手跑完）。
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
 * MIX kernel：1 AIC + 2 AIV；入口标 → flag1 桥 → SET4 → magic。
 * @param out   [out] AIV0 写 magic
 * @param src   [in]  占位
 * @param ws    [in/out] 占位
 * @param trace [in/out] fused-trace int32[16]
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR trace,
                                                  TilingData tiling)
{
    (void)src;
    (void)ws;
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (aic) {
        // ========== AIC 入口：任何 Wait 之前写槽 0 + Set(1) ==========
        TraceDigit(400); // AIC 入口
        TraceSlotStore(trace, tiling::kTraceSlotAicEntry); // DataCopy 写槽 0；SIM Host 靠 AIV 桥
        TraceDigit(404); // 已尝试写槽 0
        FsmSet(ST_AIC_ENTER); // 宣告入口，供 AIV0 桥写
        TraceDigit(405); // 已 Set(1)

        // ========== SET4：Wait(4) ==========
        TraceDigit(401);
        FsmWait(ST_SET4);
        TraceDigit(402); // AIC 完成
    } else if (subBlockID == 0) {
        // ========== AIV0 入口：任何 Wait 之前写槽 15 ==========
        TraceDigit(500);
        TraceSlotStore(trace, tiling::kTraceSlotAiv0Entry);
        TraceDigit(504); // 已 DataCopy 写槽 15

        // ========== 等 AIC 入口 Set(1)，再 AIV 桥写槽 0（SIM Host 可见）==========
        FsmWait(ST_AIC_ENTER);
        TraceSlotStore(trace, tiling::kTraceSlotAicEntry);
        TraceDigit(505); // 已桥写槽 0（证明 AIC 已 Set(1)）

        // ========== SET4 ==========
        FsmSet(ST_SET4);
        TraceDigit(502);
        WriteMagic(out);
    } else {
        // AIV1：不写 fused-trace；只参与 SET4
        TraceDigit(510);
        FsmSet(ST_SET4);
        TraceDigit(512);
    }
}
