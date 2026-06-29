/**
 * @file main.cpp
 * f203 Stage2 纯 Cube int8 MatMul：mat_a_gm x mat_b_lut_gm -> mat_c_gm
 */
#include "data_utils.h"
#include "kernel_tiling/kernel_tiling.h"
#include "launch_profile.h"
#include "tiling/platform/platform_ascendc.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mlkem_stage2_matmul_custom.h"
#else
#include "tikicpulib.h"
extern "C" void mlkem_stage2_matmul_custom(uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *);
#endif
extern void GenerateTiling(const char *socVersion, uint8_t *tilingBuf);

namespace {
constexpr size_t kM = 2;
constexpr size_t kK = 256;
constexpr size_t kN = 512;
constexpr size_t kAFileSize = kM * kK * sizeof(int8_t);
constexpr size_t kBFileSize = kK * kN * sizeof(int8_t);
constexpr size_t kCFileSize = kM * kN * sizeof(int32_t);
} // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const auto launchCfg = launch_profile::Get(launch_profile::FromEnv());
    const uint32_t blockDim = launchCfg.blockDim;

    const char *socVersion = SOC_VERSION;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    size_t tilingFileSize = sizeof(TCubeTiling) + sizeof(uint64_t);
    size_t systemWorkspaceSize = static_cast<size_t>(ascendcPlatform->GetLibApiWorkSpaceSize());
    size_t workspaceSize = systemWorkspaceSize;
    uint8_t *tilingBuf = static_cast<uint8_t *>(malloc(tilingFileSize));
    GenerateTiling(socVersion, tilingBuf);

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *a = static_cast<uint8_t *>(AscendC::GmAlloc(kAFileSize));
    uint8_t *b = static_cast<uint8_t *>(AscendC::GmAlloc(kBFileSize));
    uint8_t *c = static_cast<uint8_t *>(AscendC::GmAlloc(kCFileSize));
    uint8_t *workspace = static_cast<uint8_t *>(AscendC::GmAlloc(workspaceSize));
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(tilingFileSize));

    size_t aFileSize = kAFileSize;
    size_t bFileSize = kBFileSize;
    ReadFile("./input/mat_a_gm.bin", aFileSize, a, kAFileSize);
    ReadFile("./input/mat_b_lut_gm.bin", bFileSize, b, kBFileSize);
    memcpy_s(tiling, tilingFileSize, tilingBuf, tilingFileSize);
    ICPU_RUN_KF(mlkem_stage2_matmul_custom, blockDim, a, b, c, workspace, tiling);

    WriteFile("./output/output.bin", c, kCFileSize);
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
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHost), kAFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aDevice), kAFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t aFileSize = kAFileSize;
    ReadFile("./input/mat_a_gm.bin", aFileSize, aHost, kAFileSize);
    CHECK_ACL(aclrtMemcpy(aDevice, kAFileSize, aHost, kAFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *bHost;
    uint8_t *bDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&bHost), kBFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&bDevice), kBFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t bFileSize = kBFileSize;
    ReadFile("./input/mat_b_lut_gm.bin", bFileSize, bHost, kBFileSize);
    CHECK_ACL(aclrtMemcpy(bDevice, kBFileSize, bHost, kBFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

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
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), kCFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDevice), kCFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    ACLRT_LAUNCH_KERNEL(mlkem_stage2_matmul_custom)
    (blockDim, stream, aDevice, bDevice, cDevice, workspaceDevice, tilingDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(cHost, kCFileSize, cDevice, kCFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/output.bin", cHost, kCFileSize);

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
