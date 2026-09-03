/**
 * @file main.cpp
 * @brief 干净 Encrypt P0 Host：**两趟** MIX launch + **始终** Host 折 μ 小缓冲。
 *
 * 流程（结构即约束，无「开关才走 Host μ」路径）：
 *   1. Launch1（phase=0）：prep+NTT 一轮 Cube；设备不做 μ
 *   2. HostFoldMuAlways：向 e2Fold[8] 写入 MU01 标记（占位 e₂+=μ）
 *   3. H2D MU_FOLD 槽；Launch2（phase=1）：skipNtt，设备无 PrefixEmbed
 *
 * 验收：kernel 跑完 + out magic CLNENC01 / out[8]=0x21。
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
#include <cstring>

/**
 * Host 折 μ（P0 占位）：始终执行，写入 e2Fold[0]=MU01，其余清零。
 * 背景：F-host-mu-ok-sim / D-next-clean-p0 — μ 折叠默认在 Host，非调试开关。
 * 结论：本缓冲在 L2 前 H2D 到 ws+MU_FOLD；设备 L2 不读做 PrefixEmbed。
 * @param e2Fold Host 侧小缓冲，长度 tiling::kMuFoldElems
 */
static void HostFoldMuAlways(int32_t *e2Fold)
{
    for (size_t i = 0; i < tiling::kMuFoldElems; ++i) {
        e2Fold[i] = 0;
    }
    e2Fold[0] = tiling::kHostMuFoldMark;
    // 占位「+=μ」：e2Fold[1] 置 1，证明 Host 侧发生了折叠写
    e2Fold[1] = 1;
    std::printf("[clean-enc] HostFoldMuAlways: e2Fold[0]=MU01 e2Fold[1]=1 (default, not optional)\n");
}

/**
 * 主流程：L1 → HostFold → L2 → 写 output/out.bin。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kSrcBytes;
    size_t outFileSize = tiling::kOutBytes;
    size_t lutFileSize = tiling::kLutBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }

    // ---- Launch1 ----
    TilingData *td = (TilingData *)tiling_data;
    td->phase = tiling::kPhaseLaunch1;
    ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);

    // ---- HostFold（始终）----
    int32_t e2Fold[8];
    static_assert(tiling::kMuFoldElems == 8, "");
    HostFoldMuAlways(e2Fold);
    std::memcpy(ws + tiling::MU_FOLD, e2Fold, tiling::kMuFoldBytes);

    // ---- Launch2 ----
    td->phase = tiling::kPhaseLaunch2;
    ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *td);

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
    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---- Launch1：prep+NTT ----
    tilingHost->phase = tiling::kPhaseLaunch1;
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    std::printf("[clean-enc] Launch1 (prep+NTT) done\n");

    // ---- HostFold：始终折 μ 进小缓冲，再 H2D 到 MU_FOLD ----
    int32_t e2Fold[8];
    static_assert(tiling::kMuFoldElems == 8, "");
    HostFoldMuAlways(e2Fold);
    std::memcpy(wsHost + tiling::MU_FOLD, e2Fold, tiling::kMuFoldBytes);
    CHECK_ACL(aclrtMemcpy(wsDevice + tiling::MU_FOLD, tiling::kMuFoldBytes, wsHost + tiling::MU_FOLD,
                          tiling::kMuFoldBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---- Launch2：skipNtt（设备无 PrefixEmbed）----
    tilingHost->phase = tiling::kPhaseLaunch2;
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    std::printf("[clean-enc] Launch2 (skipNtt, no PrefixEmbed) done\n");

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
