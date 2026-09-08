/**
 * EN02-ntt-intt-2launch · Host 双 launch 粘性壳
 *
 * 基线：复制自 EN01-kem256-ntt-port/main.cpp
 * 作用：同进程、同 device session 内连续两段 launch：
 *   Launch1 — 正向 NTT：input/{src_ntt,M4_ntt}.bin → output/dst_ntt.bin
 *   Launch2 — 逆向 INTT：input/{src_intt,M4_intt}.bin → output/dst_intt.bin
 * 同一 kernel 入口 mmad_custom；仅 Host 换矩阵 digit 与输入。
 * 主门禁：两段均正常返回（不挂）。CPU 孪生 / SIM；禁 npu。
 */
#include "data_utils.h"
#include "tiling.h"
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
#endif

/**
 * 根据 tiling 计算本刀 workspace / src / dst 字节数。
 * Kyber q=3329 用更大 bench tile；与 tiling.h WorkspaceSizeForBench 一致。
 */
static void ResolveSizes(const TilingData *tiling, size_t *srcFileSize, size_t *dstFileSize,
                         size_t *wsFileSize)
{
    const int32_t maxBenchTile = tiling->q == 3329
                               ? tiling_one::max_kernel_bench_tile_kyber
                               : tiling_one::max_kernel_bench_tile;
    const size_t wsBench = tiling->bench < maxBenchTile
                         ? (size_t)tiling->bench
                         : (size_t)maxBenchTile;
    *wsFileSize = tiling_one::WorkspaceSizeForBench(wsBench);
    *srcFileSize = tiling_one::n * sizeof(int32_t) * (size_t)tiling->bench;
    *dstFileSize = tiling_one::n * sizeof(int32_t) * (size_t)tiling->bench;
}

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    // M4：四平面 digit，每平面 n×n int8
    size_t matMFileSize = tiling_one::n * tiling_one::n * 4;
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(std::max(dstFileSize, (size_t)1024));
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, (size_t)1024));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));

    // -------- Launch1：正向 NTT --------
    std::printf("[EN02] CPU Launch1 NTT begin\n");
    ok = ReadFile("./input/src_ntt.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 9;
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 10;
    ICPU_RUN_KF(mmad_custom, blockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_ntt.bin", dst, dstFileSize);
    if (!ok) return 11;
    std::printf("[EN02] CPU Launch1 NTT done\n");

    // -------- Launch2：逆向 INTT（同进程，换 M4+src）--------
    std::printf("[EN02] CPU Launch2 INTT begin\n");
    ok = ReadFile("./input/src_intt.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 19;
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 20;
    ICPU_RUN_KF(mmad_custom, blockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_intt.bin", dst, dstFileSize);
    if (!ok) return 21;
    std::printf("[EN02] CPU Launch2 INTT done\n");

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    // -------- Launch1：正向 NTT（同 stream / session）--------
    std::printf("[EN02] SIM Launch1 NTT begin\n");
    ok = ReadFile("./input/src_ntt.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 9;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 10;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_ntt.bin", dstHost, dstFileSize);
    if (!ok) return 11;
    std::printf("[EN02] SIM Launch1 NTT done\n");

    // -------- Launch2：逆向 INTT（不 recreate stream；禁 B3）--------
    std::printf("[EN02] SIM Launch2 INTT begin\n");
    ok = ReadFile("./input/src_intt.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 19;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 20;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_intt.bin", dstHost, dstFileSize);
    if (!ok) return 21;
    std::printf("[EN02] SIM Launch2 INTT done\n");

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN02] both launches finished OK\n");
    return 0;
}
