/**
 * @file main.cpp
 * @brief RB-T06 host：读 u.bin / v.bin → launch cipher_pack_custom → 写 c.bin。
 *
 * 本文件不做 Compress/Encode；正确性由 scripts/verify_result.py 对拍 golden_c.bin。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_cipher_pack_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void cipher_pack_custom(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm);
#endif

namespace {
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kUBytes = kK * kPolyN * sizeof(int32_t); // 4096
constexpr size_t kVBytes = kPolyN * sizeof(int32_t);      // 1024
constexpr size_t kCBytes = 1568;
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *uBuf = (uint8_t *)AscendC::GmAlloc(kUBytes > 1024 ? kUBytes : 1024);
    uint8_t *vBuf = (uint8_t *)AscendC::GmAlloc(kVBytes > 1024 ? kVBytes : 1024);
    uint8_t *cBuf = (uint8_t *)AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/u.bin", rs, uBuf, kUBytes) || rs != kUBytes) {
        return 1;
    }
    if (!ReadFile("./input/v.bin", rs, vBuf, kVBytes) || rs != kVBytes) {
        return 2;
    }

    ICPU_RUN_KF(cipher_pack_custom, blockDim, uBuf, vBuf, cBuf);

    if (!WriteFile("./output/c.bin", cBuf, kCBytes)) {
        return 3;
    }

    AscendC::GmFree((void *)uBuf);
    AscendC::GmFree((void *)vBuf);
    AscendC::GmFree((void *)cBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *uHost = nullptr, *vHost = nullptr, *cHost = nullptr;
    uint8_t *uDev = nullptr, *vDev = nullptr, *cDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&uHost), kUBytes));
    CHECK_ACL(aclrtMalloc((void **)&uDev, kUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), kVBytes));
    CHECK_ACL(aclrtMalloc((void **)&vDev, kVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCBytes));
    CHECK_ACL(aclrtMalloc((void **)&cDev, kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/u.bin", rs, uHost, kUBytes) || rs != kUBytes) {
        return 1;
    }
    if (!ReadFile("./input/v.bin", rs, vHost, kVBytes) || rs != kVBytes) {
        return 2;
    }
    CHECK_ACL(aclrtMemcpy(uDev, kUBytes, uHost, kUBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDev, kVBytes, vHost, kVBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(cipher_pack_custom)(blockDim, stream, uDev, vDev, cDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(cHost, kCBytes, cDev, kCBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/c.bin", cHost, kCBytes)) {
        return 3;
    }

    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
