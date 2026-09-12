/**
 * @file main.cpp
 * @brief AV01 host：input/{src,roots,indices}.bin → single_aiv_mlkem_ntt → output/dst.bin。
 *
 * CPU：ICPU_RUN_KF + AIV_MODE；SIM：ACLRT_LAUNCH_KERNEL。本刀禁 -r npu。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_single_aiv_mlkem_ntt.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void single_aiv_mlkem_ntt(GM_ADDR input, GM_ADDR output, GM_ADDR roots, GM_ADDR indices);
#endif

namespace {
constexpr size_t kN = 256;
constexpr size_t kRootWords = 1024;
constexpr size_t kMapWords = 7 * 256;
constexpr size_t kPolyBytes = kN * sizeof(int32_t);
constexpr size_t kRootBytes = kRootWords * sizeof(int32_t);
constexpr size_t kMapBytes = kMapWords * sizeof(uint32_t);
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *inBuf = (uint8_t *)AscendC::GmAlloc(kPolyBytes > 1024 ? kPolyBytes : 1024);
    uint8_t *outBuf = (uint8_t *)AscendC::GmAlloc(kPolyBytes > 1024 ? kPolyBytes : 1024);
    uint8_t *rootBuf = (uint8_t *)AscendC::GmAlloc(kRootBytes > 1024 ? kRootBytes : 1024);
    uint8_t *mapBuf = (uint8_t *)AscendC::GmAlloc(kMapBytes > 1024 ? kMapBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/src.bin", rs, inBuf, kPolyBytes) || rs != kPolyBytes) {
        return 1;
    }
    if (!ReadFile("./input/roots.bin", rs, rootBuf, kRootBytes) || rs != kRootBytes) {
        return 2;
    }
    if (!ReadFile("./input/indices.bin", rs, mapBuf, kMapBytes) || rs != kMapBytes) {
        return 3;
    }

    ICPU_RUN_KF(single_aiv_mlkem_ntt, blockDim, inBuf, outBuf, rootBuf, mapBuf);

    if (!WriteFile("./output/dst.bin", outBuf, kPolyBytes)) {
        return 4;
    }
    AscendC::GmFree((void *)inBuf);
    AscendC::GmFree((void *)outBuf);
    AscendC::GmFree((void *)rootBuf);
    AscendC::GmFree((void *)mapBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *inHost = nullptr;
    uint8_t *outHost = nullptr;
    uint8_t *rootHost = nullptr;
    uint8_t *mapHost = nullptr;
    uint8_t *inDevice = nullptr;
    uint8_t *outDevice = nullptr;
    uint8_t *rootDevice = nullptr;
    uint8_t *mapDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&inHost), kPolyBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&outHost), kPolyBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&rootHost), kRootBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&mapHost), kMapBytes));
    CHECK_ACL(aclrtMalloc((void **)&inDevice, kPolyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, kPolyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rootDevice, kRootBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mapDevice, kMapBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/src.bin", rs, inHost, kPolyBytes) || rs != kPolyBytes) {
        return 1;
    }
    if (!ReadFile("./input/roots.bin", rs, rootHost, kRootBytes) || rs != kRootBytes) {
        return 2;
    }
    if (!ReadFile("./input/indices.bin", rs, mapHost, kMapBytes) || rs != kMapBytes) {
        return 3;
    }
    CHECK_ACL(aclrtMemcpy(inDevice, kPolyBytes, inHost, kPolyBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rootDevice, kRootBytes, rootHost, kRootBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mapDevice, kMapBytes, mapHost, kMapBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(single_aiv_mlkem_ntt)
    (blockDim, stream, inDevice, outDevice, rootDevice, mapDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, kPolyBytes, outDevice, kPolyBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/dst.bin", outHost, kPolyBytes)) {
        return 4;
    }

    CHECK_ACL(aclrtFree(inDevice));
    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFree(rootDevice));
    CHECK_ACL(aclrtFree(mapDevice));
    CHECK_ACL(aclrtFreeHost(inHost));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFreeHost(rootHost));
    CHECK_ACL(aclrtFreeHost(mapHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
