/**
 * EN14-encrypt-cross-sticky · Encrypt×liboqs 粘性多轮（自 EN13 核 + EN12 编排模式）
 *
 * 同进程、同 acl session / 同一 stream，将 EN13 Alg.14 整链重复 R 轮（默认 8，
 * 环境变量 EN14_ROUNDS 可覆盖）：
 *   for r in 1..R:
 *     L0 SampleNTT → Host Âᵀ → L1 Prep → L2 NTT → L3a Matvec → L3b Dot
 *     → L4a/b INTT → Host 加噪 → L5 Pack → 落盘 output/rXX/c.bin
 *
 * 粘性约束：不 recreate stream、不重 aclInit；缓冲一次分配、R 轮复用。
 * 每轮换种子（input/rXX/ 下各 bin，SEED_D+(r-1)*10007）；硬门禁每轮 c≡liboqs（verify）。
 * 核来自 EN13；禁抄 Encrypt/KEM/frozen/ER；禁 -r npu。
 */
#include "data_utils.h"
#include "tiling.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include "aclrtlaunch_enc_samplentt_real.h"
#include "aclrtlaunch_enc_prep_cbd_real.h"
#include "aclrtlaunch_enc_matvec_real.h"
#include "aclrtlaunch_enc_dot_real.h"
#include "aclrtlaunch_enc_pack_compress_real.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" void enc_samplentt_real(GM_ADDR a_hat, GM_ADDR rho, int32_t k, int32_t n, int32_t q);
extern "C" void enc_prep_cbd_real(GM_ADDR dst, GM_ADDR sigma, int32_t k, int32_t n, int32_t q,
                                  int32_t nonce0);
extern "C" void enc_matvec_real(GM_ADDR t_hat, GM_ADDR a_hat, GM_ADDR s_hat, GM_ADDR gammas,
                                int32_t k, int32_t n, int32_t q);
extern "C" void enc_dot_real(GM_ADDR v_hat, GM_ADDR t_hat, GM_ADDR s_hat, GM_ADDR gammas,
                             int32_t k, int32_t n, int32_t q);
extern "C" void enc_pack_compress_real(GM_ADDR c_out, GM_ADDR u_in, GM_ADDR v_in, int32_t k,
                                       int32_t n, int32_t q);
#endif

/** 按 tiling 计算 NTT/INTT 段 src/dst/ws 字节数。 */
static void ResolveNttSizes(const TilingData *tiling, size_t *srcFileSize, size_t *dstFileSize,
                            size_t *wsFileSize)
{
    const int32_t maxBenchTile = tiling->q == 3329
                               ? tiling_one::max_kernel_bench_tile_kyber
                               : tiling_one::max_kernel_bench_tile;
    const size_t wsBench = tiling->bench < maxBenchTile
                         ? (size_t)tiling->bench
                         : (size_t)maxBenchTile;
    *wsFileSize = tiling_one::WorkspaceSizeForBench(wsBench);
    *srcFileSize = tiling_one::n * sizeof(int32_t) * (size_t)tiling->bench;
    *dstFileSize = *srcFileSize;
}

/**
 * 解析 EN14_ROUNDS；非法回落默认 8。
 * 背景：任务书默认 R=8；上限 64 防误设墙钟爆炸。
 */
static int32_t ResolveRounds()
{
    const char *env = std::getenv("EN14_ROUNDS");
    if (env == nullptr || env[0] == '\0') {
        return 8;
    }
    char *end = nullptr;
    long v = std::strtol(env, &end, 10);
    if (end == env || v < 1 || v > 64) {
        std::printf("[EN14] WARN EN14_ROUNDS='%s' invalid → default 8\n", env);
        return 8;
    }
    return static_cast<int32_t>(v);
}

/** Host：Â[K,K,N] → Âᵀ，使既有 matvec 实现 Âᵀ∘ŷ。 */
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

/** Host：逐系数 (x+y) mod q。 */
static void HostAddModQ(int32_t *dst, const int32_t *x, const int32_t *y, size_t elems, int32_t q)
{
    for (size_t i = 0; i < elems; ++i) {
        int64_t s = static_cast<int64_t>(x[i]) + static_cast<int64_t>(y[i]);
        s %= q;
        if (s < 0) {
            s += q;
        }
        dst[i] = static_cast<int32_t>(s);
    }
}

/** 拼 round 路径：prefix/rXX/name → buf。 */
static bool RoundPath(char *buf, size_t bufSz, const char *prefix, int32_t round, const char *name)
{
    int n = std::snprintf(buf, bufSz, "%s/r%02d/%s", prefix, round, name);
    return n > 0 && static_cast<size_t>(n) < bufSz;
}

/** 落盘第 r 轮完成标记。 */
static bool WriteRoundDone(int32_t round)
{
    char path[64];
    std::snprintf(path, sizeof(path), "./output/round_%02d_done.txt", round);
    char body[96];
    int n = std::snprintf(body, sizeof(body), "EN14 round %d finished OK\n", round);
    return WriteFile(path, body, static_cast<size_t>(n));
}

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const int32_t rounds = ResolveRounds();
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
    const size_t aHatBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) *
        sizeof(int32_t);
    const size_t sHatBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t tHatBytes = sHatBytes;
    const size_t vHatBytes = static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t gammaBytes = (static_cast<size_t>(kMatvecN) / 2U) * sizeof(int32_t);
    const size_t packUBytes = sHatBytes;
    const size_t packVBytes = vHatBytes;
    bool ok;
    char pathBuf[128];

    std::printf("[EN14] sticky Encrypt×liboqs begin R=%d (no stream recreate / no re-aclInit)\n",
                rounds);

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN14] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
                    prepOutBytes, tHatBytes);
        return 7;
    }

    const size_t hostScratch = std::max(
        dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, (size_t)1024))));
    // 粘性：下列缓冲只分配一次，R 轮复用
    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(hostScratch);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, std::max(aHatBytes, packUBytes)));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));
    uint8_t *sHat = (uint8_t *)AscendC::GmAlloc(sHatBytes);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *aHatT = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *tPub = (uint8_t *)AscendC::GmAlloc(tHatBytes);
    uint8_t *vHat = (uint8_t *)AscendC::GmAlloc(vHatBytes);
    uint8_t *gammas = (uint8_t *)AscendC::GmAlloc(gammaBytes);
    uint8_t *sigma = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *rho = (uint8_t *)AscendC::GmAlloc(kRhoBytes);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(packUBytes);
    uint8_t *e2 = (uint8_t *)AscendC::GmAlloc(packVBytes);
    uint8_t *mu = (uint8_t *)AscendC::GmAlloc(packVBytes);
    uint8_t *uNoisy = (uint8_t *)AscendC::GmAlloc(packUBytes);
    uint8_t *vNoisy = (uint8_t *)AscendC::GmAlloc(packVBytes);

    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammas, gammaBytes);
    if (!ok) return 44;

    for (int32_t r = 1; r <= rounds; ++r) {
        std::printf("[EN14] CPU round %d/%d begin\n", r, rounds);

        // 每轮换种子：从 input/rXX 读入
        size_t rhoRead = kRhoBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "rho.bin")) return 90;
        ok = ReadFile(pathBuf, rhoRead, rho, kRhoBytes);
        if (!ok) return 61;
        size_t sigmaRead = kSigmaBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "sigma.bin")) return 90;
        ok = ReadFile(pathBuf, sigmaRead, sigma, kSigmaBytes);
        if (!ok) return 31;
        size_t tPubRead = tHatBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "t_hat.bin")) return 90;
        ok = ReadFile(pathBuf, tPubRead, tPub, tHatBytes);
        if (!ok) return 71;
        size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "e1.bin")) return 90;
        ok = ReadFile(pathBuf, e1Read, e1, packUBytes);
        if (!ok) return 81;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "e2.bin")) return 90;
        ok = ReadFile(pathBuf, e2Read, e2, packVBytes);
        if (!ok) return 82;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "mu.bin")) return 90;
        ok = ReadFile(pathBuf, muRead, mu, packVBytes);
        if (!ok) return 83;

        // L0 SampleNTT + Host Âᵀ
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        ICPU_RUN_KF(enc_samplentt_real, stubBlockDim, aHat, rho, kMatvecK, kMatvecN, kMatvecQ);
        HostTransposeAHat(reinterpret_cast<int32_t *>(aHatT), reinterpret_cast<const int32_t *>(aHat),
                          kMatvecK, kMatvecN);

        // L1 Prep
        ICPU_RUN_KF(enc_prep_cbd_real, stubBlockDim, dst, sigma, kMatvecK, kMatvecN, kMatvecQ,
                    kPrepNonce0);
        std::memcpy(src, dst, prepOutBytes);

        // L2 NTT
        AscendC::SetKernelMode(KernelMode::MIX_MODE);
        ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
        if (!ok) return 10;
        ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
        std::memcpy(sHat, dst, sHatBytes);

        // L3a Matvec(Âᵀ)
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        ICPU_RUN_KF(enc_matvec_real, stubBlockDim, dst, aHatT, sHat, gammas, kMatvecK, kMatvecN,
                    kMatvecQ);
        std::memcpy(src, dst, tHatBytes);

        // L3b Dot
        ICPU_RUN_KF(enc_dot_real, stubBlockDim, vHat, tPub, sHat, gammas, kMatvecK, kMatvecN,
                    kMatvecQ);

        // L4a INTT(û)
        AscendC::SetKernelMode(KernelMode::MIX_MODE);
        ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
        if (!ok) return 20;
        ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
        std::memcpy(uNoisy, dst, packUBytes);

        // L4b INTT(v̂ pad)
        std::memset(src, 0, srcFileSize);
        std::memcpy(src, vHat, vHatBytes);
        ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
        std::memcpy(vNoisy, dst, packVBytes);

        // Host 加噪
        HostAddModQ(reinterpret_cast<int32_t *>(uNoisy), reinterpret_cast<const int32_t *>(uNoisy),
                    reinterpret_cast<const int32_t *>(e1),
                    static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN), kMatvecQ);
        HostAddModQ(reinterpret_cast<int32_t *>(vNoisy), reinterpret_cast<const int32_t *>(vNoisy),
                    reinterpret_cast<const int32_t *>(e2), static_cast<size_t>(kMatvecN), kMatvecQ);
        HostAddModQ(reinterpret_cast<int32_t *>(vNoisy), reinterpret_cast<const int32_t *>(vNoisy),
                    reinterpret_cast<const int32_t *>(mu), static_cast<size_t>(kMatvecN), kMatvecQ);

        // L5 Pack
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        std::memcpy(src, uNoisy, packUBytes);
        ICPU_RUN_KF(enc_pack_compress_real, stubBlockDim, dst, src, vNoisy, kMatvecK, kMatvecN,
                    kMatvecQ);

        // 落盘本轮 c
        std::snprintf(pathBuf, sizeof(pathBuf), "./output/r%02d", r);
        // mkdir 由 run.sh 预建；WriteFile 需父目录存在
        char cPath[96];
        std::snprintf(cPath, sizeof(cPath), "./output/r%02d/c.bin", r);
        ok = WriteFile(cPath, dst, kCipherBytes);
        if (!ok) return 52;
        if (r == 1) {
            ok = WriteFile("./output/c.bin", dst, kCipherBytes);
            if (!ok) return 53;
        }
        if (!WriteRoundDone(r)) return 60;
        std::printf("[EN14] CPU round %d/%d finished OK (c=%zu B)\n", r, rounds, kCipherBytes);
    }

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)sHat);
    AscendC::GmFree((void *)aHat);
    AscendC::GmFree((void *)aHatT);
    AscendC::GmFree((void *)tPub);
    AscendC::GmFree((void *)vHat);
    AscendC::GmFree((void *)gammas);
    AscendC::GmFree((void *)sigma);
    AscendC::GmFree((void *)rho);
    AscendC::GmFree((void *)e1);
    AscendC::GmFree((void *)e2);
    AscendC::GmFree((void *)mu);
    AscendC::GmFree((void *)uNoisy);
    AscendC::GmFree((void *)vNoisy);
    AscendC::GmFree((void *)tiling_data);
#else
    // 粘性：aclInit / CreateStream / 设备缓冲只做一次
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost, *sHatHost, *aHatHost, *aHatTHost, *tPubHost, *vHatHost,
        *gammaHost, *sigmaHost, *rhoHost, *e1Host, *e2Host, *muHost, *uHost, *vHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice, *sHatDevice, *aHatDevice, *aHatTDevice, *tPubDevice,
        *vHatDevice, *gammaDevice, *sigmaDevice, *rhoDevice, *uDevice, *vDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN14] FATAL size mismatch\n");
        return 7;
    }
    const size_t hostScratch =
        std::max(dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, packUBytes))));

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), hostScratch));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, hostScratch, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), hostScratch));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, hostScratch, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sHatHost), sHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&sHatDevice, sHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatHost), aHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&aHatDevice, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatTHost), aHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&aHatTDevice, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&tPubHost), tHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&tPubDevice, tHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&vHatHost), vHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&vHatDevice, vHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&gammaHost), gammaBytes));
    CHECK_ACL(aclrtMalloc((void **)&gammaDevice, gammaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaHost), kSigmaBytes));
    CHECK_ACL(aclrtMalloc((void **)&sigmaDevice, kSigmaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&rhoHost), kRhoBytes));
    CHECK_ACL(aclrtMalloc((void **)&rhoDevice, kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&e1Host), packUBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&e2Host), packVBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&muHost), packVBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&uHost), packUBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), packVBytes));
    CHECK_ACL(aclrtMalloc((void **)&uDevice, packUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDevice, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammaHost, gammaBytes);
    if (!ok) return 44;
    CHECK_ACL(aclrtMemcpy(gammaDevice, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    for (int32_t r = 1; r <= rounds; ++r) {
        std::printf("[EN14] SIM round %d/%d begin (same stream)\n", r, rounds);

        size_t rhoRead = kRhoBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "rho.bin")) return 90;
        ok = ReadFile(pathBuf, rhoRead, rhoHost, kRhoBytes);
        if (!ok) return 61;
        size_t sigmaRead = kSigmaBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "sigma.bin")) return 90;
        ok = ReadFile(pathBuf, sigmaRead, sigmaHost, kSigmaBytes);
        if (!ok) return 31;
        size_t tPubRead = tHatBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "t_hat.bin")) return 90;
        ok = ReadFile(pathBuf, tPubRead, tPubHost, tHatBytes);
        if (!ok) return 71;
        size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "e1.bin")) return 90;
        ok = ReadFile(pathBuf, e1Read, e1Host, packUBytes);
        if (!ok) return 81;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "e2.bin")) return 90;
        ok = ReadFile(pathBuf, e2Read, e2Host, packVBytes);
        if (!ok) return 82;
        if (!RoundPath(pathBuf, sizeof(pathBuf), "./input", r, "mu.bin")) return 90;
        ok = ReadFile(pathBuf, muRead, muHost, packVBytes);
        if (!ok) return 83;

        // L0
        CHECK_ACL(aclrtMemcpy(rhoDevice, kRhoBytes, rhoHost, kRhoBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_samplentt_real)
        (stubBlockDim, stream, aHatDevice, rhoDevice, kMatvecK, kMatvecN, kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(aHatHost, aHatBytes, aHatDevice, aHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        HostTransposeAHat(reinterpret_cast<int32_t *>(aHatTHost),
                          reinterpret_cast<const int32_t *>(aHatHost), kMatvecK, kMatvecN);
        CHECK_ACL(aclrtMemcpy(aHatTDevice, aHatBytes, aHatTHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));

        // L1
        CHECK_ACL(aclrtMemcpy(sigmaDevice, kSigmaBytes, sigmaHost, kSigmaBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_prep_cbd_real)
        (stubBlockDim, stream, dstDevice, sigmaDevice, kMatvecK, kMatvecN, kMatvecQ, kPrepNonce0);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(srcDevice, prepOutBytes, dstDevice, prepOutBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));

        // L2
        ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
        if (!ok) return 10;
        CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(sHatDevice, sHatBytes, dstDevice, sHatBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));

        // L3a
        ACLRT_LAUNCH_KERNEL(enc_matvec_real)
        (stubBlockDim, stream, dstDevice, aHatTDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN,
         kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(srcDevice, tHatBytes, dstDevice, tHatBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));

        // L3b
        CHECK_ACL(aclrtMemcpy(tPubDevice, tHatBytes, tPubHost, tHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_dot_real)
        (stubBlockDim, stream, vHatDevice, tPubDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN,
         kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(vHatHost, vHatBytes, vHatDevice, vHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));

        // L4a
        ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
        if (!ok) return 20;
        CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
        std::memcpy(uHost, dstHost, packUBytes);

        // L4b
        std::memset(srcHost, 0, srcFileSize);
        std::memcpy(srcHost, vHatHost, vHatBytes);
        CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
        std::memcpy(vHost, dstHost, packVBytes);

        // Host 加噪
        HostAddModQ(reinterpret_cast<int32_t *>(uHost), reinterpret_cast<const int32_t *>(uHost),
                    reinterpret_cast<const int32_t *>(e1Host),
                    static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN), kMatvecQ);
        HostAddModQ(reinterpret_cast<int32_t *>(vHost), reinterpret_cast<const int32_t *>(vHost),
                    reinterpret_cast<const int32_t *>(e2Host), static_cast<size_t>(kMatvecN),
                    kMatvecQ);
        HostAddModQ(reinterpret_cast<int32_t *>(vHost), reinterpret_cast<const int32_t *>(vHost),
                    reinterpret_cast<const int32_t *>(muHost), static_cast<size_t>(kMatvecN),
                    kMatvecQ);
        CHECK_ACL(aclrtMemcpy(uDevice, packUBytes, uHost, packUBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(vDevice, packVBytes, vHost, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));

        // L5
        ACLRT_LAUNCH_KERNEL(enc_pack_compress_real)
        (stubBlockDim, stream, dstDevice, uDevice, vDevice, kMatvecK, kMatvecN, kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        CHECK_ACL(aclrtMemcpy(dstHost, kCipherBytes, dstDevice, kCipherBytes, ACL_MEMCPY_DEVICE_TO_HOST));

        char cPath[96];
        std::snprintf(cPath, sizeof(cPath), "./output/r%02d/c.bin", r);
        ok = WriteFile(cPath, dstHost, kCipherBytes);
        if (!ok) return 52;
        if (r == 1) {
            ok = WriteFile("./output/c.bin", dstHost, kCipherBytes);
            if (!ok) return 53;
        }
        if (!WriteRoundDone(r)) return 60;
        std::printf("[EN14] SIM round %d/%d finished OK\n", r, rounds);
    }

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(sHatDevice));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFree(aHatDevice));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFree(aHatTDevice));
    CHECK_ACL(aclrtFreeHost(aHatTHost));
    CHECK_ACL(aclrtFree(tPubDevice));
    CHECK_ACL(aclrtFreeHost(tPubHost));
    CHECK_ACL(aclrtFree(vHatDevice));
    CHECK_ACL(aclrtFreeHost(vHatHost));
    CHECK_ACL(aclrtFree(gammaDevice));
    CHECK_ACL(aclrtFreeHost(gammaHost));
    CHECK_ACL(aclrtFree(sigmaDevice));
    CHECK_ACL(aclrtFreeHost(sigmaHost));
    CHECK_ACL(aclrtFree(rhoDevice));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFree(uDevice));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFree(vDevice));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFreeHost(e1Host));
    CHECK_ACL(aclrtFreeHost(e2Host));
    CHECK_ACL(aclrtFreeHost(muHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN14] sticky R=%d rounds finished OK (Encrypt×liboqs)\n", rounds);
    return 0;
}
