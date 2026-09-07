/**
 * @file main.cpp
 * @brief Host：装填 seed/LUT/MAC、单 launch MIX、打印 TRACE（含 SAMPLE 段）、落盘完成标记。
 *
 * T07：SAMPLE 前置 stub + GATE 真 Vec MAC；不对正确性对拍。
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

static const int32_t kTraceCodes[tiling::kTraceSlots] = {
    201, 301, 401, 402, 203, 303,
    403, 204, 304, 404, 205, 305,
    206, 306, 405, 406, 207, 307,
    211, 311, 212, 312
};

static void PrintTraceSlots(const int32_t *slots)
{
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            std::printf("%d\n", static_cast<int>(kTraceCodes[i]));
        }
    }
    std::fflush(stdout);
}

static void FillTraceOnes(uint8_t *ws)
{
    auto *ones = reinterpret_cast<int32_t *>(ws + tiling::TRACE_ONES);
    for (size_t i = 0; i < tiling::kTraceAlignInts; ++i) {
        ones[i] = 1;
    }
}

/** Host 预填 GATE Vec MAC 操作数：a[i]=i+1，b[i]=2，acc[i]=0（每 AIV 64 int32）。 */
static void FillMacOperands(uint8_t *ws)
{
    for (int aiv = 0; aiv < 2; ++aiv) {
        const size_t off = static_cast<size_t>(aiv) * tiling::kMacVecBytes;
        auto *a = reinterpret_cast<int32_t *>(ws + tiling::MAC_A_OFF + off);
        auto *b = reinterpret_cast<int32_t *>(ws + tiling::MAC_B_OFF + off);
        auto *acc = reinterpret_cast<int32_t *>(ws + tiling::MAC_ACC_OFF + off);
        for (size_t i = 0; i < tiling::kMacElems; ++i) {
            a[i] = static_cast<int32_t>(i + 1 + aiv);
            b[i] = 2;
            acc[i] = 0;
        }
    }
}

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
    std::memset(trace, 0, traceFileSize);
    FillTraceOnes(ws);
    FillMacOperands(ws);

    ok = ReadFile("./input/seed.bin", seedFileSize, ws + tiling::SEED, seedFileSize);
    if (!ok) {
        return 10;
    }
    std::printf("108\n");
    std::fflush(stdout);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    std::printf("101\n");
    std::fflush(stdout);
    ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
    PrintTraceSlots(reinterpret_cast<const int32_t *>(trace));
    std::printf("199\n");
    std::fflush(stdout);

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

    std::memset(wsHost, 0, wsFileSize);
    std::memset(traceHost, 0, traceFileSize);
    FillTraceOnes(wsHost);
    FillMacOperands(wsHost);

    ok = ReadFile("./input/seed.bin", seedFileSize, wsHost + tiling::SEED, seedFileSize);
    if (!ok) {
        return 10;
    }
    std::printf("108\n");
    std::fflush(stdout);

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    std::printf("101\n");
    std::fflush(stdout);
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    PrintTraceSlots(reinterpret_cast<const int32_t *>(traceHost));
    std::printf("199\n");
    std::fflush(stdout);

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
