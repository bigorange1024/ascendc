/**
 * @file main.cpp
 * @brief Host：SEED_D → 设备 Alg.13 行 3–7（16× SampleNTT 向量路径）→ output/a_hat.bin。
 */
#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_a_hat16_layout.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_alg13_a_hat_16poly_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                           uint8_t *a_hat_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_alg13_a_hat_16poly(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm);
#endif

namespace {
constexpr uint32_t kBlockDim = F203_AHAT16_BLOCK_DIM;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kAHatOutBytes = F203Ahat16::kAHatBytes;
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

    std::cout << "[main] f203_alg13_a_hat_16poly SEED_D=" << seed_d << " blockDim=" << kBlockDim
              << " F203_ALG7_REJ_IMPL=" << F203_ALG7_REJ_IMPL << " F203_AHAT16_BATCH_SHAKE=" << F203_AHAT16_BATCH_SHAKE
              << " F203_ALG7_XOF_504=" << F203_ALG7_XOF_504 << "\n";

#ifdef __CCE_KT_TEST__
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatOutBytes));

    std::memcpy(seedGm, &seed_d, kSeedBytes);

    ICPU_RUN_KF(f203_alg13_a_hat_16poly, kBlockDim, seedGm, aHatGm);

    if (!WriteFile("./output/a_hat.bin", aHatGm, kAHatOutBytes)) {
        return 2;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(aHatGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *aHatDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_alg13_a_hat_16poly_do(kBlockDim, nullptr, stream, seedDev, aHatDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatOutBytes, aHatDev, kAHatOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatOutBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
