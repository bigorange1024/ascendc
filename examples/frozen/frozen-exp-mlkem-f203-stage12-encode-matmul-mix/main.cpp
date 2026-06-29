/**
 * F203 Stage1+2 MIX Host：se + LUT → mat_c
 * CPU 孪生分两趟 launch（encode / matmul），真机单趟 CrossCore。
 */
#include "data_utils.h"
#include "f203_tiling.h"
#include "kernel_tiling/kernel_tiling.h"
#include "launch_profile.h"
#include "tiling/platform/platform_ascendc.h"

#include <cstdlib>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_stage12_mix_custom.h"
#else
#include "tikicpulib.h"
extern "C" void f203_stage12_mix_custom(uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *);
extern volatile int g_f203_host_pass;
#endif

extern void GenerateTiling(const char *socVersion, uint8_t *tilingBuf);

namespace {
constexpr size_t kKPolys = 8;
constexpr size_t kN = 256;
constexpr size_t kRowsA = 16;
constexpr size_t kOutCols = 512;
constexpr size_t kSeBytes = kKPolys * kN * sizeof(int32_t);
constexpr size_t kLutBytes = kN * kOutCols * sizeof(int8_t);
constexpr size_t kMatCBytes = kRowsA * kOutCols * sizeof(int32_t);
} // namespace

static void LoadLutToWs(uint8_t *wsHost, const uint8_t *lutHost)
{
    std::memcpy(wsHost + f203_ws::LUT, lutHost, kLutBytes);
}

static void RunKernel(uint32_t blockDim, uint8_t *matC, uint8_t *se, uint8_t *ws, uint8_t *workspace,
                      uint8_t *tiling)
{
#ifdef ASCENDC_CPU_DEBUG
    ICPU_RUN_KF(f203_stage12_mix_custom, blockDim, matC, se, ws, workspace, tiling);
#else
    (void)blockDim;
    (void)matC;
    (void)se;
    (void)ws;
    (void)workspace;
    (void)tiling;
#endif
}

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const auto launchCfg = launch_profile::Get(launch_profile::FromEnv());
    const uint32_t blockDim = launchCfg.blockDim;
    const char *socVersion = SOC_VERSION;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    const size_t tilingFileSize = sizeof(TCubeTiling) + sizeof(uint64_t);
    const size_t workspaceSize = static_cast<size_t>(ascendcPlatform->GetLibApiWorkSpaceSize());

    uint8_t *tilingBuf = static_cast<uint8_t *>(malloc(tilingFileSize));
    GenerateTiling(socVersion, tilingBuf);
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);

    uint8_t *matCHost = static_cast<uint8_t *>(AscendC::GmAlloc(kMatCBytes));
    uint8_t *seHost = static_cast<uint8_t *>(AscendC::GmAlloc(kSeBytes));
    uint8_t *wsHost = static_cast<uint8_t *>(AscendC::GmAlloc(f203_ws::WSSIZE));
    uint8_t *lutHost = static_cast<uint8_t *>(AscendC::GmAlloc(kLutBytes));
    uint8_t *workspaceHost = static_cast<uint8_t *>(AscendC::GmAlloc(workspaceSize));
    uint8_t *tilingHost = static_cast<uint8_t *>(AscendC::GmAlloc(tilingFileSize));

    size_t seFileSize = kSeBytes;
    size_t lutFileSize = kLutBytes;
    ok = ReadFile("./input/se_polyvec_gm.bin", seFileSize, seHost, kSeBytes);
    if (!ok) {
        return 1;
    }
    ok = ReadFile("./input/mat_b_lut_gm.bin", lutFileSize, lutHost, kLutBytes);
    if (!ok) {
        return 2;
    }
    LoadLutToWs(wsHost, lutHost);
    std::memcpy(tilingHost, tilingBuf, tilingFileSize);

    g_f203_host_pass = 1;
    RunKernel(blockDim, matCHost, seHost, wsHost, workspaceHost, tilingHost);

    g_f203_host_pass = 2;
    RunKernel(blockDim, matCHost, seHost, wsHost, workspaceHost, tilingHost);

    ok = WriteFile("./output/mat_c_gm.bin", matCHost, kMatCBytes);
    if (!ok) {
        return 3;
    }
    if (std::getenv("F203_DEBUG_WS") != nullptr) {
        WriteFile("./output/debug_mat_a.bin", wsHost + f203_ws::MAT_A, kRowsA * kN);
    }

    AscendC::GmFree(matCHost);
    AscendC::GmFree(seHost);
    AscendC::GmFree(wsHost);
    AscendC::GmFree(lutHost);
    AscendC::GmFree(workspaceHost);
    AscendC::GmFree(tilingHost);
#else
    CHECK_ACL(aclInit(nullptr));
    const int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *matCHost = nullptr;
    uint8_t *matCDev = nullptr;
    uint8_t *seHost = nullptr;
    uint8_t *seDev = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *lutHost = nullptr;
    uint8_t *workspaceDev = nullptr;
    uint8_t *tilingHost = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matCHost), kMatCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matCDev), kMatCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seHost), kSeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seDev), kSeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), f203_ws::WSSIZE));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), f203_ws::WSSIZE, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&lutHost), kLutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&workspaceDev), workspaceSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHost), tilingFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), tilingFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(tilingHost, tilingBuf, tilingFileSize);

    size_t seFileSize = kSeBytes;
    size_t lutFileSize = kLutBytes;
    ok = ReadFile("./input/se_polyvec_gm.bin", seFileSize, seHost, kSeBytes);
    if (!ok) {
        return 1;
    }
    ok = ReadFile("./input/mat_b_lut_gm.bin", lutFileSize, lutHost, kLutBytes);
    if (!ok) {
        return 2;
    }
    LoadLutToWs(wsHost, lutHost);

    CHECK_ACL(aclrtMemcpy(seDev, kSeBytes, seHost, kSeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, f203_ws::WSSIZE, wsHost, f203_ws::WSSIZE, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, tilingFileSize, tilingHost, tilingFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(f203_stage12_mix_custom)(blockDim, stream, matCDev, seDev, wsDev, workspaceDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(matCHost, kMatCBytes, matCDev, kMatCBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/mat_c_gm.bin", matCHost, kMatCBytes);
    if (!ok) {
        return 3;
    }

    CHECK_ACL(aclrtFree(matCDev));
    CHECK_ACL(aclrtFree(seDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(workspaceDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(matCHost));
    CHECK_ACL(aclrtFreeHost(seHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(lutHost));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    free(tilingBuf);
    return 0;
}
