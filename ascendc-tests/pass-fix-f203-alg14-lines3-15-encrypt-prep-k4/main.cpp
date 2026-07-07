/**
 * @file main.cpp
 * @brief Host：ek_pke + coins → 设备 Alg.14 prep（Â + r/e₁/e₂）→ a_hat.bin + re.bin。
 *
 * 验收：CPU + SIM 双模式；对拍 golden_a_hat / golden_re（max_abs_diff=0）。
 * 探针：pass-fix-f203-alg14-lines3-15-encrypt-prep-k4（2026-07-07 晋级 pass-）。
 */
#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_layout.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_encrypt_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                     uint8_t *coins_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *re_gm,
                                     uint8_t *tiling_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_encrypt_prep(GM_ADDR ek_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm,
                                                        GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm);
#endif

namespace {
using namespace F203EncryptPrep;

constexpr uint32_t kBlockDim = kPrepBlockDim;
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);

void FillPrfTiling(ShakeGeneralTilingData *t)
{
    // maxMsgLen 须与 f203_se_vector_prf.hpp PRF_MSG_STRIDE(64) 一致，非有效消息长 33
    FillShakeTiling(t, 8U, 64U, kPrfBytesPerPoly, SHAKE256_RATE_BYTES);
    t->blockDim = 1U;
}
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t rs = 0;

    ShakeGeneralTilingData tilingHost{};
    FillPrfTiling(&tilingHost);

    std::cout << "[main] f203_encrypt_prep blockDim=" << kBlockDim << " ek=" << kEkBytes << " coins=" << kCoinsSize
              << " a_hat=" << kAHatBytes << " re=" << kReBytes << "\n";

#ifdef __CCE_KT_TEST__
    uint8_t *ekGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkBytes));
    uint8_t *coinsGm = static_cast<uint8_t *>(AscendC::GmAlloc(kCoinsSize));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *reGm = static_cast<uint8_t *>(AscendC::GmAlloc(kReBytes));
    uint8_t *tilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kTilingBytes));

    ReadFile("./input/ek_pke.bin", rs, ekGm, kEkBytes);
    if (rs != kEkBytes) {
        std::cerr << "[FAIL] ek_pke.bin size\n";
        return 1;
    }
    ReadFile("./input/coins.bin", rs, coinsGm, kCoinsSize);
    if (rs != kCoinsSize) {
        std::cerr << "[FAIL] coins.bin size\n";
        return 1;
    }
    std::memcpy(tilingGm, &tilingHost, kTilingBytes);

    ICPU_RUN_KF(f203_encrypt_prep, kBlockDim, ekGm, coinsGm, aHatGm, prfGm, reGm, tilingGm);

    if (!WriteFile("./output/a_hat.bin", aHatGm, kAHatBytes) || !WriteFile("./output/re.bin", reGm, kReBytes)) {
        return 2;
    }

    AscendC::GmFree(ekGm);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(tilingGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekHost = nullptr;
    uint8_t *coinsHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *reHost = nullptr;
    uint8_t *tilingHostBuf = nullptr;
    uint8_t *ekDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&coinsHost), kCoinsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&reHost), kReBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), kTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&coinsDev), kCoinsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&reDev), kReBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    ReadFile("./input/ek_pke.bin", rs, ekHost, kEkBytes);
    if (rs != kEkBytes) {
        std::cerr << "[FAIL] ek_pke.bin size\n";
        return 1;
    }
    ReadFile("./input/coins.bin", rs, coinsHost, kCoinsSize);
    if (rs != kCoinsSize) {
        std::cerr << "[FAIL] coins.bin size\n";
        return 1;
    }
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);

    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(coinsDev, kCoinsSize, coinsHost, kCoinsSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_encrypt_prep_do(kBlockDim, nullptr, stream, ekDev, coinsDev, aHatDev, prfDev, reDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(reHost, kReBytes, reDev, kReBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes) || !WriteFile("./output/re.bin", reHost, kReBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFreeHost(coinsHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(reHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
