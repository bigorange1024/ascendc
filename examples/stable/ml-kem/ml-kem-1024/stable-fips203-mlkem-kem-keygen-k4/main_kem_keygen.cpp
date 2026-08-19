/**
 * @file main_kem_keygen.cpp
 * @brief FIPS 203 Alg.19 ML-KEM.KeyGen host（incubating：2 launch 全链）。
 *
 * 规格：本目录 exp-…-实现方案-customspec；踩坑见 SyncAll / CPU AIV1 做尾。
 *
 * 设备全链（2 launch，Alg.19→Alg.16→Alg.13 同 session）：
 *   L1 f203_keygen_prep  — vendored Alg.13 行 3–15
 *   L2 mmad_custom       — vendored Alg.13 行 16–21 + 内嵌 Alg.16 尾（F203_KEM_KEYGEN_TAIL=1）
 *
 * 生产 I/O：input/seed_d.bin(4B)+LUT → output/ek_kem.bin(1568)+dk_kem.bin(3168)。
 * 禁止：把 correctness / device 探针当 CMake 依赖；仅 I/O 对拍。
 */
#include "data_utils.h"
#include "f203_keygen_layout.h"
#include "f203_keygen_prep_layout.h"
#include "f203_kem_kg_layout.h"
#include "shake_general_tiling_data.h"
#include "tiling.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
#include "aclrtlaunch_mmad_custom.h"
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm);
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling,
                                                  GM_ADDR rho_gm, GM_ADDR ek_pke_gm, GM_ADDR seed_d_gm, GM_ADDR dk_kem_gm);
extern volatile int g_2s1e_mix_pass;
#endif

namespace {
using namespace F203KeygenPrep;

constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr uint32_t kMmadBlockDim = 1U;

void FillVecTiling(TilingData *t)
{
    std::memset(t, 0, sizeof(TilingData));
    t->tileLength = 256;
    t->mixPass = 0;
}

}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t seed_d = 0U;
    size_t rs = 0;
#if KEM_KG_EXT_SEED
    uint8_t extSeed[F203KemKg::kExtSeedBytes];
    rs = sizeof(extSeed);
    if (!ReadFile("./input/kem_seed.bin", rs, extSeed, sizeof(extSeed)) || rs != sizeof(extSeed)) {
        std::cerr << "[FAIL] read input/kem_seed.bin (KEM_KG_EXT_SEED)\n";
        return 1;
    }
    const size_t kSeedGmBytes = F203KemKg::kExtSeedBytes;
    const void *seedSrc = extSeed;
#else
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return 1;
    }
    const size_t kSeedGmBytes = kSeedBytes;
    const void *seedSrc = &seed_d;
#endif

    ShakeGeneralTilingData shakeTilingHost{};
    FillShakeTiling(&shakeTilingHost, kSeBatch, kSeMaxMsgLen, kSePrfOutLen, SHAKE256_RATE_BYTES);
    shakeTilingHost.blockDim = kPrepBlockDim;

    TilingData vecTilingHost{};
    FillVecTiling(&vecTilingHost);

    const size_t dstFileSize = tiling::dstFileBytes;
    const size_t tHatFileSize = tiling::tHatFileBytes;
    const size_t ekPolyBytes = byte_encode::polyVecBytes;
    const size_t ekPkeBytes = F203Keygen::kEkPkeBytes;
    const size_t skFileSize = byte_encode::polyVecBytes;
    const size_t lutFileSize = tiling::lutEvenOddFileBytes;
    const size_t wsFileSize = tiling::wssize;

    std::cout << "[main] kem_keygen device-k4 SEED_D=" << seed_d << " prep_blockDim=" << kPrepBlockDim
              << " mmad_blockDim=" << kMmadBlockDim << " launches=2 (prep | compute+kem_tail)\n";

#ifdef __CCE_KT_TEST__
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedGmBytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *rhoGm = static_cast<uint8_t *>(AscendC::GmAlloc(kRhoBytes));
    uint8_t *xGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeXBytes));
    uint8_t *lenGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeLenBytes));
    uint8_t *wsSeGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeWsBytes));
    uint8_t *shakeTilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kShakeTilingBytes));

    uint8_t *dstGm = static_cast<uint8_t *>(AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024));
    uint8_t *tHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(tHatFileSize > 1024 ? tHatFileSize : 1024));
    uint8_t *ekPolyGm = static_cast<uint8_t *>(AscendC::GmAlloc(ekPolyBytes > 1024 ? ekPolyBytes : 1024));
    uint8_t *ekPkeGm = static_cast<uint8_t *>(AscendC::GmAlloc(ekPkeBytes > 1024 ? ekPkeBytes : 1024));
    uint8_t *skGm = static_cast<uint8_t *>(AscendC::GmAlloc(skFileSize > 1024 ? skFileSize : 1024));
    uint8_t *dkKemGm = static_cast<uint8_t *>(AscendC::GmAlloc(F203KemKg::kDkKemBytes));
    uint8_t *wsGm = static_cast<uint8_t *>(AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024));

    std::memcpy(seedGm, seedSrc, kSeedGmBytes);
    std::memcpy(shakeTilingGm, &shakeTilingHost, kShakeTilingBytes);
    // 清零 sk/dk_kem：暴露 Encode 未写完的抢跑；CPU 软旗用 dk_kem[0:2]
    std::memset(skGm, 0, skFileSize);
    std::memset(dkKemGm, 0, F203KemKg::kDkKemBytes);
    for (size_t i = 0; i < wsFileSize; ++i) {
        wsGm[i] = 0;
    }
    rs = lutFileSize;
    if (!ReadFile("./input/lut_even_stacked.bin", rs, wsGm + tiling::LUT_EVEN_STACKED, lutFileSize)) {
        return 10;
    }
    rs = lutFileSize;
    if (!ReadFile("./input/lut_odd_stacked.bin", rs, wsGm + tiling::LUT_ODD_STACKED, lutFileSize)) {
        return 10;
    }

    ICPU_RUN_KF(f203_keygen_prep, kPrepBlockDim, seedGm, aHatGm, prfGm, srcGm, rhoGm, xGm, lenGm, wsSeGm, shakeTilingGm);

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    g_2s1e_mix_pass = vecTilingHost.mixPass;
    // L2：PKE compute + KEM 尾（seed_d 再读 4B 派生 z；dk_kem 写入独立 GM）
    ICPU_RUN_KF(mmad_custom, kMmadBlockDim, dstGm, tHatGm, ekPolyGm, skGm, srcGm, aHatGm, wsGm, vecTilingHost, rhoGm,
                ekPkeGm, seedGm, dkKemGm);

    if (!WriteFile("./output/ek_kem.bin", ekPkeGm, F203KemKg::kEkKemBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_kem.bin", dkKemGm, F203KemKg::kDkKemBytes)) {
        return 3;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(rhoGm);
    AscendC::GmFree(xGm);
    AscendC::GmFree(lenGm);
    AscendC::GmFree(wsSeGm);
    AscendC::GmFree(shakeTilingGm);
    AscendC::GmFree(dstGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(ekPolyGm);
    AscendC::GmFree(ekPkeGm);
    AscendC::GmFree(skGm);
    AscendC::GmFree(dkKemGm);
    AscendC::GmFree(wsGm);
#else
    CHECK_ACL(aclInit(nullptr));
    // 设备号：读 ASCEND_DEVICE_ID；缺省 0（标准默认；探针挂死脏退后同卡会连环挂，见 acl_session；需换卡时再 export）。SIM 由 run.sh 强制 export=0。
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    // 早退 / SIGINT / SIGTERM 均会 ResetDevice+Finalize，减轻同卡污染
    ascendc_acl::DeviceGuard aclGuard(deviceId);
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *rhoDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *wsSeDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;

    uint8_t *dstDev = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *ekPolyDev = nullptr;
    uint8_t *ekPkeHost = nullptr;
    uint8_t *ekPkeDev = nullptr;
    uint8_t *skDev = nullptr;
    uint8_t *dkKemHost = nullptr;
    uint8_t *dkKemDev = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *wsDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedGmBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedGmBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rhoDev), kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kSeXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kSeLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsSeDev), kSeWsBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&shakeTilingDev), kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dstDev), dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tHatDev), tHatFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPolyDev), ekPolyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), ekPkeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), ekPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&skDev), skFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkKemHost), F203KemKg::kDkKemBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkKemDev), F203KemKg::kDkKemBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsFileSize));

    std::memcpy(seedHost, seedSrc, kSeedGmBytes);
    std::memset(wsHost, 0, wsFileSize);
    // NPU/SIM：LUT 读失败不得静默继续（全 0 workspace → 错密钥，见 qa/2026-08-03）。
    rs = lutFileSize;
    if (!ReadFile("./input/lut_even_stacked.bin", rs, wsHost + tiling::LUT_EVEN_STACKED, lutFileSize) ||
        rs != lutFileSize) {
        std::cerr << "[FAIL] read input/lut_even_stacked.bin\n";
        return 10;
    }
    rs = lutFileSize;
    if (!ReadFile("./input/lut_odd_stacked.bin", rs, wsHost + tiling::LUT_ODD_STACKED, lutFileSize) ||
        rs != lutFileSize) {
        std::cerr << "[FAIL] read input/lut_odd_stacked.bin\n";
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedGmBytes, seedHost, kSeedGmBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTilingHost, kShakeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_keygen_prep_do(kPrepBlockDim, nullptr, stream, seedDev, aHatDev, prfDev, srcDev, rhoDev, xDev, lenDev, wsSeDev,
                        shakeTilingDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_keygen_prep"));

    // L2：stable mmad + F203_KEM_KEYGEN_TAIL 内嵌尾；ek 与 ek_kem 同缓冲
    ACLRT_LAUNCH_KERNEL(mmad_custom)(kMmadBlockDim, stream, dstDev, tHatDev, ekPolyDev, skDev, srcDev, aHatDev, wsDev,
                                     &vecTilingHost, rhoDev, ekPkeDev, seedDev, dkKemDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "mmad_custom"));

    CHECK_ACL(aclrtMemcpy(ekPkeHost, ekPkeBytes, ekPkeDev, ekPkeBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkKemHost, F203KemKg::kDkKemBytes, dkKemDev, F203KemKg::kDkKemBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/ek_kem.bin", ekPkeHost, F203KemKg::kEkKemBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_kem.bin", dkKemHost, F203KemKg::kDkKemBytes)) {
        return 3;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFree(wsSeDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(dstDev));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(ekPolyDev));
    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(skDev));
    CHECK_ACL(aclrtFree(dkKemDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(dkKemHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    // ResetDevice+Finalize 由 aclGuard 析构统一执行（含早退路径）
#endif
    return 0;
}
