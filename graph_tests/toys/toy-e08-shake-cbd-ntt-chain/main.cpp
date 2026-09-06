/**
 * @file main.cpp
 * @brief E08 Host：数字 TRACE + ≥3 轮 2-launch（L1 真 SHAKE+CBD → μ → L2 真 NTT+basemul+INTT+SET4）。
 *
 * L1 Sync 后：D2H SHAKE y（out 前缀 32B）+ D2H CBD 覆写后的 src（供 CBD golden）。
 * 加载 ntt256 风格 input（M4 + Minv4 + g→G0 + prf→P0）；src 初值可任意，L1 CBD 覆盖。
 * 输出 = INTT(MultiplyNTTs(NTT(CBD(prf)),ĝ))。
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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/** Host TRACE：只打三位数字。 */
static void TraceDigit(int code)
{
    std::printf("%d\n", code);
    std::fflush(stdout);
}

/** Host μ 空操作：仅 TRACE 105。 */
static void HostMuNop()
{
    TraceDigit(105);
}

/**
 * 主流程：同进程 for N：100→L1→101→(D2H shake+cbd)→105→110→L2→111。
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
    size_t matMFileSize = tiling::n * tiling::n * 4;
    size_t gFileSize = tiling::kGBytes;
    size_t prfFileSize = tiling::kPrfBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    const size_t shakeYBytes = tiling::kShakeYBytes;
    bool ok;

    uint8_t shakeYHost[tiling::kShakeYBytes] = {0};
    // 最后一轮 L1 后的 CBD 输出（src）
    int32_t cbdHost[tiling::n] = {0};

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *td = (TilingData *)tiling_data;
    td->tileLength = static_cast<int32_t>(tiling::n);

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(std::max(outFileSize, (size_t)1024));
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, (size_t)1024));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));

    // src 初值占位；L1 CBD 会覆盖
    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/M4.bin", matMFileSize, ws + tiling::M0, matMFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/g.bin", gFileSize, ws + tiling::G0, gFileSize);
    if (!ok) {
        return 11;
    }
    ok = ReadFile("./input/Minv4.bin", matMFileSize, ws + tiling::Minv0, matMFileSize);
    if (!ok) {
        return 12;
    }
    ok = ReadFile("./input/prf.bin", prfFileSize, ws + tiling::P0, prfFileSize);
    if (!ok) {
        return 16;
    }

    for (int r = 0; r < rounds; ++r) {
        TraceDigit(100);
        td->phase = tiling::kPhaseLaunch1;
        ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);
        TraceDigit(101);
        std::memcpy(shakeYHost, out, shakeYBytes);
        std::memcpy(cbdHost, src, srcFileSize);

        HostMuNop();

        TraceDigit(110);
        td->phase = tiling::kPhaseLaunch2;
        ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);
        TraceDigit(111);
    }

    ok = WriteFile("./output/shake_y.bin", shakeYHost, shakeYBytes);
    if (!ok) {
        return 13;
    }
    ok = WriteFile("./output/cbd_src.bin", cbdHost, srcFileSize);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/dst.bin", out, outFileSize);
    if (!ok) {
        return 15;
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
    tilingHost->tileLength = static_cast<int32_t>(tiling::n);

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
    ok = ReadFile("./input/M4.bin", matMFileSize, wsHost + tiling::M0, matMFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/g.bin", gFileSize, wsHost + tiling::G0, gFileSize);
    if (!ok) {
        return 11;
    }
    ok = ReadFile("./input/Minv4.bin", matMFileSize, wsHost + tiling::Minv0, matMFileSize);
    if (!ok) {
        return 12;
    }
    ok = ReadFile("./input/prf.bin", prfFileSize, wsHost + tiling::P0, prfFileSize);
    if (!ok) {
        return 16;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    for (int r = 0; r < rounds; ++r) {
        TraceDigit(100);
        tilingHost->phase = tiling::kPhaseLaunch1;
        ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        TraceDigit(101);
        CHECK_ACL(aclrtMemcpy(shakeYHost, shakeYBytes, outDevice, shakeYBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(cbdHost, srcFileSize, srcDevice, srcFileSize, ACL_MEMCPY_DEVICE_TO_HOST));

        HostMuNop();

        TraceDigit(110);
        tilingHost->phase = tiling::kPhaseLaunch2;
        ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        TraceDigit(111);
    }

    ok = WriteFile("./output/shake_y.bin", shakeYHost, shakeYBytes);
    if (!ok) {
        return 13;
    }
    ok = WriteFile("./output/cbd_src.bin", cbdHost, srcFileSize);
    if (!ok) {
        return 17;
    }

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/dst.bin", outHost, outFileSize);
    if (!ok) {
        return 15;
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
