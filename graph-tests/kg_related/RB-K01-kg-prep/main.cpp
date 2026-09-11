/**
 * @file main.cpp
 * @brief RB-K01 host：seed_d → kg_prep_custom → Â / ŝ / ê bin。
 *
 * 本文件在流水线中的位置：仅 Host 壳（H2D/D2H + launch + tiling 填充）；算法在设备核。
 * 对齐 KGR-P01：BLOCK_DIM=1；AIV_MODE（CPU 孪生）；PRF tiling 与 lines8-15 同契约。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_kg_prep_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void kg_prep_custom(GM_ADDR seedDGm, GM_ADDR aHatGm, GM_ADDR sHatGm, GM_ADDR eGm, GM_ADDR prfWsGm,
                               GM_ADDR srcWsGm, GM_ADDR xWsGm, GM_ADDR lenWsGm, GM_ADDR tilingGm);
#endif

namespace {
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kAHatPolys = kK * kK;
constexpr size_t kSrcRows = 2 * kK;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kAHatBytes = kAHatPolys * kPolyN * sizeof(int32_t);
constexpr size_t kSHatBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kEBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kPrfBytes = kSrcRows * 128U;  // 8×128
constexpr size_t kSrcBytes = kSrcRows * kPolyN * sizeof(int32_t);
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr size_t kMinAlloc = 1024;
/** 与 lines8-15：PRF 有效 33B，UB 行 stride 垫到 64；rate=SHAKE256。 */
constexpr uint32_t kPrfBatch = 8U;
constexpr uint32_t kPrfMaxMsgLen = 64U;
constexpr uint32_t kPrfOutLen = 128U;
constexpr size_t kXBytes = static_cast<size_t>(kPrfBatch) * kPrfMaxMsgLen;
constexpr size_t kLenBytes = static_cast<size_t>(kPrfBatch) * sizeof(uint32_t);
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = kBlockDim;

    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kBlockDim;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *seedBuf = (uint8_t *)AscendC::GmAlloc(kSeedBytes > kMinAlloc ? kSeedBytes : kMinAlloc);
    uint8_t *aHatBuf = (uint8_t *)AscendC::GmAlloc(kAHatBytes);
    uint8_t *sHatBuf = (uint8_t *)AscendC::GmAlloc(kSHatBytes);
    uint8_t *eBuf = (uint8_t *)AscendC::GmAlloc(kEBytes);
    uint8_t *prfBuf = (uint8_t *)AscendC::GmAlloc(kPrfBytes);
    uint8_t *srcBuf = (uint8_t *)AscendC::GmAlloc(kSrcBytes);
    uint8_t *xBuf = (uint8_t *)AscendC::GmAlloc(kXBytes > kMinAlloc ? kXBytes : kMinAlloc);
    uint8_t *lenBuf = (uint8_t *)AscendC::GmAlloc(kLenBytes > kMinAlloc ? kLenBytes : kMinAlloc);
    uint8_t *tilingBuf = (uint8_t *)AscendC::GmAlloc(kTilingBytes > kMinAlloc ? kTilingBytes : kMinAlloc);

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedBuf, kSeedBytes) || rs != kSeedBytes) {
        return 1;
    }
    std::memcpy(tilingBuf, &tilingHost, kTilingBytes);

    ICPU_RUN_KF(kg_prep_custom, blockDim, seedBuf, aHatBuf, sHatBuf, eBuf, prfBuf, srcBuf, xBuf, lenBuf, tilingBuf);

    if (!WriteFile("./output/a_hat.bin", aHatBuf, kAHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/s_hat.bin", sHatBuf, kSHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/e.bin", eBuf, kEBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcBuf, kSrcBytes)) {
        return 2;
    }
    if (!WriteFile("./output/prf_out.bin", prfBuf, kPrfBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)seedBuf);
    AscendC::GmFree((void *)aHatBuf);
    AscendC::GmFree((void *)sHatBuf);
    AscendC::GmFree((void *)eBuf);
    AscendC::GmFree((void *)prfBuf);
    AscendC::GmFree((void *)srcBuf);
    AscendC::GmFree((void *)xBuf);
    AscendC::GmFree((void *)lenBuf);
    AscendC::GmFree((void *)tilingBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *sHatHost = nullptr;
    uint8_t *eHost = nullptr;
    uint8_t *prfHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *xHost = nullptr;
    uint8_t *lenHost = nullptr;
    uint8_t *tilingHostBuf = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *sHatDev = nullptr;
    uint8_t *eDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&sHatHost), kSHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&eHost), kEBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&prfHost), kPrfBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), kXBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&lenHost), kLenBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&tilingHostBuf), kTilingBytes));
    CHECK_ACL(aclrtMalloc((void **)&seedDev, kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sHatDev, kSHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&eDev, kEBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&srcDev, kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&xDev, kXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&lenDev, kLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&tilingDev, kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedHost, kSeedBytes) || rs != kSeedBytes) {
        return 1;
    }
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(kg_prep_custom)
    (blockDim, stream, seedDev, aHatDev, sHatDev, eDev, prfDev, srcDev, xDev, lenDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(sHatHost, kSHatBytes, sHatDev, kSHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(eHost, kEBytes, eDev, kEBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(srcHost, kSrcBytes, srcDev, kSrcBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(prfHost, kPrfBytes, prfDev, kPrfBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/s_hat.bin", sHatHost, kSHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/e.bin", eHost, kEBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcHost, kSrcBytes)) {
        return 2;
    }
    if (!WriteFile("./output/prf_out.bin", prfHost, kPrfBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFree(eDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFreeHost(eHost));
    CHECK_ACL(aclrtFreeHost(prfHost));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(xHost));
    CHECK_ACL(aclrtFreeHost(lenHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
