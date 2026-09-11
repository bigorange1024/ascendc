/**
 * @file main.cpp
 * @brief RB-T05 host：input/ek_t_hat.bin → byte_decode12_custom → output/t_hat.bin。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_byte_decode12_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void byte_decode12_custom(GM_ADDR ekTHatGm, GM_ADDR tHatGm);
#endif

namespace {
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kPolyBytes = 384;
constexpr size_t kInBytes = kK * kPolyBytes;
constexpr size_t kOutBytes = kK * kPolyN * sizeof(int32_t);
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *inBuf = (uint8_t *)AscendC::GmAlloc(kInBytes > 1024 ? kInBytes : 1024);
    uint8_t *outBuf = (uint8_t *)AscendC::GmAlloc(kOutBytes > 1024 ? kOutBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/ek_t_hat.bin", rs, inBuf, kInBytes) || rs != kInBytes) {
        return 1;
    }

    ICPU_RUN_KF(byte_decode12_custom, blockDim, inBuf, outBuf);

    if (!WriteFile("./output/t_hat.bin", outBuf, kOutBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)inBuf);
    AscendC::GmFree((void *)outBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *inHost = nullptr;
    uint8_t *outHost = nullptr;
    uint8_t *inDevice = nullptr;
    uint8_t *outDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&inHost), kInBytes));
    CHECK_ACL(aclrtMalloc((void **)&inDevice, kInBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/ek_t_hat.bin", rs, inHost, kInBytes) || rs != kInBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(inDevice, kInBytes, inHost, kInBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(byte_decode12_custom)(blockDim, stream, inDevice, outDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDevice, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/t_hat.bin", outHost, kOutBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(inDevice));
    CHECK_ACL(aclrtFreeHost(inHost));
    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
