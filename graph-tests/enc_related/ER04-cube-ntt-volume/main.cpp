/**
 * @file main.cpp
 * @brief Host：装填 seed/LUT/MAC，串行 2 launch（PREP→COMPUTE），打印 TRACE，落盘完成标记。
 *
 * ER04：Encrypt 外形双 launch + GATE MAC 256×32 + NTT/INTT Cube×16；不对正确性对拍（非门禁）。
 * Host 编号：110=prep launch 前，120=compute launch 前，199=末次 Sync 返回后。
 * MAC：优先读 gen_data 写出的 mac_{a,b,acc}.bin；缺文件则按同规则现场填充。
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

/** Host 打印用：槽下标 → KB 三位十进制编号（与 trace_map.md 一致）。 */
static const int32_t kTraceCodes[tiling::kTraceSlots] = {
    201, 301, 401, 402, 203, 303, /* NTT */
    403, 204, 304, 404, 205, 305, /* GATE */
    206, 306, 405, 406, 207, 307, /* INTT */
    211, 311, 212, 312            /* SAMPLE / prep */
};

/**
 * 打印已置位 TRACE：每槽 32B 块首 int32≠0 则输出对应编号（一行一个）。
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

/** Host 预填 AIC TRACE 用的全 1 模板。 */
static void FillTraceOnes(uint8_t *ws)
{
    auto *ones = reinterpret_cast<int32_t *>(ws + tiling::TRACE_ONES);
    for (size_t i = 0; i < tiling::kTraceAlignInts; ++i) {
        ones[i] = 1;
    }
}

/**
 * Host 装填 GATE Vec MAC：读 gen_data 写出的 mac_{a,b,acc}.bin（各 2×kMacElems int32）。
 * 规则与 gen_data 一致：a[i]=i+1+aiv，b[i]=2，acc[i]=0。缺文件则失败（由调用方处理）。
 */
static bool LoadMacOperands(uint8_t *ws)
{
    size_t macSlice = tiling::kMacVecBytes * 2;
    size_t aSz = macSlice;
    size_t bSz = macSlice;
    size_t accSz = macSlice;
    if (!ReadFile("./input/mac_a.bin", aSz, ws + tiling::MAC_A_OFF, macSlice)) {
        return false;
    }
    if (!ReadFile("./input/mac_b.bin", bSz, ws + tiling::MAC_B_OFF, macSlice)) {
        return false;
    }
    if (!ReadFile("./input/mac_acc.bin", accSz, ws + tiling::MAC_ACC_OFF, macSlice)) {
        return false;
    }
    return true;
}

/** 清零 TRACE 缓冲（每轮 launch 前）。 */
static void ClearTraceBuffer(void *traceHost, size_t traceFileSize)
{
    std::memset(traceHost, 0, traceFileSize);
}

/**
 * 设置 tiling.phase 并（设备路径）H2D；CPU 路径直接改本地质子。
 */
static void SetPhase(TilingData *tiling, int32_t phase)
{
    tiling->phase = phase;
}

#ifdef ASCENDC_CPU_DEBUG
/**
 * CPU 孪生：prep → compute 各 ICPU_RUN_KF 一次。
 */
static int32_t RunCpuTwoLaunch(uint8_t *out, uint8_t *ws, uint8_t *trace, TilingData *tiling,
                               uint32_t blockDim, size_t outFileSize, size_t traceFileSize)
{
    auto *traceSlots = reinterpret_cast<int32_t *>(trace);

    /* ---- Launch1 PREP ---- */
    ClearTraceBuffer(traceSlots, traceFileSize);
    SetPhase(tiling, ER01_PHASE_PREP);
    std::printf("110\n");
    std::fflush(stdout);
    ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
    PrintTraceSlots(traceSlots);

    /* ---- Launch2 COMPUTE ---- */
    ClearTraceBuffer(traceSlots, traceFileSize);
    SetPhase(tiling, ER01_PHASE_COMPUTE);
    std::printf("120\n");
    std::fflush(stdout);
    ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
    PrintTraceSlots(traceSlots);

    std::printf("199\n");
    std::fflush(stdout);
    return WriteFile("./output/out.bin", out, outFileSize) ? 0 : 14;
}
#else
/**
 * SIM/NPU：prep → Sync → compute → Sync；每轮 D2H 打印 TRACE。
 */
static int32_t RunDeviceTwoLaunch(aclrtStream stream, uint8_t *outDevice, uint8_t *wsDevice,
                                  uint8_t *traceDevice, uint8_t *outHost, uint8_t *traceHost,
                                  TilingData *tiling, uint32_t blockDim, size_t outFileSize,
                                  size_t traceFileSize, size_t tilingSize)
{
    /* ---- Launch1 PREP ---- */
    ClearTraceBuffer(traceHost, traceFileSize);
    CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    SetPhase(tiling, ER01_PHASE_PREP);
    std::printf("110\n");
    std::fflush(stdout);
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    PrintTraceSlots(reinterpret_cast<const int32_t *>(traceHost));

    /* ---- Launch2 COMPUTE ---- */
    ClearTraceBuffer(traceHost, traceFileSize);
    CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    SetPhase(tiling, ER01_PHASE_COMPUTE);
    std::printf("120\n");
    std::fflush(stdout);
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    PrintTraceSlots(reinterpret_cast<const int32_t *>(traceHost));

    std::printf("199\n");
    std::fflush(stdout);

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    (void)tilingSize;
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
    size_t seedFileSize = tiling::kSeedBytes;
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
    std::memset(ws, 0, wsFileSize);
    FillTraceOnes(ws);
    if (!LoadMacOperands(ws)) {
        return 10;
    }

    ok = ReadFile("./input/seed.bin", seedFileSize, ws + tiling::SEED, seedFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }

    const int32_t rc =
        RunCpuTwoLaunch(out, ws, trace, tiling, blockDim, outFileSize, traceFileSize);
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
    std::memset(traceHost, 0, traceFileSize);
    FillTraceOnes(wsHost);
    if (!LoadMacOperands(wsHost)) {
        return 10;
    }

    ok = ReadFile("./input/seed.bin", seedFileSize, wsHost + tiling::SEED, seedFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    const int32_t rc =
        RunDeviceTwoLaunch(stream, outDevice, wsDevice, traceDevice, outHost, traceHost, tiling,
                           blockDim, outFileSize, traceFileSize, tilingSize);

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
