/*
 * Host 驱动：SEED_D → Device Alg.13 行 8–15 → output/src.bin
 */
#include "data_utils.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_se_device_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *src_gm, uint8_t *workspace, uint8_t *tiling);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_se_device_k4(GM_ADDR seed_d_gm, GM_ADDR src_gm, GM_ADDR workspace,
                                                       GM_ADDR tiling);
#endif

namespace {
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kSrcBytes = 8U * 256U * sizeof(int32_t);
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t seed_d = 20260619U;
    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return 1;
    }

    std::cout << "[main] f203_se_device_k4 SEED_D=" << seed_d << " blockDim=" << kBlockDim << "\n";

#ifdef __CCE_KT_TEST__
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *workspace = static_cast<uint8_t *>(AscendC::GmAlloc(64));
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(64));

    std::memcpy(seedGm, &seed_d, kSeedBytes);

    ICPU_RUN_KF(f203_se_device_k4, kBlockDim, seedGm, srcGm, workspace, tiling);

    if (!WriteFile("./output/src.bin", srcGm, kSrcBytes)) {
        return 2;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(workspace);
    AscendC::GmFree(tiling);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), 64, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), 64, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_se_device_k4_do(kBlockDim, nullptr, stream, seedDev, srcDev, wsDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(srcHost, kSrcBytes, srcDev, kSrcBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/src.bin", srcHost, kSrcBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
