/**
 * KernelLaunch：形状由 MATMUL_M/N/K、MATMUL_BLOCK_DIM 等环境变量控制。
 */
#include "data_utils.h"
#include "kernel_tiling/kernel_tiling.h"
#include "matmul_shape.h"
#include "tiling/platform/platform_ascendc.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_int8_matmul_custom.h"
#else
#include "tikicpulib.h"
extern "C" void int8_matmul_custom(uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *);
#endif
extern void GenerateTiling(const char *socVersion, uint8_t *tilingBuf);

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const size_t m = static_cast<size_t>(matmul_shape::M());
    const size_t n = static_cast<size_t>(matmul_shape::N());
    const size_t k = static_cast<size_t>(matmul_shape::K());
    const size_t aFileSize = m * k * sizeof(int8_t);
    const size_t bFileSize = k * n * sizeof(int8_t);
    const size_t cFileSize = m * n * sizeof(int32_t);
    const uint32_t blockDim = static_cast<uint32_t>(matmul_shape::BlockDim());

    const char *socVersion = SOC_VERSION;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    const size_t tilingFileSize = sizeof(TCubeTiling) + sizeof(uint64_t);
    const size_t workspaceSize = static_cast<size_t>(ascendcPlatform->GetLibApiWorkSpaceSize());
    uint8_t *tilingBuf = static_cast<uint8_t *>(malloc(tilingFileSize));
    GenerateTiling(socVersion, tilingBuf);

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *a = static_cast<uint8_t *>(AscendC::GmAlloc(aFileSize));
    uint8_t *b = static_cast<uint8_t *>(AscendC::GmAlloc(bFileSize));
    uint8_t *c = static_cast<uint8_t *>(AscendC::GmAlloc(cFileSize));
    uint8_t *workspace = static_cast<uint8_t *>(AscendC::GmAlloc(workspaceSize));
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(tilingFileSize));

    size_t aRead = aFileSize;
    size_t bRead = bFileSize;
    ReadFile("./input/x1_gm.bin", aRead, a, aFileSize);
    ReadFile("./input/x2_gm.bin", bRead, b, bFileSize);
    memcpy_s(tiling, tilingFileSize, tilingBuf, tilingFileSize);
    ICPU_RUN_KF(int8_matmul_custom, blockDim, a, b, c, workspace, tiling);

    WriteFile("./output/output.bin", c, cFileSize);
    AscendC::GmFree(a);
    AscendC::GmFree(b);
    AscendC::GmFree(c);
    AscendC::GmFree(workspace);
    AscendC::GmFree(tiling);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *aHost;
    uint8_t *aDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHost), aFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aDevice), aFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ReadFile("./input/x1_gm.bin", aFileSize, aHost, aFileSize);
    CHECK_ACL(aclrtMemcpy(aDevice, aFileSize, aHost, aFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *bHost;
    uint8_t *bDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&bHost), bFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&bDevice), bFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ReadFile("./input/x2_gm.bin", bFileSize, bHost, bFileSize);
    CHECK_ACL(aclrtMemcpy(bDevice, bFileSize, bHost, bFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *workspaceDevice;
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&workspaceDevice), workspaceSize, ACL_MEM_MALLOC_HUGE_FIRST));

    uint8_t *tilingHost;
    uint8_t *tilingDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHost), tilingFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDevice), tilingFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMemcpy(tilingHost, tilingFileSize, tilingBuf, tilingFileSize, ACL_MEMCPY_HOST_TO_HOST));
    CHECK_ACL(aclrtMemcpy(tilingDevice, tilingFileSize, tilingHost, tilingFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *cHost;
    uint8_t *cDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), cFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDevice), cFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    ACLRT_LAUNCH_KERNEL(int8_matmul_custom)
    (blockDim, stream, aDevice, bDevice, cDevice, workspaceDevice, tilingDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(cHost, cFileSize, cDevice, cFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/output.bin", cHost, cFileSize);

    CHECK_ACL(aclrtFree(aDevice));
    CHECK_ACL(aclrtFreeHost(aHost));
    CHECK_ACL(aclrtFree(bDevice));
    CHECK_ACL(aclrtFreeHost(bHost));
    CHECK_ACL(aclrtFree(cDevice));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFree(tilingDevice));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtFree(workspaceDevice));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    free(tilingBuf);
    return 0;
}
