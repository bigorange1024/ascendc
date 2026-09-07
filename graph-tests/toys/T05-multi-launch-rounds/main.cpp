/**
 * @file main.cpp
 * @brief Host：装填 LUT=I₃₂、**串行 launch 2 次** MIX、打印 TRACE 编号、落盘完成标记。
 *
 * 不对正确性对拍；SIM/NPU 路径每轮 SynchronizeStream 返回 + TRACE 可见即视为该轮跑完。
 * Host 编号（KB §6 扩展）：110=第 1 轮 launch 前，120=第 2 轮 launch 前，199=末轮 Sync 返回后。
 * 每轮设备 TRACE 仍用 T03 槽位（201–307）；两轮设备号可重复，Host 110/120 区分轮次。
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

/** 串行 launch 轮数（TASK：建议 2 轮）。 */
static constexpr int kLaunchRounds = 2;

/** Host 打印用：槽下标 → KB 十进制编号（与 trace_map.md 一致）。 */
static const int32_t kTraceCodes[tiling::kTraceSlots] = {
    201, 301, 401, 402, 203, 303, /* NTT 段 */
    403, 204, 304, 404, 205, 305, /* GATE 段 */
    206, 306, 405, 406, 207, 307  /* INTT 段（第二轮 1/3） */
};

/** 每轮 launch 前 Host 轮次号（110/120…）。 */
static const int32_t kHostRoundBeforeLaunch[kLaunchRounds] = {110, 120};

/**
 * 打印已置位 TRACE：每槽 32B 块首 int32≠0 则输出对应 KB 编号（一行一个）。
 * @param slots Host 侧 int32 缓冲，长度 kTraceSlots * kTraceAlignInts
 */
static void PrintTraceSlots(const int32_t *slots)
{
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            std::printf("%d\n", static_cast<int>(kTraceCodes[i]));
        }
    }
    std::fflush(stdout);
}

/** Host 预填 AIC TRACE 用的全 1 模板（ws+TRACE_ONES，8×int32）。 */
static void FillTraceOnes(uint8_t *ws)
{
    auto *ones = reinterpret_cast<int32_t *>(ws + tiling::TRACE_ONES);
    for (size_t i = 0; i < tiling::kTraceAlignInts; ++i) {
        ones[i] = 1;
    }
}

/** 清零 TRACE 缓冲（每轮 launch 前 Host 侧预清零）。 */
static void ClearTraceBuffer(int32_t *traceHost, size_t traceFileSize)
{
    std::memset(traceHost, 0, traceFileSize);
}

#ifdef ASCENDC_CPU_DEBUG
/**
 * CPU 孪生：串行 ICPU_RUN_KF kLaunchRounds 次。
 * @return 0 成功；非 0 与 T03 一致（读盘/写盘失败）
 */
static int32_t RunCpuRounds(uint8_t *out, uint8_t *ws, uint8_t *trace, TilingData tiling,
                            uint32_t blockDim, size_t outFileSize, size_t traceFileSize)
{
    auto *traceSlots = reinterpret_cast<int32_t *>(trace);
    for (int round = 0; round < kLaunchRounds; ++round) {
        ClearTraceBuffer(traceSlots, traceFileSize);
        std::printf("%d\n", kHostRoundBeforeLaunch[round]);
        std::fflush(stdout);
        ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, tiling);
        PrintTraceSlots(traceSlots);
    }
    std::printf("199\n");
    std::fflush(stdout);
    return WriteFile("./output/out.bin", out, outFileSize) ? 0 : 14;
}
#else
/**
 * SIM/NPU：串行 ACLRT_LAUNCH + SynchronizeStream kLaunchRounds 次。
 * 每轮：H2D trace 清零 → launch → Sync → D2H trace 打印。
 */
static int32_t RunDeviceRounds(aclrtStream stream, uint8_t *outDevice, uint8_t *wsDevice,
                               uint8_t *traceDevice, uint8_t *outHost, uint8_t *traceHost,
                               TilingData *tiling, uint32_t blockDim, size_t outFileSize,
                               size_t traceFileSize)
{
    for (int round = 0; round < kLaunchRounds; ++round) {
        ClearTraceBuffer(reinterpret_cast<int32_t *>(traceHost), traceFileSize);
        CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                              ACL_MEMCPY_HOST_TO_DEVICE));

        std::printf("%d\n", kHostRoundBeforeLaunch[round]);
        std::fflush(stdout);
        ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));

        CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        PrintTraceSlots(reinterpret_cast<const int32_t *>(traceHost));
    }
    std::printf("199\n");
    std::fflush(stdout);

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    return WriteFile("./output/out.bin", outHost, outFileSize) ? 0 : 14;
}
#endif

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
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *trace = (uint8_t *)AscendC::GmAlloc(traceFileSize > 1024 ? traceFileSize : 1024);
    FillTraceOnes(ws);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }

    const int32_t rc = RunCpuRounds(out, ws, trace, *tiling, blockDim, outFileSize, traceFileSize);
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)trace);
    AscendC::GmFree((void *)tiling_data);
    return rc;
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

    std::memset(wsHost, 0, wsFileSize);
    FillTraceOnes(wsHost);

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    const int32_t rc =
        RunDeviceRounds(stream, outDevice, wsDevice, traceDevice, outHost, traceHost, tiling, blockDim,
                       outFileSize, traceFileSize);

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
    return rc;
#endif
}
