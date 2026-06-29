/**
 * @file main.cpp
 * F203 Stage3 Host 入口：mat_c_gm → out_gm。
 * Launch：LAUNCH_PROFILE=aiv=1|2|8（run.sh --aiv，见 launch_profile.h）。
 */
#include "data_utils.h"
#include "launch_profile.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
extern void f203_stage3_routea_mod_do(uint32_t blockDim, void *stream, uint8_t *matC, uint8_t *out);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_stage3_routea_mod_custom(GM_ADDR matC, GM_ADDR out);
#endif

namespace {
constexpr size_t kKPolys = 8;
constexpr size_t kN = 256;
constexpr size_t kOutCols = 512;
constexpr size_t kRowsC = 16;
constexpr size_t kMatCBytes = kRowsC * kOutCols * sizeof(int32_t);
constexpr size_t kOutBytes = kKPolys * kN * sizeof(int32_t);
} // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const launch_profile::Config launchCfg = launch_profile::Get(launch_profile::FromEnv());
    const uint32_t blockDim = launchCfg.blockDim;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *matC = static_cast<uint8_t *>(AscendC::GmAlloc(kMatCBytes));
    uint8_t *out = static_cast<uint8_t *>(AscendC::GmAlloc(kOutBytes));

    size_t matCFileSize = kMatCBytes;
    ReadFile("./input/mat_c_gm.bin", matCFileSize, matC, kMatCBytes);
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_stage3_routea_mod_custom, blockDim, matC, out);
    WriteFile("./output/out_gm.bin", out, kOutBytes);

    AscendC::GmFree(matC);
    AscendC::GmFree(out);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *matCHost;
    uint8_t *matCDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matCHost), kMatCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matCDevice), kMatCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t matCFileSize = kMatCBytes;
    ReadFile("./input/mat_c_gm.bin", matCFileSize, matCHost, kMatCBytes);
    CHECK_ACL(aclrtMemcpy(matCDevice, kMatCBytes, matCHost, kMatCBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *outHost;
    uint8_t *outDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDevice), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    f203_stage3_routea_mod_do(blockDim, stream, matCDevice, outDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDevice, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/out_gm.bin", outHost, kOutBytes);

    CHECK_ACL(aclrtFree(matCDevice));
    CHECK_ACL(aclrtFreeHost(matCHost));
    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
