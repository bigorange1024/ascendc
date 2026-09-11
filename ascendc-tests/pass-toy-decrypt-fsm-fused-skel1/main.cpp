/**
 * @file main.cpp
 * @brief Host：softSyncGm/TRACE 清零、同进程多 launch 全骨架 MIX、每轮打印 TRACE、落盘完成标记。
 *
 * Decrypt fused 握手骨架（Soft×2 + GATE×2 + NTT/INTT 1/3）；不对正确性对拍。
 * SIM 路径每轮同步返回且 TRACE/完成标记可见即视为该轮骨架跑完。
 * 若挂在 SoftSync 忙等或 CrossCore Wait，aclrtSynchronizeStream 不返回 → run.sh 超时 124。
 *
 * 背景：DGT-20260903-4 / Q-TOY-MULTI-LAUNCH — 同进程多 launch 加压 SIM，搜 SynchronizeStream 挂死。
 * 结论：每轮 H2D 清 TRACE+softSync → launch 全融合骨架 → Sync → 打印 TRACE（对照 encrypt l18-skel1）。
 * 未采用：改 SoftSync/GATE/NTT 握手碰运气；第二进程外层反复 run.sh。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR softSyncGm, GM_ADDR trace,
                            TilingData tiling);
#endif

/**
 * 打印已置位 TRACE 逻辑槽（每槽 32B 块，读块首 int32；仿 GT-4 / l18-trace）。
 * @param iter  当前轮次（0-based）
 * @param slots Host 侧 int32 缓冲，长度 kTraceSlots * kTraceAlignInts
 */
static void PrintTraceSlots(int iter, const int32_t *slots)
{
    int pop = 0;
    for (size_t i = 0; i < tiling::kTraceSlots; ++i) {
        if (slots[i * tiling::kTraceAlignInts] != 0) {
            ++pop;
        }
    }
    std::printf("[toy-fused-skel-trace] iter=%d stages set=%d/%zu :", iter, pop,
                tiling::kTraceSlots);
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
 * Host 预填 AIC TRACE 用的全 1 模板（ws+TRACE_ONES，8×int32）。
 */
static void FillTraceOnes(uint8_t *ws)
{
    auto *ones = reinterpret_cast<int32_t *>(ws + tiling::TRACE_ONES);
    for (size_t i = 0; i < tiling::kTraceAlignInts; ++i) {
        ones[i] = 1;
    }
}

/**
 * 解析同进程全骨架 launch 次数：环境变量 TOY_LAUNCH_REPEAT，默认 16；非法/≤0 回落默认。
 * 对照 pass-toy-encrypt-fsm-l18-skel1（活跃 toy）；本目录无 TOY_SPLIT_2LAUNCH。
 */
static int ParseLaunchRepeat()
{
    int launchRepeat = 16;
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
 * 主流程：分配缓冲 → 同进程多轮（清 TRACE+softSync → launch → sync → 打印）→ 写 output/out.bin。
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
    size_t traceFileSize = tiling::kTraceBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    const int launchRepeat = ParseLaunchRepeat();
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
    uint8_t *trace = (uint8_t *)AscendC::GmAlloc(traceFileSize > 1024 ? traceFileSize : 1024);
    std::memset(out, 0, outFileSize);
    std::memset(ws, 0, wsFileSize);

    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    // S0 Host 预填常数，供 AIC NTT/INTT 段 MMAD（禁真 unpack）
    std::memset(ws + tiling::S0, 1, tiling::kS0Bytes);

    std::printf("[host] TOY_LAUNCH_REPEAT=%d (CPU 壳；不作验收)\n", launchRepeat);
    std::fflush(stdout);
    /*
     * 背景：DGT-20260903-4 SIM 加压搜挂死（CPU 仅镜像循环形状）。
     * 未采用：改 SoftSync/GATE/NTT 握手碰运气。
     */
    for (int iter = 0; iter < launchRepeat; ++iter) {
        std::memset(softSync, 0, softSyncBytes);
        std::memset(trace, 0, traceFileSize);
        FillTraceOnes(ws);
        std::printf("[host] iter=%d/%d FULL fused begin\n", iter, launchRepeat);
        std::fflush(stdout);
        ICPU_RUN_KF(mmad_custom, blockDim, out, ws, softSync, trace, *tiling);
        std::printf("[host] iter=%d/%d FULL fused sync done\n", iter, launchRepeat);
        std::fflush(stdout);
        PrintTraceSlots(iter, reinterpret_cast<const int32_t *>(trace));
    }

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)softSync);
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

    uint8_t *outHost, *wsHost, *softSyncHost, *traceHost;
    uint8_t *outDevice, *wsDevice, *softSyncDevice, *traceDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&softSyncHost), softSyncBytes));
    CHECK_ACL(aclrtMalloc((void **)&softSyncDevice, softSyncBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&traceHost), traceFileSize));
    CHECK_ACL(aclrtMalloc((void **)&traceDevice, traceFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(outHost, 0, outFileSize);
    std::memset(wsHost, 0, wsFileSize);
    FillTraceOnes(wsHost);

    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    std::memset(wsHost + tiling::S0, 1, tiling::kS0Bytes);

    /* out / ws（LUT、S0、TRACE_ONES）只 H2D 一次；每轮只重清 softSync+TRACE */
    CHECK_ACL(aclrtMemcpy(outDevice, outFileSize, outHost, outFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    /*
     * 背景：DGT-20260903-4 / Q-TOY-MULTI-LAUNCH — SIM 同进程多 launch 加压搜 SynchronizeStream 挂死。
     * 每轮：H2D 清 TRACE + softSyncGm → launch 全融合骨架 → Sync → 打印 TRACE（对照 encrypt l18）。
     * 未采用：改 SoftSync/GATE/NTT 握手碰运气；第二进程外层反复整树 run.sh。
     */
    std::printf("[host] TOY_LAUNCH_REPEAT=%d FULL Soft×2+GATE×2+NTT/INTT1/3 "
                "(DGT-20260903-4; clear TRACE+softSync each round)\n",
                launchRepeat);
    std::fflush(stdout);

    for (int iter = 0; iter < launchRepeat; ++iter) {
        // F-HOST-ZERO-SOFTSYNC：每轮启动前 softSyncGm 全 0
        std::memset(softSyncHost, 0, softSyncBytes);
        std::memset(traceHost, 0, traceFileSize);
        CHECK_ACL(aclrtMemcpy(softSyncDevice, softSyncBytes, softSyncHost, softSyncBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(traceDevice, traceFileSize, traceHost, traceFileSize,
                              ACL_MEMCPY_HOST_TO_DEVICE));

        std::printf("[host] iter=%d/%d FULL fused begin\n", iter, launchRepeat);
        std::fflush(stdout);
        ACLRT_LAUNCH_KERNEL(mmad_custom)
        (blockDim, stream, outDevice, wsDevice, softSyncDevice, traceDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        std::printf("[host] iter=%d/%d FULL fused sync done\n", iter, launchRepeat);
        std::fflush(stdout);

        CHECK_ACL(aclrtMemcpy(traceHost, traceFileSize, traceDevice, traceFileSize,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        PrintTraceSlots(iter, reinterpret_cast<const int32_t *>(traceHost));
    }

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(softSyncHost, softSyncBytes, softSyncDevice, softSyncBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));

    {
        const int32_t *ss = reinterpret_cast<const int32_t *>(softSyncHost);
        printf("[host] softSyncGm slot0=%d slot1=%d (expect 0 after Clear)\n", ss[0], ss[1]);
        fflush(stdout);
    }

    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }

    // PASS 门禁：AIV0 完成标记非 0（证明末轮已过双 Soft+双 GATE+NTT/INTT）
    const int32_t mark0 = reinterpret_cast<const int32_t *>(outHost)[0];
    if (mark0 == 0) {
        printf("[FAIL] fused skel completion mark slot0 still 0 (hang or mark missing)\n");
        fflush(stdout);
        return 20;
    }
    printf("[host] fused skel completion mark OK mark0=0x%08X (after %d launches)\n",
           static_cast<unsigned>(mark0), launchRepeat);
    fflush(stdout);

    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(softSyncDevice));
    CHECK_ACL(aclrtFreeHost(softSyncHost));
    CHECK_ACL(aclrtFree(traceDevice));
    CHECK_ACL(aclrtFreeHost(traceHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
