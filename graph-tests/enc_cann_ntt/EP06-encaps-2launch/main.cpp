/**
 * EP06-encaps-2launch · Alg.16/20 Encaps × liboqs · Host launch = 2
 *
 * 允许读入：ek_kem、m + LUT；r/coins 由 Host FO 内存派生（禁止落中间 .bin）
 * 禁止读入：ρ / t̂ / e1 / e2 / μ / σ / r 等中间 .bin
 * Host FO→K；L1 SampleNTT+CBD(y/e1/e2)；开场一次 H2D；仅 c/K 出
 */
#include "data_utils.h"
#include "host_ek_decode.hpp"
#include "host_hg_sha3.hpp"
#include "tiling.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_enc_prep_l1.h"
#include "aclrtlaunch_enc_compute_l2.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void enc_prep_l1(GM_ADDR a_hat, GM_ADDR y_out, GM_ADDR e1_out, GM_ADDR e2_out,
                            GM_ADDR rho, GM_ADDR sigma, int32_t k, int32_t n, int32_t q,
                            int32_t nonce0);
extern "C" void enc_compute_l2(GM_ADDR c_out, GM_ADDR y_in, GM_ADDR a_hat, GM_ADDR t_pub,
                               GM_ADDR gammas, GM_ADDR e1, GM_ADDR e2, GM_ADDR m, GM_ADDR ws_ntt,
                               GM_ADDR ws_intt, GM_ADDR scratch, TilingData tiling);
#endif

/** Host launch 审计计数（仅 SIM/NPU 路径累加 ACLRT_LAUNCH_KERNEL）。 */
static int g_hostLaunchCount = 0;

static void ResolveNttSizes(const TilingData *tiling, size_t *srcFileSize, size_t *dstFileSize,
                            size_t *wsFileSize)
{
    const int32_t maxBenchTile = tiling->q == 3329 ? tiling_one::max_kernel_bench_tile_kyber
                                                   : tiling_one::max_kernel_bench_tile;
    const size_t wsBench =
        tiling->bench < maxBenchTile ? (size_t)tiling->bench : (size_t)maxBenchTile;
    *wsFileSize = tiling_one::WorkspaceSizeForBench(wsBench);
    *srcFileSize = tiling_one::n * sizeof(int32_t) * (size_t)tiling->bench;
    *dstFileSize = *srcFileSize;
}

static bool HostFoAndLoad(uint8_t *rho, int32_t *tHat, uint8_t *coins, uint8_t *m,
                          uint8_t *wsNtt, uint8_t *wsIntt, uint8_t *gammas,
                          size_t matMFileSize, size_t gammaBytes)
{
    std::vector<uint8_t> ek(host_ek::kEkBytes);
    size_t ekR = host_ek::kEkBytes;
    size_t mR = 32, gR = gammaBytes, nttR = matMFileSize, inttR = matMFileSize;
    if (!ReadFile("./input/ek_kem.bin", ekR, ek.data(), host_ek::kEkBytes) || ekR != host_ek::kEkBytes) {
        std::printf("[EP06] need input/ek_kem.bin\n");
        return false;
    }
    if (!ReadFile("./input/m.bin", mR, m, 32) || mR != 32) {
        std::printf("[EP06] need input/m.bin\n");
        return false;
    }
    uint8_t h[32], kBar[32], r[32];
    host_hg::H_Sha3_256(h, ek.data(), host_ek::kEkBytes);
    host_hg::G_Sha3_512(kBar, r, m, h);
    std::memcpy(coins, r, 32);
    if (!WriteFile("./output/K.bin", kBar, 32)) {
        return false;
    }
    if (!host_ek::DecodeEkPke(rho, tHat, ek.data(), ekR)) {
        std::printf("[EP06] DecodeEkPke failed\n");
        return false;
    }
    if (!ReadFile("./input/M4_ntt.bin", nttR, wsNtt, matMFileSize)) {
        return false;
    }
    if (!ReadFile("./input/M4_intt.bin", inttR, wsIntt, matMFileSize)) {
        return false;
    }
    if (!ReadFile("./input/gammas.bin", gR, gammas, gammaBytes)) {
        return false;
    }
    return true;
}


int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t matMFileSize = tiling_one::n * tiling_one::n * 4;
    uint32_t mixBlockDim = 1;
    uint32_t stubBlockDim = 1;

    constexpr int32_t kMatvecK = 4;
    constexpr int32_t kMatvecN = 256;
    constexpr int32_t kMatvecQ = 3329;
    constexpr int32_t kPrepNonce0 = 0;
    constexpr size_t kSigmaBytes = 32;
    constexpr size_t kRhoBytes = 32;
    constexpr size_t kMsgBytes = 32;
    constexpr size_t kC1PolyBytes = 352;
    constexpr size_t kC2Bytes = 160;
    constexpr size_t kCipherBytes = kMatvecK * kC1PolyBytes + kC2Bytes;
    const size_t prepOutBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t aHatBytes = static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecK) *
                             static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t sHatBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t tHatBytes = sHatBytes;
    const size_t vHatBytes = static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t gammaBytes = (static_cast<size_t>(kMatvecN) / 2U) * sizeof(int32_t);
    const size_t packUBytes = sHatBytes;
    const size_t packVBytes = vHatBytes;
    const size_t scratchBytes = 4 * sHatBytes + 2 * vHatBytes;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) {
        return 8;
    }
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes) {
        std::printf("[EP06] FATAL size mismatch src=%zu prep=%zu\n", srcFileSize, prepOutBytes);
        return 7;
    }

    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *y = (uint8_t *)AscendC::GmAlloc(prepOutBytes);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(packUBytes);
    uint8_t *e2 = (uint8_t *)AscendC::GmAlloc(packVBytes);
    uint8_t *cOut = (uint8_t *)AscendC::GmAlloc(kCipherBytes);
    uint8_t *wsNtt = (uint8_t *)AscendC::GmAlloc(wsFileSize);
    uint8_t *wsIntt = (uint8_t *)AscendC::GmAlloc(wsFileSize);
    uint8_t *scratch = (uint8_t *)AscendC::GmAlloc(scratchBytes);
    uint8_t *tPub = (uint8_t *)AscendC::GmAlloc(tHatBytes);
    uint8_t *gammas = (uint8_t *)AscendC::GmAlloc(gammaBytes);
    uint8_t *sigma = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *rho = (uint8_t *)AscendC::GmAlloc(kRhoBytes);
    uint8_t *m = (uint8_t *)AscendC::GmAlloc(kMsgBytes);

    if (!HostFoAndLoad(rho, reinterpret_cast<int32_t *>(tPub), sigma, m, wsNtt, wsIntt, gammas,
                           matMFileSize, gammaBytes)) {
        return 11;
    }

    std::printf("[EP06] CPU L1 prep begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_prep_l1, stubBlockDim, aHat, y, e1, e2, rho, sigma, kMatvecK, kMatvecN,
                kMatvecQ, kPrepNonce0);
    ++g_hostLaunchCount;
    std::printf("[EP06] CPU L1 prep done (no intermediate dump)\n");

    std::printf("[EP06] CPU L2 compute begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(enc_compute_l2, mixBlockDim, cOut, y, aHat, tPub, gammas, e1, e2, m, wsNtt, wsIntt,
                scratch, *tiling);
    ++g_hostLaunchCount;
    ok = WriteFile("./output/c.bin", cOut, kCipherBytes);
    if (!ok) {
        return 53;
    }
    std::printf("[EP06] CPU L2 compute done\n");

    AscendC::GmFree((void *)aHat);
    AscendC::GmFree((void *)y);
    AscendC::GmFree((void *)e1);
    AscendC::GmFree((void *)e2);
    AscendC::GmFree((void *)cOut);
    AscendC::GmFree((void *)wsNtt);
    AscendC::GmFree((void *)wsIntt);
    AscendC::GmFree((void *)scratch);
    AscendC::GmFree((void *)tPub);
    AscendC::GmFree((void *)gammas);
    AscendC::GmFree((void *)sigma);
    AscendC::GmFree((void *)rho);
    AscendC::GmFree((void *)m);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes) {
        std::printf("[EP06] FATAL size mismatch\n");
        return 7;
    }

    uint8_t *cHost, *wsNttHost, *wsInttHost, *tPubHost, *gammaHost, *sigmaHost, *rhoHost, *mHost;
    uint8_t *aHatDev, *yDev, *e1Dev, *e2Dev, *cDev, *wsNttDev, *wsInttDev, *scratchDev;
    uint8_t *tPubDev, *gammaDev, *sigmaDev, *rhoDev, *mDev;

    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCipherBytes));
    CHECK_ACL(aclrtMalloc((void **)&cDev, kCipherBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsNttHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsNttDev, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsInttHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsInttDev, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&tPubHost), tHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&tPubDev, tHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&gammaHost), gammaBytes));
    CHECK_ACL(aclrtMalloc((void **)&gammaDev, gammaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaHost), kSigmaBytes));
    CHECK_ACL(aclrtMalloc((void **)&sigmaDev, kSigmaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&rhoHost), kRhoBytes));
    CHECK_ACL(aclrtMalloc((void **)&rhoDev, kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&mHost), kMsgBytes));
    CHECK_ACL(aclrtMalloc((void **)&mDev, kMsgBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&yDev, prepOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e1Dev, packUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e2Dev, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&scratchDev, scratchBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    if (!HostFoAndLoad(rhoHost, reinterpret_cast<int32_t *>(tPubHost), sigmaHost, mHost,
                           wsNttHost, wsInttHost, gammaHost, matMFileSize, gammaBytes)) {
        return 11;
    }

    CHECK_ACL(aclrtMemcpy(rhoDev, kRhoBytes, rhoHost, kRhoBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sigmaDev, kSigmaBytes, sigmaHost, kSigmaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tPubDev, tHatBytes, tPubHost, tHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(gammaDev, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsNttDev, wsFileSize, wsNttHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsInttDev, wsFileSize, wsInttHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, kMsgBytes, mHost, kMsgBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemset(e1Dev, packUBytes, 0, packUBytes));
    CHECK_ACL(aclrtMemset(e2Dev, packVBytes, 0, packVBytes));

    std::printf("[EP06] SIM L1 prep begin\n");
    ACLRT_LAUNCH_KERNEL(enc_prep_l1)
    (stubBlockDim, stream, aHatDev, yDev, e1Dev, e2Dev, rhoDev, sigmaDev, kMatvecK, kMatvecN,
     kMatvecQ, kPrepNonce0);
    ++g_hostLaunchCount;
    CHECK_ACL(aclrtSynchronizeStream(stream));
    std::printf("[EP06] SIM L1 done (A/y/e1/e2 on device; no mid H2D)\n");

    std::printf("[EP06] SIM L2 compute begin\n");
    ACLRT_LAUNCH_KERNEL(enc_compute_l2)
    (mixBlockDim, stream, cDev, yDev, aHatDev, tPubDev, gammaDev, e1Dev, e2Dev, mDev, wsNttDev,
     wsInttDev, scratchDev, tiling);
    ++g_hostLaunchCount;
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(cHost, kCipherBytes, cDev, kCipherBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/c.bin", cHost, kCipherBytes);
    if (!ok) {
        return 53;
    }
    std::printf("[EP06] SIM L2 compute done\n");

    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(yDev));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFree(e2Dev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFree(wsNttDev));
    CHECK_ACL(aclrtFreeHost(wsNttHost));
    CHECK_ACL(aclrtFree(wsInttDev));
    CHECK_ACL(aclrtFreeHost(wsInttHost));
    CHECK_ACL(aclrtFree(scratchDev));
    CHECK_ACL(aclrtFree(tPubDev));
    CHECK_ACL(aclrtFreeHost(tPubHost));
    CHECK_ACL(aclrtFree(gammaDev));
    CHECK_ACL(aclrtFreeHost(gammaHost));
    CHECK_ACL(aclrtFree(sigmaDev));
    CHECK_ACL(aclrtFreeHost(sigmaHost));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EP06] host_launch_count=%d (expect 2)\n", g_hostLaunchCount);
    if (g_hostLaunchCount != 2) {
        std::printf("[EP06] FATAL launch audit failed\n");
        return 99;
    }
    std::printf("[EP06] segments finished OK (Encrypt 2-launch)\n");
    return 0;
}
