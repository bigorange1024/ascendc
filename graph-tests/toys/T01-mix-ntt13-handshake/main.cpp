/**
 * @file main.cpp
 * @brief Host：装填 LUT=I₃₂、单 launch MIX、打印 TRACE 编号、落盘完成标记。
 *
 * 不对正确性对拍；SIM/NPU 路径同步返回 + TRACE 可见即视为核跑完。
 * Host 编号：101=launch 前，199=SynchronizeStream 返回后（KB §6 Host 100s）。
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

/** Host 打印用：槽下标 → KB 十进制编号（与 trace_map.md 一致）。 */
static const int32_t kTraceCodes[tiling::kTraceSlots] = {201, 301, 401, 402, 203, 303};

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
 * 主流程：读 tiling/lut → 清 TRACE → 单 block MIX launch → Sync → 打印 TRACE → 写 out.bin。
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
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    /* CPU 孪生仅作编译壳；本任务验收以 SIM 为主。 */
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *trace = (uint8_t *)AscendC::GmAlloc(traceFileSize > 1024 ? traceFileSize : 1024);
    std::memset(trace, 0, traceFileSize);
    FillTraceOnes(ws);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    std::printf("101\n"); /* Host launch 前 */
    std::fflush(stdout);
    ICPU_RUN_KF(mmad_custom, blockDim, out, ws, trace, *tiling);
    PrintTraceSlots(reinterpret_cast<const int32_t *>(trace));
    std::printf("199\n"); /* Host sync 后（CPU 路径无真 Sync） */
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

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    std::printf("101\n"); /* Host launch 前 */
    std::fflush(stdout);
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, wsDevice, traceDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    PrintTraceSlots(reinterpret_cast<const int32_t *>(traceHost));
    std::printf("199\n"); /* Host SynchronizeStream 返回后 */
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
