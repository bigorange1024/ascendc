#include <cstdlib>
#include <cstring>

#include "data_utils.h"
#include "kernel_tiling/kernel_tiling.h"
#include "tiling.h"
#include "launch_profile.h"
#include "tiling/platform/platform_ascendc.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#ifdef S12_HAVE_MMAD
#include "aclrtlaunch_mmad_custom.h"
#endif
#ifdef S12_HAVE_STAGE2
#ifdef S12_STAGE2_INT8_KERNEL
#include "aclrtlaunch_int8_matmul_custom.h"
#else
#include "aclrtlaunch_stage2_matmul_custom.h"
#endif
#endif
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern volatile int g_mix_s12_pass;
extern "C" void mmad_custom(GM_ADDR matC, GM_ADDR src, GM_ADDR ws, GM_ADDR mmWorkspace, GM_ADDR tilingGm,
                            TilingData tiling);
extern "C" void stage2_matmul_custom(GM_ADDR matA, GM_ADDR lut, GM_ADDR matC, GM_ADDR mmWorkspace, GM_ADDR tilingGm);
#endif
extern void GenerateTiling(const char *socVersion, uint8_t *tilingBuf);

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

#if defined(S12_HAVE_MMAD)
    constexpr uint32_t kStage1BlockDim = 1;
#endif
    size_t tilingDataSize = sizeof(TilingData);
    size_t srcFileSize = tiling::kPolys * tiling::n * sizeof(int32_t);
    const size_t matCFileSize = tiling::mRows * tiling::outCols * sizeof(int32_t);
    size_t bFileSize = tiling::n * tiling::outCols;
    const size_t wsFileSize = tiling::wssize;

    const char *socVersion = SOC_VERSION;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    const size_t cubeTilingSize = sizeof(TCubeTiling) + sizeof(uint64_t);
    const size_t mmWorkspaceSize = static_cast<size_t>(ascendcPlatform->GetLibApiWorkSpaceSize());

    uint8_t *cubeTilingBuf = static_cast<uint8_t *>(malloc(cubeTilingSize));
    GenerateTiling(socVersion, cubeTilingBuf);
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    const size_t matAFileSize = tiling::mRows * tiling::n;
    const uint32_t stage2BlockDim = launch_profile::Get(launch_profile::FromEnv()).blockDim;

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tilingData = static_cast<uint8_t *>(AscendC::GmAlloc(tilingDataSize));
    ReadFile("./input/tiling.bin", tilingDataSize, tilingData, tilingDataSize);
    TilingData *tiling = reinterpret_cast<TilingData *>(tilingData);

    uint8_t *matC = static_cast<uint8_t *>(AscendC::GmAlloc(matCFileSize));
    uint8_t *src = static_cast<uint8_t *>(AscendC::GmAlloc(srcFileSize));
    uint8_t *ws = static_cast<uint8_t *>(AscendC::GmAlloc(wsFileSize));
    uint8_t *mmWorkspace = static_cast<uint8_t *>(AscendC::GmAlloc(mmWorkspaceSize));
    uint8_t *cubeTiling = static_cast<uint8_t *>(AscendC::GmAlloc(cubeTilingSize));

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/mat_b_lut.bin", bFileSize, ws + tiling::B_LUT, bFileSize);
    if (!ok) {
        return 10;
    }
    memcpy_s(cubeTiling, cubeTilingSize, cubeTilingBuf, cubeTilingSize);

    g_mix_s12_pass = 1;
    ICPU_RUN_KF(mmad_custom, kStage1BlockDim, matC, src, ws, mmWorkspace, cubeTiling, *tiling);
    ok = WriteFile("./output/s0.bin", ws + tiling::S0, tiling::mRows * tiling::n);
    if (!ok) {
        return 11;
    }

    uint8_t *matA = static_cast<uint8_t *>(AscendC::GmAlloc(matAFileSize));
    uint8_t *matB = static_cast<uint8_t *>(AscendC::GmAlloc(bFileSize));
    memcpy_s(matA, matAFileSize, ws + tiling::S0, matAFileSize);
    memcpy_s(matB, bFileSize, ws + tiling::B_LUT, bFileSize);
    // Stage2 独立 GM + 新 workspace/tiling（勿复用 MIX pass1 的 mmWorkspace，否则 CPU Matmul 挂起）
    uint8_t *mmWorkspace2 = static_cast<uint8_t *>(AscendC::GmAlloc(mmWorkspaceSize));
    uint8_t *cubeTiling2 = static_cast<uint8_t *>(AscendC::GmAlloc(cubeTilingSize));
    memcpy_s(cubeTiling2, cubeTilingSize, cubeTilingBuf, cubeTilingSize);
    ICPU_RUN_KF(stage2_matmul_custom, stage2BlockDim, matA, matB, matC, mmWorkspace2, cubeTiling2);

    ok = WriteFile("./output/mat_c.bin", matC, matCFileSize);
    if (!ok) {
        return 12;
    }

    AscendC::GmFree(matA);
    AscendC::GmFree(matB);
    AscendC::GmFree(mmWorkspace2);
    AscendC::GmFree(cubeTiling2);
    AscendC::GmFree(matC);
    AscendC::GmFree(src);
    AscendC::GmFree(ws);
    AscendC::GmFree(mmWorkspace);
    AscendC::GmFree(cubeTiling);
    AscendC::GmFree(tilingData);
#else
    const size_t matAFileSize = tiling::mRows * tiling::n;
#if defined(S12_HAVE_STAGE2)
    const uint32_t stage2BlockDim = launch_profile::Get(launch_profile::FromEnv()).blockDim;
#endif

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tilingHost;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHost), tilingDataSize));
    ReadFile("./input/tiling.bin", tilingDataSize, tilingHost, tilingDataSize);
    tilingHost->mixPass = 1;

    uint8_t *matCHost;
    uint8_t *matCDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matCHost), matCFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matCDevice), matCFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    uint8_t *srcHost;
    uint8_t *srcDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDevice), srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *wsHost;
    uint8_t *wsDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDevice), wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/mat_b_lut.bin", bFileSize, wsHost + tiling::B_LUT, bFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *mmWsDevice;
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mmWsDevice), mmWorkspaceSize, ACL_MEM_MALLOC_HUGE_FIRST));

    uint8_t *cubeHost;
    uint8_t *cubeDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cubeHost), cubeTilingSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cubeDevice), cubeTilingSize, ACL_MEM_MALLOC_HUGE_FIRST));
    memcpy(cubeHost, cubeTilingBuf, cubeTilingSize);
    CHECK_ACL(aclrtMemcpy(cubeDevice, cubeTilingSize, cubeHost, cubeTilingSize, ACL_MEMCPY_HOST_TO_DEVICE));

#if defined(S12_HAVE_MMAD)
    ACLRT_LAUNCH_KERNEL(mmad_custom)(kStage1BlockDim, stream, matCDevice, srcDevice, wsDevice, mmWsDevice, cubeDevice,
                                     tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(wsHost, wsFileSize, wsDevice, wsFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./input/mat_a_gm.bin", wsHost + tiling::S0, matAFileSize);
#endif

#if defined(S12_HAVE_STAGE2)
    uint8_t *matADevice;
    uint8_t *matBDevice;
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matADevice), matAFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matBDevice), bFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    uint8_t *matAHost;
    size_t matAReadSize = matAFileSize;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matAHost), matAFileSize));
    ok = ReadFile("./input/mat_a_gm.bin", matAReadSize, matAHost, matAFileSize);
    if (!ok) {
        return 11;
    }
    CHECK_ACL(aclrtMemcpy(matADevice, matAFileSize, matAHost, matAFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(matBDevice, bFileSize, wsHost + tiling::B_LUT, bFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtFreeHost(matAHost));

    uint8_t *mmWsDevice2;
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mmWsDevice2), mmWorkspaceSize, ACL_MEM_MALLOC_HUGE_FIRST));
    uint8_t *cubeDevice2;
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cubeDevice2), cubeTilingSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMemcpy(cubeDevice2, cubeTilingSize, cubeHost, cubeTilingSize, ACL_MEMCPY_HOST_TO_DEVICE));

#ifdef S12_STAGE2_INT8_KERNEL
    ACLRT_LAUNCH_KERNEL(int8_matmul_custom)(stage2BlockDim, stream, matADevice, matBDevice, matCDevice, mmWsDevice2,
                                            cubeDevice2);
#else
    ACLRT_LAUNCH_KERNEL(stage2_matmul_custom)(stage2BlockDim, stream, matADevice, matBDevice, matCDevice, mmWsDevice2,
                                              cubeDevice2);
#endif
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(matCHost, matCFileSize, matCDevice, matCFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/mat_c.bin", matCHost, matCFileSize);

    CHECK_ACL(aclrtFree(matADevice));
    CHECK_ACL(aclrtFree(matBDevice));
    CHECK_ACL(aclrtFree(mmWsDevice2));
    CHECK_ACL(aclrtFree(cubeDevice2));
#endif

    CHECK_ACL(aclrtFree(matCDevice));
    CHECK_ACL(aclrtFreeHost(matCHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(mmWsDevice));
    CHECK_ACL(aclrtFree(cubeDevice));
    CHECK_ACL(aclrtFreeHost(cubeHost));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    free(cubeTilingBuf);
    return 0;
}
