/**
 * Phase A Host：se → MIX encode(FSM) → mat_a；sync 标记在 ws[0]
 */
#include "data_utils.h"
#include "phase_a_tiling.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_phase_a_fsm_custom.h"
#include "tiling/platform/platform_ascendc.h"
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_phase_a_fsm_custom(GM_ADDR se, GM_ADDR matA, GM_ADDR ws, TilingData tiling);
#endif

namespace {
constexpr size_t kSeBytes = 8 * 256 * sizeof(int32_t);
constexpr size_t kMatABytes = 16 * 256;
constexpr size_t kSyncBytes = sizeof(int32_t);
constexpr size_t kTilingBytes = sizeof(TilingData);
} // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const uint32_t blockDim = 1;
    TilingData tiling{};
    tiling.tileLength = 256;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *se = static_cast<uint8_t *>(AscendC::GmAlloc(kSeBytes));
    uint8_t *matA = static_cast<uint8_t *>(AscendC::GmAlloc(kMatABytes));
    uint8_t *ws = static_cast<uint8_t *>(AscendC::GmAlloc(phase_a_tiling::kWsBytes));
    size_t seFileSize = kSeBytes;
    ReadFile("./input/se_polyvec_gm.bin", seFileSize, se, kSeBytes);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_phase_a_fsm_custom, blockDim, se, matA, ws, tiling);
    AscendC::SetKernelMode(KernelMode::AIC_MODE);
    ICPU_RUN_KF(f203_phase_a_fsm_custom, blockDim, se, matA, ws, tiling);
    WriteFile("./output/mat_a_gm.bin", matA, kMatABytes);
    WriteFile("./output/sync.bin", ws, kSyncBytes);
    AscendC::GmFree(se);
    AscendC::GmFree(matA);
    AscendC::GmFree(ws);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(SOC_VERSION);
    const size_t kWsBytes = static_cast<size_t>(ascendcPlatform->GetLibApiWorkSpaceSize());

    uint8_t *seHost;
    uint8_t *seDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seHost), kSeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seDevice), kSeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t seFileSize = kSeBytes;
    ReadFile("./input/se_polyvec_gm.bin", seFileSize, seHost, kSeBytes);
    CHECK_ACL(aclrtMemcpy(seDevice, kSeBytes, seHost, kSeBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *matAHost;
    uint8_t *matADevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matAHost), kMatABytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matADevice), kMatABytes, ACL_MEM_MALLOC_HUGE_FIRST));

    uint8_t *wsHost;
    uint8_t *wsDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), kWsBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDevice), kWsBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    TilingData *tilingHost;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHost), kTilingBytes));

    *tilingHost = tiling;

    ACLRT_LAUNCH_KERNEL(f203_phase_a_fsm_custom)(blockDim, stream, seDevice, matADevice, wsDevice, tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(matAHost, kMatABytes, matADevice, kMatABytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, kWsBytes, wsDevice, kWsBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/mat_a_gm.bin", matAHost, kMatABytes);
    WriteFile("./output/sync.bin", wsHost, kSyncBytes);

    CHECK_ACL(aclrtFree(seDevice));
    CHECK_ACL(aclrtFreeHost(seHost));
    CHECK_ACL(aclrtFree(matADevice));
    CHECK_ACL(aclrtFreeHost(matAHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
