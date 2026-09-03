/**
 * @file main.cpp
 * @brief Host：softSyncGm H2D 清零、单 launch MIX、打印 TRACE 槽、落盘完成标记。
 *
 * 不对正确性对拍；SIM 路径同步返回且槽位可见完成码即视为 SoftSync 跑完。
 * 若挂在 AIV1 busy-wait，aclrtSynchronizeStream 不返回 → run.sh 超时 124。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR softSyncGm, TilingData tiling);
#endif

/**
 * 打印 TRACE 逻辑槽（每槽 32B 块，读块首 int32；仿 GT-4 / [toy-l18-trace]）。
 * @param slots Host 侧 int32 缓冲，长度 kTraceSlots * kTraceAlignInts
 */
static void PrintTraceSlots(const int32_t *slots)
{
    int pop = 0;
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            ++pop;
        }
    }
    std::printf("[toy-softsync-trace] SoftSyncArrive slots set=%d/%zu :", pop, tiling::kTraceSlots);
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        const int32_t v = slots[i * tiling::kTraceAlignInts];
        if (v != 0) {
            std::printf(" s%zu=0x%08X", i, static_cast<unsigned>(v));
        } else {
            std::printf(" s%zu=0", i);
        }
    }
    std::printf("\n");
    std::fflush(stdout);
}

/**
 * 主流程：分配 softSyncGm 清零 → 单 block MIX launch → 打印槽 → 写 output/out.bin。
 * @return 0 成功且见完成标记；非 0 读输入/写输出失败或未见标记
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t outFileSize = tiling::kOutBytes;
    size_t lutFileSize = tiling::kLutBytes;
    size_t softSyncBytes = tiling::kSoftSyncBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    /* CPU 孪生仅作编译壳；本任务验收不认 CPU（D-RG-SIM-PRIMARY）。 */
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *softSync = (uint8_t *)AscendC::GmAlloc(softSyncBytes > 1024 ? softSyncBytes : 1024);
    std::memset(out, 0, outFileSize);
    std::memset(ws, 0, wsFileSize);
    std::memset(softSync, 0, softSyncBytes);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    // S0 Host 预填常数，供 AIC 独立 MMAD（本刀 AIV 不写 S0）
    std::memset(ws + tiling::S0, 1, tiling::kS0Bytes);

    ICPU_RUN_KF(mmad_custom, blockDim, out, ws, softSync, *tiling);

    PrintTraceSlots(reinterpret_cast<const int32_t *>(out));
    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)softSync);
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

    uint8_t *outHost, *wsHost, *softSyncHost;
    uint8_t *outDevice, *wsDevice, *softSyncDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&softSyncHost), softSyncBytes));
    CHECK_ACL(aclrtMalloc((void **)&softSyncDevice, softSyncBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(outHost, 0, outFileSize);
    std::memset(wsHost, 0, wsFileSize);
    // F-HOST-ZERO-SOFTSYNC：启动前 softSyncGm 全 0
    std::memset(softSyncHost, 0, softSyncBytes);

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    std::memset(wsHost + tiling::S0, 1, tiling::kS0Bytes);

    CHECK_ACL(aclrtMemcpy(outDevice, outFileSize, outHost, outFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(softSyncDevice, softSyncBytes, softSyncHost, softSyncBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    printf("[host] launch mmad_custom MIX_AIC_1_2 SoftSyncArrive slot0 (DGT-20260903-1)\n");
    fflush(stdout);
    ACLRT_LAUNCH_KERNEL(mmad_custom)
    (blockDim, stream, outDevice, wsDevice, softSyncDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    printf("[host] synchronize done (SoftSyncArrive returned)\n");
    fflush(stdout);

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(softSyncHost, softSyncBytes, softSyncDevice, softSyncBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));

    PrintTraceSlots(reinterpret_cast<const int32_t *>(outHost));
    {
        const int32_t *ss = reinterpret_cast<const int32_t *>(softSyncHost);
        printf("[host] softSyncGm slot0=%d slot1=%d\n", ss[0], ss[1]);
        fflush(stdout);
    }

    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }

    // PASS 门禁：AIV0 完成标记非 0（证明已过 SoftSync 并写出 TRACE）
    const int32_t mark0 = reinterpret_cast<const int32_t *>(outHost)[0];
    if (mark0 == 0) {
        printf("[FAIL] SoftSync completion mark slot0 still 0 (hang-in-busy-wait or mark missing)\n");
        fflush(stdout);
        return 20;
    }
    printf("[host] SoftSync completion mark OK mark0=0x%08X\n", static_cast<unsigned>(mark0));
    fflush(stdout);

    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(softSyncDevice));
    CHECK_ACL(aclrtFreeHost(softSyncHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
