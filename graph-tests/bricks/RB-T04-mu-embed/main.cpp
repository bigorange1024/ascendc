/**
 * @file main.cpp
 * @brief RB-T04 host：读 input/m.bin → launch mu_embed_custom → 写 output/mu.bin。
 *
 * 本文件不做 μ 计算；正确性由 scripts/verify_result.py 对拍 golden_mu.bin。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mu_embed_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mu_embed_custom(GM_ADDR mGm, GM_ADDR muGm);
#endif

namespace {
constexpr size_t kMsgBytes = 32;
constexpr size_t kPolyN = 256;
constexpr size_t kMuBytes = kPolyN * sizeof(int32_t);
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *mBuf = (uint8_t *)AscendC::GmAlloc(kMsgBytes > 1024 ? kMsgBytes : 1024);
    uint8_t *muBuf = (uint8_t *)AscendC::GmAlloc(kMuBytes > 1024 ? kMuBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mBuf, kMsgBytes) || rs != kMsgBytes) {
        return 1;
    }

    ICPU_RUN_KF(mu_embed_custom, blockDim, mBuf, muBuf);

    if (!WriteFile("./output/mu.bin", muBuf, kMuBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)mBuf);
    AscendC::GmFree((void *)muBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *mHost = nullptr;
    uint8_t *muHost = nullptr;
    uint8_t *mDevice = nullptr;
    uint8_t *muDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&mHost), kMsgBytes));
    CHECK_ACL(aclrtMalloc((void **)&mDevice, kMsgBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&muHost), kMuBytes));
    CHECK_ACL(aclrtMalloc((void **)&muDevice, kMuBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mHost, kMsgBytes) || rs != kMsgBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(mDevice, kMsgBytes, mHost, kMsgBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(mu_embed_custom)(blockDim, stream, mDevice, muDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(muHost, kMuBytes, muDevice, kMuBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/mu.bin", muHost, kMuBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(mDevice));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFree(muDevice));
    CHECK_ACL(aclrtFreeHost(muHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
