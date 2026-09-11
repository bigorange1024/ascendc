/**
 * @file main.cpp
 * @brief RB-D04-decaps-G host：读 m'/h → launch decaps_g_custom → 写 K'/r'。
 *
 * 本文件在流水线中的位置：仅 Host 壳（H2D / launch / mid-sync / D2H）。
 * Host **不**预喂最终 K' / r'（输出缓冲清零后由设备写出）；golden 仅 verify 对拍。
 *
 * h 输入文件为 h.bin：契约上表示 dk_kem[3104:3136) 切片（见 scripts/gen_data.py），
 * 本刀不在 Host 用 H(ek) 重算替代该切片。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_decaps_g_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void decaps_g_custom(GM_ADDR mPrimeGm, GM_ADDR hGm, GM_ADDR kPrimeGm, GM_ADDR rPrimeGm);
#endif

namespace {
constexpr size_t kHalf = 32;
constexpr size_t kMinAlloc = 1024;
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *mBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);
    uint8_t *hBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);
    uint8_t *kBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);
    uint8_t *rBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);

    size_t rs = 0;
    if (!ReadFile("./input/m_prime.bin", rs, mBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/h.bin", rs, hBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    // Host 不预喂 K'/r'：清零后交给设备 UB+DataCopy 写出。
    for (size_t i = 0; i < kHalf; ++i) {
        kBuf[i] = 0;
        rBuf[i] = 0;
    }

    ICPU_RUN_KF(decaps_g_custom, blockDim, mBuf, hBuf, kBuf, rBuf);

    if (!WriteFile("./output/k_prime.bin", kBuf, kHalf)) {
        return 2;
    }
    if (!WriteFile("./output/r_prime.bin", rBuf, kHalf)) {
        return 2;
    }

    AscendC::GmFree((void *)mBuf);
    AscendC::GmFree((void *)hBuf);
    AscendC::GmFree((void *)kBuf);
    AscendC::GmFree((void *)rBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *mHost = nullptr;
    uint8_t *hHost = nullptr;
    uint8_t *kHost = nullptr;
    uint8_t *rHost = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *hDev = nullptr;
    uint8_t *kDev = nullptr;
    uint8_t *rDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&mHost), kHalf));
    CHECK_ACL(aclrtMallocHost((void **)(&hHost), kHalf));
    CHECK_ACL(aclrtMallocHost((void **)(&kHost), kHalf));
    CHECK_ACL(aclrtMallocHost((void **)(&rHost), kHalf));
    CHECK_ACL(aclrtMalloc((void **)&mDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&hDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&kDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/m_prime.bin", rs, mHost, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/h.bin", rs, hHost, kHalf) || rs != kHalf) {
        return 1;
    }
    for (size_t i = 0; i < kHalf; ++i) {
        kHost[i] = 0;
        rHost[i] = 0;
    }

    CHECK_ACL(aclrtMemcpy(mDev, kHalf, mHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHalf, hHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(kDev, kHalf, kHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rDev, kHalf, rHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(decaps_g_custom)(blockDim, stream, mDev, hDev, kDev, rDev);
    // Host mid-sync：stream 同步后 D2H，供 verify / 后续 launch 可见。
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(kHost, kHalf, kDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(rHost, kHalf, rDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/k_prime.bin", kHost, kHalf)) {
        return 2;
    }
    if (!WriteFile("./output/r_prime.bin", rHost, kHalf)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFree(kDev));
    CHECK_ACL(aclrtFree(rDev));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFreeHost(kHost));
    CHECK_ACL(aclrtFreeHost(rHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
