/**
 * @file main.cpp
 * @brief E19 Host：分配 fused-trace[16]，单 launch MIX×TOY_ROUNDS；核后 D2H 打印入口槽。
 *
 * TRACE 协议：Host 三位数字 + stderr `[e19-trace] stages set=…`（仿 MaybeTrace 打印形）。
 * 验收主据：每轮结束后槽 14 与 15 均为 1（stages set≥2）。
 */
#include "data_utils.h"
#include "tiling.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR trace, TilingData tiling);
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

/** Host TRACE：只打三位数字。 */
static void TraceDigit(int code)
{
    std::printf("%d\n", code);
    std::fflush(stdout);
}

/**
 * 核后 D2H 打印非零槽（SIM 简化：不做 Sync 中轮询）。
 * @param label 轮次标签
 * @param host  int32[16] Host 缓冲
 * @return 非零槽个数
 */
static int PrintTraceSlots(int round, const int32_t *host)
{
    int pop = 0;
    for (int i = 0; i < tiling::kTraceStages; ++i) {
        if (host[i] != 0) {
            ++pop;
        }
    }
    std::fprintf(stderr, "[e19-trace] round=%d stages set=%d/%d :", round, pop, tiling::kTraceStages);
    for (int i = 0; i < tiling::kTraceStages; ++i) {
        if (host[i] != 0) {
            std::fprintf(stderr, " %d", i);
        }
    }
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    return pop;
}

/**
 * 主流程：同进程 for N：清零 trace → 100 → launch → Sync → D2H 打印 → 111。
 * @return 0 成功；非 0 失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int rounds = 8;
    if (const char *envR = std::getenv("TOY_ROUNDS")) {
        int v = std::atoi(envR);
        if (v > 0 && v <= 64) {
            rounds = v;
        }
    }

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kSrcBytes;
    size_t outFileSize = tiling::kOutBytes;
    const size_t traceBytes = tiling::kTraceBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *td = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *trace = (uint8_t *)AscendC::GmAlloc(traceBytes > 1024 ? traceBytes : 1024);
    int32_t *traceHostView = reinterpret_cast<int32_t *>(trace);

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }

    for (int r = 0; r < rounds; ++r) {
        std::memset(trace, 0, traceBytes);
        TraceDigit(100);
        ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, trace, *td);
        (void)PrintTraceSlots(r, traceHostView);
        // 入口槽必须已置位
        if (traceHostView[tiling::kTraceSlotAicEntry] != 1 ||
            traceHostView[tiling::kTraceSlotAiv0Entry] != 1) {
            std::fprintf(stderr, "[FAIL] entry slots missing after round %d\n", r);
            return 21;
        }
        TraceDigit(111);
    }

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)src);
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

    uint8_t *outHost, *srcHost, *wsHost;
    uint8_t *outDevice, *srcDevice, *wsDevice;
    int32_t *traceHost = nullptr;
    void *traceDevice = nullptr;

    TilingData *tilingHost;
    CHECK_ACL(aclrtMallocHost((void **)(&tilingHost), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tilingHost, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    for (size_t i = 0; i < wsFileSize; ++i) {
        wsHost[i] = 0;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // fused-trace[16]：Host 分配；每轮清零后传入 launch
    CHECK_ACL(aclrtMallocHost((void **)(&traceHost), traceBytes));
    CHECK_ACL(aclrtMalloc((void **)&traceDevice, traceBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(traceHost, 0, traceBytes);
    CHECK_ACL(aclrtMemcpy(traceDevice, traceBytes, traceHost, traceBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    for (int r = 0; r < rounds; ++r) {
        // 清零设备槽，避免轮间残留
        std::memset(traceHost, 0, traceBytes);
        CHECK_ACL(aclrtMemcpy(traceDevice, traceBytes, traceHost, traceBytes, ACL_MEMCPY_HOST_TO_DEVICE));

        TraceDigit(100);
        ACLRT_LAUNCH_KERNEL(mmad_custom)
        (blockDim, stream, outDevice, srcDevice, wsDevice, static_cast<uint8_t *>(traceDevice), tilingHost);
        CHECK_ACL(aclrtSynchronizeStream(stream));

        // 核后 D2H：SIM 上偶发延迟；短轮询对齐 MaybeTrace（验收槽 0+15）
        int pop = 0;
        constexpr int kMaxAttempts = 40; // 40×50ms ≈ 2s
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            CHECK_ACL(aclrtMemcpy(traceHost, traceBytes, traceDevice, traceBytes, ACL_MEMCPY_DEVICE_TO_HOST));
            pop = 0;
            for (int i = 0; i < tiling::kTraceStages; ++i) {
                if (traceHost[i] != 0) {
                    ++pop;
                }
            }
            if (traceHost[tiling::kTraceSlotAicEntry] == 1 &&
                traceHost[tiling::kTraceSlotAiv0Entry] == 1) {
                if (attempt > 0) {
                    std::fprintf(stderr, "[e19-trace] delayed visibility attempt=%d\n", attempt);
                    std::fflush(stderr);
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        pop = PrintTraceSlots(r, traceHost);
        if (pop < 2 || traceHost[tiling::kTraceSlotAicEntry] != 1 ||
            traceHost[tiling::kTraceSlotAiv0Entry] != 1) {
            std::fprintf(stderr, "[FAIL] entry slots 0/15 missing after round %d (pop=%d)\n", r, pop);
            // 失败亦做 ACL 收尾，避免 CAModel 进程异常退出（曾见 rc=139）
            CHECK_ACL(aclrtFree(outDevice));
            CHECK_ACL(aclrtFreeHost(outHost));
            CHECK_ACL(aclrtFree(srcDevice));
            CHECK_ACL(aclrtFreeHost(srcHost));
            CHECK_ACL(aclrtFree(wsDevice));
            CHECK_ACL(aclrtFreeHost(wsHost));
            CHECK_ACL(aclrtFree(traceDevice));
            CHECK_ACL(aclrtFreeHost(traceHost));
            CHECK_ACL(aclrtFreeHost(tilingHost));
            CHECK_ACL(aclrtDestroyStream(stream));
            CHECK_ACL(aclrtResetDevice(deviceId));
            CHECK_ACL(aclFinalize());
            return 21;
        }
        TraceDigit(111);
    }

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }

    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(traceDevice));
    CHECK_ACL(aclrtFreeHost(traceHost));
    CHECK_ACL(aclrtFreeHost(tilingHost));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
