/**
 * @file main.cpp
 * @brief E03 Host：数字 TRACE + 同进程连续 ≥3 轮 2-launch（L1→μ空→L2）。
 *
 * TRACE 协议（知识库 §6）：仅 printf 三位数字；对照表见 TRACE.md。
 * 轮次：TOY_ROUNDS 环境变量（默认 3）。
 * Host μ：105 空操作（D-host-mu-default）；不写设备、不分配业务缓冲。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling);
#endif

#include <cstdio>
#include <cstdlib>

/** Host TRACE：只打三位数字。 */
static void TraceDigit(int code)
{
    std::printf("%d\n", code);
    std::fflush(stdout);
}

/**
 * Host μ 空操作：仅打 TRACE 105，不写 GM / 不调 kernel。
 * 背景：D-host-mu-default — Encrypt 形态 Host 可空 μ；本 stub 验证空操作可插入。
 */
static void HostMuNop()
{
    TraceDigit(105);
}

/**
 * 主流程：同进程 for N：100→L1→101→105→110→L2→111。
 * @return 0 成功；非 0 文件 I/O 失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int rounds = 3;
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

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }

    for (int r = 0; r < rounds; ++r) {
        TraceDigit(100);
        td->phase = tiling::kPhaseLaunch1;
        ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);
        TraceDigit(101);

        HostMuNop(); // 105：Host μ 空操作

        TraceDigit(110);
        td->phase = tiling::kPhaseLaunch2;
        ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);
        TraceDigit(111);
    }

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
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

    for (int r = 0; r < rounds; ++r) {
        // ---- L1 采样 stub ----
        TraceDigit(100);
        tilingHost->phase = tiling::kPhaseLaunch1;
        ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        TraceDigit(101);

        HostMuNop(); // 105：Host μ 空操作

        // ---- L2 代数 stub + Wait/SET(4) ----
        TraceDigit(110);
        tilingHost->phase = tiling::kPhaseLaunch2;
        ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
        CHECK_ACL(aclrtSynchronizeStream(stream));
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
    CHECK_ACL(aclrtFreeHost(tilingHost));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
