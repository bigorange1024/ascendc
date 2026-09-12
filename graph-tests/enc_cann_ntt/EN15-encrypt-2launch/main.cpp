/**
 * EN15-encrypt-2launch · Alg.14 Encrypt × liboqs · Host launch = 2
 *
 * 外形（与 PLAN / 历史 prep+compute 一致）：
 *   L1 prep    — enc_prep_l1：SampleNTT(ρ)→Â + CBD(coins)→y（1 launch）
 *   Host mid   — Â→Âᵀ；上传 t̂/gammas/e1/e2/μ/M4_ntt/M4_intt（无 ACLRT_LAUNCH_KERNEL）
 *   L2 compute — enc_compute_l2：NTT→matvec→dot→INTT×2→加噪→pack→c（1 launch）
 *
 * 硬门禁：Host ACLRT_LAUNCH_KERNEL 计数 == 2；c ≡ liboqs max=0。禁 -r npu（本战役）。
 */
#include "data_utils.h"
#include "tiling.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_enc_prep_l1.h"
#include "aclrtlaunch_enc_compute_l2.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void enc_prep_l1(GM_ADDR a_hat, GM_ADDR y_out, GM_ADDR rho, GM_ADDR sigma, int32_t k,
                            int32_t n, int32_t q, int32_t nonce0);
extern "C" void enc_compute_l2(GM_ADDR c_out, GM_ADDR y_in, GM_ADDR a_hat_T, GM_ADDR t_pub,
                               GM_ADDR gammas, GM_ADDR e1, GM_ADDR e2, GM_ADDR mu, GM_ADDR ws_ntt,
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

/** Host：Â[K,K,N] 转置为 Âᵀ，供 matvec 实现 Âᵀ∘ŷ。 */
static void HostTransposeAHat(int32_t *dst, const int32_t *src, int32_t k, int32_t n)
{
    for (int32_t p = 0; p < k; ++p) {
        for (int32_t j = 0; j < k; ++j) {
            const int32_t *sp = src + ((j * k + p) * n);
            int32_t *dp = dst + ((p * k + j) * n);
            std::memcpy(dp, sp, static_cast<size_t>(n) * sizeof(int32_t));
        }
    }
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
    // scratch：4·vec + 2·poly（见 enc_compute_l2 注释）
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
        std::printf("[EN15] FATAL size mismatch src=%zu prep=%zu\n", srcFileSize, prepOutBytes);
        return 7;
    }

    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *aHatT = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *y = (uint8_t *)AscendC::GmAlloc(prepOutBytes);
    uint8_t *cOut = (uint8_t *)AscendC::GmAlloc(kCipherBytes);
    uint8_t *wsNtt = (uint8_t *)AscendC::GmAlloc(wsFileSize);
    uint8_t *wsIntt = (uint8_t *)AscendC::GmAlloc(wsFileSize);
    uint8_t *scratch = (uint8_t *)AscendC::GmAlloc(scratchBytes);
    uint8_t *tPub = (uint8_t *)AscendC::GmAlloc(tHatBytes);
    uint8_t *gammas = (uint8_t *)AscendC::GmAlloc(gammaBytes);
    uint8_t *sigma = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *rho = (uint8_t *)AscendC::GmAlloc(kRhoBytes);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(packUBytes);
    uint8_t *e2 = (uint8_t *)AscendC::GmAlloc(packVBytes);
    uint8_t *mu = (uint8_t *)AscendC::GmAlloc(packVBytes);

    // -------- L1 Prep --------
    std::printf("[EN15] CPU L1 prep begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t rhoRead = kRhoBytes, sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/rho.bin", rhoRead, rho, kRhoBytes);
    if (!ok) {
        return 61;
    }
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigma, kSigmaBytes);
    if (!ok) {
        return 31;
    }
    ICPU_RUN_KF(enc_prep_l1, stubBlockDim, aHat, y, rho, sigma, kMatvecK, kMatvecN, kMatvecQ,
                kPrepNonce0);
    ++g_hostLaunchCount; // CPU 孪生：ICPU_RUN_KF 计为 1 次语义 launch
    ok = WriteFile("./output/dst_a_hat.bin", aHat, aHatBytes);
    if (!ok) {
        return 62;
    }
    ok = WriteFile("./output/dst_prep.bin", y, prepOutBytes);
    if (!ok) {
        return 32;
    }
    HostTransposeAHat(reinterpret_cast<int32_t *>(aHatT), reinterpret_cast<const int32_t *>(aHat),
                      kMatvecK, kMatvecN);
    ok = WriteFile("./output/dst_a_hat_T.bin", aHatT, aHatBytes);
    if (!ok) {
        return 63;
    }
    std::printf("[EN15] CPU L1 prep + Host Âᵀ done\n");

    // -------- L2 Compute --------
    std::printf("[EN15] CPU L2 compute begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsNtt + 0, matMFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsIntt + 0, matMFileSize);
    if (!ok) {
        return 20;
    }
    size_t tPubRead = tHatBytes, gammaRead = gammaBytes;
    ok = ReadFile("./input/t_hat.bin", tPubRead, tPub, tHatBytes);
    if (!ok) {
        return 71;
    }
    ok = ReadFile("./input/gammas.bin", gammaRead, gammas, gammaBytes);
    if (!ok) {
        return 44;
    }
    size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
    ok = ReadFile("./input/e1.bin", e1Read, e1, packUBytes);
    if (!ok) {
        return 81;
    }
    ok = ReadFile("./input/e2.bin", e2Read, e2, packVBytes);
    if (!ok) {
        return 82;
    }
    ok = ReadFile("./input/mu.bin", muRead, mu, packVBytes);
    if (!ok) {
        return 83;
    }
    ICPU_RUN_KF(enc_compute_l2, mixBlockDim, cOut, y, aHatT, tPub, gammas, e1, e2, mu, wsNtt, wsIntt,
                scratch, *tiling);
    ++g_hostLaunchCount;
    ok = WriteFile("./output/c.bin", cOut, kCipherBytes);
    if (!ok) {
        return 53;
    }
    ok = WriteFile("./output/dst_pack.bin", cOut, kCipherBytes);
    if (!ok) {
        return 52;
    }
    std::printf("[EN15] CPU L2 compute done (c=%zu B)\n", kCipherBytes);

    AscendC::GmFree((void *)aHat);
    AscendC::GmFree((void *)aHatT);
    AscendC::GmFree((void *)y);
    AscendC::GmFree((void *)cOut);
    AscendC::GmFree((void *)wsNtt);
    AscendC::GmFree((void *)wsIntt);
    AscendC::GmFree((void *)scratch);
    AscendC::GmFree((void *)tPub);
    AscendC::GmFree((void *)gammas);
    AscendC::GmFree((void *)sigma);
    AscendC::GmFree((void *)rho);
    AscendC::GmFree((void *)e1);
    AscendC::GmFree((void *)e2);
    AscendC::GmFree((void *)mu);
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
        std::printf("[EN15] FATAL size mismatch\n");
        return 7;
    }

    uint8_t *aHatHost, *aHatTHost, *yHost, *cHost, *wsNttHost, *wsInttHost, *scratchHost;
    uint8_t *tPubHost, *gammaHost, *sigmaHost, *rhoHost, *e1Host, *e2Host, *muHost;
    uint8_t *aHatDev, *aHatTDev, *yDev, *cDev, *wsNttDev, *wsInttDev, *scratchDev;
    uint8_t *tPubDev, *gammaDev, *sigmaDev, *rhoDev, *e1Dev, *e2Dev, *muDev;

    CHECK_ACL(aclrtMallocHost((void **)(&aHatHost), aHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatTHost), aHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&aHatTDev, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&yHost), prepOutBytes));
    CHECK_ACL(aclrtMalloc((void **)&yDev, prepOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCipherBytes));
    CHECK_ACL(aclrtMalloc((void **)&cDev, kCipherBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsNttHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsNttDev, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsInttHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsInttDev, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&scratchHost), scratchBytes));
    CHECK_ACL(aclrtMalloc((void **)&scratchDev, scratchBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&tPubHost), tHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&tPubDev, tHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&gammaHost), gammaBytes));
    CHECK_ACL(aclrtMalloc((void **)&gammaDev, gammaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaHost), kSigmaBytes));
    CHECK_ACL(aclrtMalloc((void **)&sigmaDev, kSigmaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&rhoHost), kRhoBytes));
    CHECK_ACL(aclrtMalloc((void **)&rhoDev, kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&e1Host), packUBytes));
    CHECK_ACL(aclrtMalloc((void **)&e1Dev, packUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&e2Host), packVBytes));
    CHECK_ACL(aclrtMalloc((void **)&e2Dev, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&muHost), packVBytes));
    CHECK_ACL(aclrtMalloc((void **)&muDev, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    // -------- L1 --------
    std::printf("[EN15] SIM L1 prep begin\n");
    size_t rhoRead = kRhoBytes, sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/rho.bin", rhoRead, rhoHost, kRhoBytes);
    if (!ok) {
        return 61;
    }
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigmaHost, kSigmaBytes);
    if (!ok) {
        return 31;
    }
    CHECK_ACL(aclrtMemcpy(rhoDev, kRhoBytes, rhoHost, kRhoBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sigmaDev, kSigmaBytes, sigmaHost, kSigmaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_prep_l1)
    (stubBlockDim, stream, aHatDev, yDev, rhoDev, sigmaDev, kMatvecK, kMatvecN, kMatvecQ,
     kPrepNonce0);
    ++g_hostLaunchCount;
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(aHatHost, aHatBytes, aHatDev, aHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(yHost, prepOutBytes, yDev, prepOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_a_hat.bin", aHatHost, aHatBytes);
    if (!ok) {
        return 62;
    }
    ok = WriteFile("./output/dst_prep.bin", yHost, prepOutBytes);
    if (!ok) {
        return 32;
    }
    HostTransposeAHat(reinterpret_cast<int32_t *>(aHatTHost),
                      reinterpret_cast<const int32_t *>(aHatHost), kMatvecK, kMatvecN);
    CHECK_ACL(aclrtMemcpy(aHatTDev, aHatBytes, aHatTHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = WriteFile("./output/dst_a_hat_T.bin", aHatTHost, aHatBytes);
    if (!ok) {
        return 63;
    }
    std::printf("[EN15] SIM L1 prep + Host Âᵀ done\n");

    // -------- Host mid-sync：喂 L2 输入（无 launch）--------
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsNttHost, matMFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsInttHost, matMFileSize);
    if (!ok) {
        return 20;
    }
    CHECK_ACL(aclrtMemcpy(wsNttDev, wsFileSize, wsNttHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsInttDev, wsFileSize, wsInttHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    size_t tPubRead = tHatBytes, gammaRead = gammaBytes;
    ok = ReadFile("./input/t_hat.bin", tPubRead, tPubHost, tHatBytes);
    if (!ok) {
        return 71;
    }
    ok = ReadFile("./input/gammas.bin", gammaRead, gammaHost, gammaBytes);
    if (!ok) {
        return 44;
    }
    size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
    ok = ReadFile("./input/e1.bin", e1Read, e1Host, packUBytes);
    if (!ok) {
        return 81;
    }
    ok = ReadFile("./input/e2.bin", e2Read, e2Host, packVBytes);
    if (!ok) {
        return 82;
    }
    ok = ReadFile("./input/mu.bin", muRead, muHost, packVBytes);
    if (!ok) {
        return 83;
    }
    CHECK_ACL(aclrtMemcpy(tPubDev, tHatBytes, tPubHost, tHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(gammaDev, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e1Dev, packUBytes, e1Host, packUBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e2Dev, packVBytes, e2Host, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(muDev, packVBytes, muHost, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // -------- L2 --------
    std::printf("[EN15] SIM L2 compute begin\n");
    ACLRT_LAUNCH_KERNEL(enc_compute_l2)
    (mixBlockDim, stream, cDev, yDev, aHatTDev, tPubDev, gammaDev, e1Dev, e2Dev, muDev, wsNttDev,
     wsInttDev, scratchDev, tiling);
    ++g_hostLaunchCount;
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(cHost, kCipherBytes, cDev, kCipherBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/c.bin", cHost, kCipherBytes);
    if (!ok) {
        return 53;
    }
    ok = WriteFile("./output/dst_pack.bin", cHost, kCipherBytes);
    if (!ok) {
        return 52;
    }
    std::printf("[EN15] SIM L2 compute done\n");

    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFree(aHatTDev));
    CHECK_ACL(aclrtFreeHost(aHatTHost));
    CHECK_ACL(aclrtFree(yDev));
    CHECK_ACL(aclrtFreeHost(yHost));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFree(wsNttDev));
    CHECK_ACL(aclrtFreeHost(wsNttHost));
    CHECK_ACL(aclrtFree(wsInttDev));
    CHECK_ACL(aclrtFreeHost(wsInttHost));
    CHECK_ACL(aclrtFree(scratchDev));
    CHECK_ACL(aclrtFreeHost(scratchHost));
    CHECK_ACL(aclrtFree(tPubDev));
    CHECK_ACL(aclrtFreeHost(tPubHost));
    CHECK_ACL(aclrtFree(gammaDev));
    CHECK_ACL(aclrtFreeHost(gammaHost));
    CHECK_ACL(aclrtFree(sigmaDev));
    CHECK_ACL(aclrtFreeHost(sigmaHost));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFreeHost(e1Host));
    CHECK_ACL(aclrtFree(e2Dev));
    CHECK_ACL(aclrtFreeHost(e2Host));
    CHECK_ACL(aclrtFree(muDev));
    CHECK_ACL(aclrtFreeHost(muHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN15] host_launch_count=%d (expect 2)\n", g_hostLaunchCount);
    if (g_hostLaunchCount != 2) {
        std::printf("[EN15] FATAL launch audit failed\n");
        return 99;
    }
    std::printf("[EN15] segments finished OK (Encrypt 2-launch × liboqs)\n");
    return 0;
}
