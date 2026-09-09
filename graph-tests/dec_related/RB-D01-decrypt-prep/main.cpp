/**
 * @file main.cpp
 * @brief RB-D01 host：dk_pke + c → dec_prep_custom → ŝ/u/v bin。
 *
 * 本文件在流水线中的位置：仅 Host 壳（H2D/D2H + launch）；算法在设备核。
 * 对齐 DRW-D01：BLOCK_DIM=1；AIV_MODE（CPU 孪生）。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_dec_prep_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void dec_prep_custom(GM_ADDR dkPkeGm, GM_ADDR cGm, GM_ADDR sHatGm, GM_ADDR uGm, GM_ADDR vGm);
#endif

namespace {
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kDkBytes = 1536;
constexpr size_t kCBytes = 1568;
constexpr size_t kSHatBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kUBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kVBytes = kPolyN * sizeof(int32_t);
constexpr size_t kMinAlloc = 1024;
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *dkBuf = (uint8_t *)AscendC::GmAlloc(kDkBytes > kMinAlloc ? kDkBytes : kMinAlloc);
    uint8_t *cBuf = (uint8_t *)AscendC::GmAlloc(kCBytes > kMinAlloc ? kCBytes : kMinAlloc);
    uint8_t *sHatBuf = (uint8_t *)AscendC::GmAlloc(kSHatBytes > kMinAlloc ? kSHatBytes : kMinAlloc);
    uint8_t *uBuf = (uint8_t *)AscendC::GmAlloc(kUBytes > kMinAlloc ? kUBytes : kMinAlloc);
    uint8_t *vBuf = (uint8_t *)AscendC::GmAlloc(kVBytes > kMinAlloc ? kVBytes : kMinAlloc);

    size_t rs = 0;
    if (!ReadFile("./input/dk_pke.bin", rs, dkBuf, kDkBytes) || rs != kDkBytes) {
        return 1;
    }
    if (!ReadFile("./input/c.bin", rs, cBuf, kCBytes) || rs != kCBytes) {
        return 1;
    }

    ICPU_RUN_KF(dec_prep_custom, blockDim, dkBuf, cBuf, sHatBuf, uBuf, vBuf);

    if (!WriteFile("./output/s_hat.bin", sHatBuf, kSHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/u.bin", uBuf, kUBytes)) {
        return 2;
    }
    if (!WriteFile("./output/v.bin", vBuf, kVBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)dkBuf);
    AscendC::GmFree((void *)cBuf);
    AscendC::GmFree((void *)sHatBuf);
    AscendC::GmFree((void *)uBuf);
    AscendC::GmFree((void *)vBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dkHost = nullptr;
    uint8_t *cHost = nullptr;
    uint8_t *sHatHost = nullptr;
    uint8_t *uHost = nullptr;
    uint8_t *vHost = nullptr;
    uint8_t *dkDev = nullptr;
    uint8_t *cDev = nullptr;
    uint8_t *sHatDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&dkHost), kDkBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&sHatHost), kSHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&uHost), kUBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), kVBytes));
    CHECK_ACL(aclrtMalloc((void **)&dkDev, kDkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDev, kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sHatDev, kSHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, kUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, kVBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/dk_pke.bin", rs, dkHost, kDkBytes) || rs != kDkBytes) {
        return 1;
    }
    if (!ReadFile("./input/c.bin", rs, cHost, kCBytes) || rs != kCBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(dkDev, kDkBytes, dkHost, kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, kCBytes, cHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(dec_prep_custom)(blockDim, stream, dkDev, cDev, sHatDev, uDev, vDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(sHatHost, kSHatBytes, sHatDev, kSHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHost, kUBytes, uDev, kUBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, kVBytes, vDev, kVBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/s_hat.bin", sHatHost, kSHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/u.bin", uHost, kUBytes)) {
        return 2;
    }
    if (!WriteFile("./output/v.bin", vHost, kVBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
