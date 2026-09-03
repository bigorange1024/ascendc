/**
 * @file main.cpp
 * @brief Decrypt 握手骨架 toy Host：装填 GM、初始化 softSync、**单趟** MIX launch、落盘 out。
 *
 * GM：out[64B] / src[64B 占位] / ws[S0|LUT|MAT_C|STUB] / softSync[64B，前 2×int32]；
 * LUT=I₃₂ 由 host 预填。
 *
 * softSync 初始化（运行时 env `SKEL_SOFTSYNC_PREFILL`，非 cmake 宏；测 J-dirty-softsync-hang-vs-race）：
 *   - 0（默认）：launch 前清零（F-host-zeros-softsync 生产定式）
 *   - 1：launch 前把 softSync int32[2] **都写成 1**（脏非零）→ AIV1 跳过自旋（误放行），**不是** hang 注入
 * 设备侧 SoftSync 逻辑不改；本 Host 开关不打开任何 OMIT_*。
 * 验收不对算法正确性，只要求 kernel 跑完且 out 含约定 magic（SKELDEC1）。
 */
#include "data_utils.h"
#include "tiling.h"
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR softSyncGm, TilingData tiling);
#endif

/**
 * 读运行时 env `SKEL_SOFTSYNC_PREFILL`（默认 0）。
 * @return 0=清零；1=预填 int32 脏值 1；非法值时 stderr 提示并当 0
 */
static int SoftSyncPrefillMode()
{
    const char *env = std::getenv("SKEL_SOFTSYNC_PREFILL");
    if (env == nullptr || env[0] == '\0' || (env[0] == '0' && env[1] == '\0')) {
        return 0;
    }
    if (env[0] == '1' && env[1] == '\0') {
        return 1;
    }
    std::fprintf(stderr, "[WARN] SKEL_SOFTSYNC_PREFILL must be 0 or 1, got '%s'; treat as 0\n", env);
    return 0;
}

/**
 * Host 侧初始化 softSync 缓冲（64B；有效语义为前 2×int32）。
 * @param softSyncHost host 缓冲指针
 * @param softSyncSize 分配字节数（tiling::kSoftSyncBytes）
 * @param prefill 0=全零；1=两槽均写 int32(1)，其余字节仍 0
 *
 * 背景：TASK-012 测脏哨兵 vs hang；结论预期两档均绿（脏非零≠SIM hang）。
 */
static void InitSoftSyncHost(uint8_t *softSyncHost, size_t softSyncSize, int prefill)
{
    for (size_t i = 0; i < softSyncSize; ++i) {
        softSyncHost[i] = 0;
    }
    if (prefill == 1) {
        // 脏非零：slot0/slot1 写成 1，使 AIV1 SoftSync 自旋条件已满足（误放行），非 hang
        auto *slots = reinterpret_cast<int32_t *>(softSyncHost);
        slots[0] = 1;
        slots[1] = 1;
    }
}

/**
 * 主流程：读 tiling/src/lut → 按 SKEL_SOFTSYNC_PREFILL 初始化 softSync → 1 次 MIX launch → 写 output/out.bin。
 * @return 0 成功；非 0 文件 I/O 失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const int softSyncPrefill = SoftSyncPrefillMode();
    std::printf("SKEL_SOFTSYNC_PREFILL=%d\n", softSyncPrefill);

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kSrcBytes;
    size_t outFileSize = tiling::kOutBytes;
    size_t lutFileSize = tiling::kLutBytes;
    size_t softSyncSize = tiling::kSoftSyncBytes;
    uint32_t blockDim = 1; /**< 单 block MIX：1 AIC + 2 AIV */
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    uint8_t *softSync = (uint8_t *)AscendC::GmAlloc(softSyncSize > 1024 ? softSyncSize : 1024);

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    // softSync：默认清零；SKEL_SOFTSYNC_PREFILL=1 时两槽写 1（脏哨兵≠hang，见文件头）
    InitSoftSyncHost(softSync, softSyncSize, softSyncPrefill);
    ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, softSync, *tiling);

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)src);
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

    uint8_t *outHost, *srcHost, *wsHost, *softSyncHost;
    uint8_t *outDevice, *srcDevice, *wsDevice, *softSyncDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

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
    // workspace 先清零，再写入 LUT
    for (size_t i = 0; i < wsFileSize; ++i) {
        wsHost[i] = 0;
    }
    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // softSyncGm：默认清零（生产定式）；PREFILL=1 脏写 int32[2]=1（TASK-012 / J-dirty-softsync-hang-vs-race）
    CHECK_ACL(aclrtMallocHost((void **)(&softSyncHost), softSyncSize));
    CHECK_ACL(aclrtMalloc((void **)&softSyncDevice, softSyncSize, ACL_MEM_MALLOC_HUGE_FIRST));
    InitSoftSyncHost(softSyncHost, softSyncSize, softSyncPrefill);
    CHECK_ACL(aclrtMemcpy(softSyncDevice, softSyncSize, softSyncHost, softSyncSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // 唯一 MIX launch（禁止滥增 launch）
    ACLRT_LAUNCH_KERNEL(mmad_custom)
    (blockDim, stream, outDevice, srcDevice, wsDevice, softSyncDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

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
    CHECK_ACL(aclrtFree(softSyncDevice));
    CHECK_ACL(aclrtFreeHost(softSyncHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
