/*
 * Host 驱动：SEED_D → Device Phase G+P+C → output/src.bin
 * 默认编译为 F203_SE_VECTOR_V3（见 f203_se_stage_config.hpp）。
 */
#include "data_utils.h"
#include "f203_se_stage_config.hpp"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_se_vector_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *x_gm, uint8_t *lengths_gm,
                                     uint8_t *workspace, uint8_t *tiling);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_se_vector_k4(GM_ADDR seed_d_gm, GM_ADDR prf_out_gm, GM_ADDR src_gm,
                                                        GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR workspace,
                                                        GM_ADDR tiling);
#endif

namespace {
constexpr uint32_t kBlockDim = 1U;
constexpr uint32_t kBatch = 8U;
/** 与 f203_se_vector_prf.hpp PRF_MSG_STRIDE 一致（8B 对齐 UB 行距；有效消息长 33）。 */
constexpr uint32_t kMaxMsgLen = 64U;
constexpr uint32_t kOutLen = 128U;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kBatch) * kOutLen;
constexpr size_t kSrcBytes = 8U * 256U * sizeof(int32_t);
constexpr size_t kXBytes = static_cast<size_t>(kBatch) * kMaxMsgLen;
constexpr size_t kLenBytes = static_cast<size_t>(kBatch) * sizeof(uint32_t);
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);
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

    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, kBatch, kMaxMsgLen, kOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kBlockDim;

    std::cout << "[main] f203_se_vector_k4 stage=" << F203_SE_STAGE_LABEL << " SEED_D=" << seed_d
              << " blockDim=" << kBlockDim << " batch=" << kBatch << " rate=" << tilingHost.rate
              << " groupSize=" << tilingHost.groupSize << "\n";

#ifdef __CCE_KT_TEST__
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *xGm = static_cast<uint8_t *>(AscendC::GmAlloc(kXBytes));
    uint8_t *lenGm = static_cast<uint8_t *>(AscendC::GmAlloc(kLenBytes));
    uint8_t *workspace = static_cast<uint8_t *>(AscendC::GmAlloc(64));
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(kTilingBytes));

    std::memcpy(seedGm, &seed_d, kSeedBytes);
    std::memcpy(tiling, &tilingHost, kTilingBytes);

    ICPU_RUN_KF(f203_se_vector_k4, kBlockDim, seedGm, prfGm, srcGm, xGm, lenGm, workspace, tiling);

    if (!WriteFile("./output/prf_out.bin", prfGm, kPrfBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcGm, kSrcBytes)) {
        return 3;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(xGm);
    AscendC::GmFree(lenGm);
    AscendC::GmFree(workspace);
    AscendC::GmFree(tiling);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *prfHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *tilingDev = nullptr;
    uint8_t *tilingHostBuf = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prfHost), kPrfBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), kTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), 64, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_se_vector_k4_do(kBlockDim, nullptr, stream, seedDev, prfDev, srcDev, xDev, lenDev, wsDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(prfHost, kPrfBytes, prfDev, kPrfBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(srcHost, kSrcBytes, srcDev, kSrcBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/prf_out.bin", prfHost, kPrfBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcHost, kSrcBytes)) {
        return 3;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(prfHost));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
