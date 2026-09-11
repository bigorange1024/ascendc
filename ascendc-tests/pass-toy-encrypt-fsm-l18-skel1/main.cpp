/**
 * @file main.cpp
 * @brief Host：装填 LUT=I₃₂、同进程多 launch MIX、每轮打印 TRACE 槽、落盘完成标记。
 *
 * 不对正确性对拍；SIM 路径每轮同步返回且 TRACE 非空即视为该轮核跑完可见。
 *
 * 背景：GT-20260903-7 模仿 F-SIM-LAUNCH（生产 Encrypt 2 Host launch）加压 SIM。
 * 结论：TOY_SPLIT_2LAUNCH=1 时每轮 launch0(NTT_ONLY)→Sync→launch1(GATE_INTT_ONLY)→Sync；
 *       默认未设时仍单 launch 全链路（与 GT-5/6 兼容）。
 * 未采用：同核 fused 当唯一加压；外层反复整树 run.sh 当唯一加压。
 */
#include "data_utils.h"
#include "tiling.h"
#include <cstdio>
#include <cstring>
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace, TilingData tiling);
#endif

/**
 * 打印已置位 TRACE 逻辑槽（每槽 32B 块，读块首 int32；仿 [l18-trace]）。
 * @param iter  当前「组」轮次（0-based）；两段模式下同一 iter 会打两次
 * @param tag   段标签（如 "FULL" / "NTT_ONLY" / "GATE_INTT_ONLY"）
 * @param slots Host 侧 int32 缓冲，长度 kTraceSlots * kTraceAlignInts
 */
static void PrintTraceSlots(int iter, const char *tag, const int32_t *slots)
{
    int pop = 0;
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            ++pop;
        }
    }
    std::printf("[toy-l18-trace] iter=%d phase=%s stages set=%d/%zu :", iter, tag, pop,
                tiling::kTraceSlots);
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            std::printf(" %zu", i);
        }
    }
    std::printf("\n");
    std::fflush(stdout);
}

/**
 * Host 预填 AIC TRACE 用的全 1 模板（ws+TRACE_ONES，8×int32）。
 * Cube 无 UB Duplicate，Mark 时 DataCopy 此模板 → L1 → 槽块。
 */
static void FillTraceOnes(uint8_t *ws)
{
    auto *ones = reinterpret_cast<int32_t *>(ws + tiling::TRACE_ONES);
    for (size_t i = 0; i < tiling::kTraceAlignInts; ++i) {
        ones[i] = 1;
    }
}

/**
 * 解析同进程「组」次数：环境变量 TOY_LAUNCH_REPEAT，默认 8；非法/≤0 回落默认。
 * 两段模式下=「NTT_ONLY+GATE_INTT_ONLY」组数；单 launch 模式=全链路 launch 次数。
 */
static int ParseLaunchRepeat()
{
    int launchRepeat = 8;
#ifndef ASCENDC_CPU_DEBUG
    if (const char *envRep = std::getenv("TOY_LAUNCH_REPEAT")) {
        const int v = std::atoi(envRep);
        if (v > 0) {
            launchRepeat = v;
        }
    }
#endif
    return launchRepeat;
}

/**
 * 是否拆两段 Host launch：TOY_SPLIT_2LAUNCH=1（非 0）启用；未设/0=单 launch 全链路。
 */
static bool ParseSplit2Launch()
{
#ifndef ASCENDC_CPU_DEBUG
    if (const char *env = std::getenv("TOY_SPLIT_2LAUNCH")) {
        return std::atoi(env) != 0;
    }
#endif
    return false;
}

/**
 * 主流程：读 tiling/lut → 同进程多轮（清 TRACE → launch → sync → 打印）→ 写 output/out.bin。
 * @return 0 成功；非 0 读输入/写输出失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t outFileSize = tiling::kOutBytes;
    size_t lutFileSize = tiling::kLutBytes;
    size_t traceFileSize = tiling::kTraceBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    const int launchRepeat = ParseLaunchRepeat();
    const bool split2 = ParseSplit2Launch();
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    /* CPU 孪生仅作编译壳；本任务验收不认 CPU（D-RG-SIM-PRIMARY）。 */
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *trace = (uint8_t *)AscendC::GmAlloc(traceFileSize > 1024 ? traceFileSize : 1024);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }

    std::printf("[host] TOY_LAUNCH_REPEAT=%d TOY_SPLIT_2LAUNCH=%d (CPU 壳；不作验收)\n",
                launchRepeat, split2 ? 1 : 0);
    std::fflush(stdout);
    for (int iter = 0; iter < launchRepeat; ++iter) {
        /* TRACE：每轮开始清一次（两段组也只清一次，便于累计 NTT+GATE/INTT 槽） */
        std::memset(trace, 0, traceFileSize);
        FillTraceOnes(ws);
        if (split2) {
            tiling->phase = PHASE_NTT_ONLY;
            std::printf("[host] iter=%d/%d phase=NTT_ONLY launch0 begin\n", iter, launchRepeat);
            std::fflush(stdout);
            ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
            std::printf("[host] iter=%d/%d phase=NTT_ONLY sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            PrintTraceSlots(iter, "NTT_ONLY", reinterpret_cast<const int32_t *>(trace));

            tiling->phase = PHASE_GATE_INTT_ONLY;
            std::printf("[host] iter=%d/%d phase=GATE_INTT_ONLY launch1 begin\n", iter,
                        launchRepeat);
            std::fflush(stdout);
            ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
            std::printf("[host] iter=%d/%d phase=GATE_INTT_ONLY sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            PrintTraceSlots(iter, "GATE_INTT_ONLY", reinterpret_cast<const int32_t *>(trace));
        } else {
            tiling->phase = PHASE_FULL;
            std::printf("[host] iter=%d/%d phase=FULL begin\n", iter, launchRepeat);
            std::fflush(stdout);
            ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
            std::printf("[host] iter=%d/%d phase=FULL sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            PrintTraceSlots(iter, "FULL", reinterpret_cast<const int32_t *>(trace));
        }
    }

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)trace);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *outHost, *wsHost, *traceHost;
    uint8_t *outDevice, *wsDevice, *traceDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&traceHost), traceFileSize));
    CHECK_ACL(aclrtMalloc((void **)&traceDevice, traceFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }

    /*
     * GT-20260903-7 / Q-TOY-SIM-2LAUNCH-HANG：
     * TRACE 清零策略=「每轮（一组）开始清一次」：两段 launch 共享同一 TRACE 缓冲，
     * launch0 写 NTT 槽、launch1 追加 GATE/INTT 槽；挂死时可看最后一节 begin/sync。
     * 未采用：每段清零（会丢掉 launch0 槽，不利于累计可见性）。
     */
    if (split2) {
        std::printf("[host] TOY_SPLIT_2LAUNCH=1 TOY_LAUNCH_REPEAT=%d "
                    "launch0=NTT_ONLY→Sync→launch1=GATE_INTT_ONLY→Sync "
                    "(GT-20260903-7; TRACE clear once per round)\n",
                    launchRepeat);
    } else {
        std::printf("[host] TOY_SPLIT_2LAUNCH=0 TOY_LAUNCH_REPEAT=%d "
                    "FULL NTT1/3→GATE4/8→INTT1/3 (GT-5/6 compat)\n",
                    launchRepeat);
    }
    std::fflush(stdout);

    for (int iter = 0; iter < launchRepeat; ++iter) {
        /* 每轮开始清零 Host/Device TRACE（两段组只清一次） */
        std::memset(traceHost, 0, traceFileSize);
        CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                              ACL_MEMCPY_HOST_TO_DEVICE));

        FillTraceOnes(wsHost);
        CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

        if (split2) {
            tiling->phase = PHASE_NTT_ONLY;
            std::printf("[host] iter=%d/%d phase=NTT_ONLY launch0 begin\n", iter, launchRepeat);
            std::fflush(stdout);
            ACLRT_LAUNCH_KERNEL(mmad_custom)
            (blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
            CHECK_ACL(aclrtSynchronizeStream(stream));
            std::printf("[host] iter=%d/%d phase=NTT_ONLY sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            PrintTraceSlots(iter, "NTT_ONLY", reinterpret_cast<const int32_t *>(traceHost));

            /* launch1：不重清 TRACE；ws 仍在 device（LUT/TRACE_ONES 未改） */
            tiling->phase = PHASE_GATE_INTT_ONLY;
            std::printf("[host] iter=%d/%d phase=GATE_INTT_ONLY launch1 begin\n", iter,
                        launchRepeat);
            std::fflush(stdout);
            ACLRT_LAUNCH_KERNEL(mmad_custom)
            (blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
            CHECK_ACL(aclrtSynchronizeStream(stream));
            std::printf("[host] iter=%d/%d phase=GATE_INTT_ONLY sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            PrintTraceSlots(iter, "GATE_INTT_ONLY", reinterpret_cast<const int32_t *>(traceHost));
        } else {
            tiling->phase = PHASE_FULL;
            std::printf("[host] iter=%d/%d phase=FULL begin\n", iter, launchRepeat);
            std::fflush(stdout);
            ACLRT_LAUNCH_KERNEL(mmad_custom)
            (blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
            CHECK_ACL(aclrtSynchronizeStream(stream));
            std::printf("[host] iter=%d/%d phase=FULL sync done\n", iter, launchRepeat);
            std::fflush(stdout);
            CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            PrintTraceSlots(iter, "FULL", reinterpret_cast<const int32_t *>(traceHost));
        }
    }

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }

    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(traceDevice));
    CHECK_ACL(aclrtFreeHost(traceHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
