/**
 * EP02-encaps-call-encrypt · Alg.14 Encrypt × liboqs 交叉（Host 多段 + cann-ntt）
 *
 * 段序（同 session；各段独立 launch；禁融胖 MIX GATE 4/8）：
 *   L0 SampleNTT — enc_samplentt_real     （ρ → Â；随后 Host 转置为 Âᵀ）
 *   L1 Prep      — enc_prep_cbd_real        （coins → y；nonce 0..3）
 *   L2 NTT       — mmad_custom + M4_ntt     （ŷ = NTT(y)）
 *   L3a Matvec   — enc_matvec_real          （û = Âᵀ ∘ ŷ）
 *   L3b Dot      — enc_dot_real             （v̂ = ⟨t̂, ŷ⟩）
 *   L4a INTT_u   — mmad_custom + M4_intt    （u ← INTT(û)）
 *   L4b INTT_v   — mmad_custom + M4_intt    （v ← INTT(v̂||0||0||0) 取 poly0）
 *   Host 加噪    — u+=e1；v+=e2+μ（FEEDBACK 声明：加噪在 Host）
 *   L5 Pack      — enc_pack_compress_real   （c = BE(Compress(u))‖BE(Compress(v))）
 *
 * 硬门禁：output/c.bin ≡ liboqs fixture c（max=0）。禁 -r npu。
 */
#include "data_utils.h"
#include "host_hg_sha3.hpp"
#include "tiling.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
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
 * Host：把 Â[K,K,N] 行主序转置为 Âᵀ，使既有 matvec(t[p]+=A[p,j]∘s[j]) 实现 Âᵀ∘ŷ。
 * flat(p,j,c)=(p·K+j)·N+c → 写 dst 时交换 p↔j。
 */
static void HostTransposeAHat(int32_t *dst, const int32_t *src, int32_t k, int32_t n)
{
    for (int32_t p = 0; p < k; ++p) {
        for (int32_t j = 0; j < k; ++j) {
            const int32_t *sp = src + ((j * k + p) * n); // A[j,p]
            int32_t *dp = dst + ((p * k + j) * n);       // A_T[p,j]
            std::memcpy(dp, sp, static_cast<size_t>(n) * sizeof(int32_t));
        }
    }
}

/** Host：逐系数 (x+y) mod q，写回 dst（可与 x 同缓冲）。 */
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
    constexpr int32_t kPrepNonce0 = 0; // Encrypt：y←PRF(r,0..3)
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

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EP02] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
                    prepOutBytes, tHatBytes);
        return 7;
    }

    const size_t hostScratch = std::max(
        dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, (size_t)1024))));
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

    // -------- Host Encaps 头：H(ek)、G(m‖H)→(K̄,r)；本刀 r 已由 gen_data 写入 sigma --------
    std::printf("[EP02] CPU Host H/G begin\n");
    {
        uint8_t *ekHostTmp = (uint8_t *)AscendC::GmAlloc(kCipherBytes);
        uint8_t *mHostTmp = (uint8_t *)AscendC::GmAlloc(32);
        uint8_t *hTmp = (uint8_t *)AscendC::GmAlloc(32);
        uint8_t *kTmp = (uint8_t *)AscendC::GmAlloc(32);
        uint8_t *rTmp = (uint8_t *)AscendC::GmAlloc(32);
        size_t ekR = kCipherBytes, mR = 32;
        ok = ReadFile("./input/ek_kem.bin", ekR, ekHostTmp, kCipherBytes);
        if (!ok) return 91;
        ok = ReadFile("./input/m.bin", mR, mHostTmp, 32);
        if (!ok) return 92;
        host_hg::H_Sha3_256(hTmp, ekHostTmp, kCipherBytes);
        host_hg::G_Sha3_512(kTmp, rTmp, mHostTmp, hTmp);
        ok = WriteFile("./output/K.bin", kTmp, 32);
        if (!ok) return 93;
        ok = WriteFile("./output/r_host.bin", rTmp, 32);
        if (!ok) return 94;
        AscendC::GmFree((void *)ekHostTmp);
        AscendC::GmFree((void *)mHostTmp);
        AscendC::GmFree((void *)hTmp);
        AscendC::GmFree((void *)kTmp);
        AscendC::GmFree((void *)rTmp);
    }
    std::printf("[EP02] CPU Host H/G done (K written)\n");

    // -------- L0 SampleNTT --------
    std::printf("[EP02] CPU L0 SampleNTT begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t rhoRead = kRhoBytes;
    ok = ReadFile("./input/rho.bin", rhoRead, rho, kRhoBytes);
    if (!ok) return 61;
    ICPU_RUN_KF(enc_samplentt_real, stubBlockDim, aHat, rho, kMatvecK, kMatvecN, kMatvecQ);
    ok = WriteFile("./output/dst_a_hat.bin", aHat, aHatBytes);
    if (!ok) return 62;
    HostTransposeAHat(reinterpret_cast<int32_t *>(aHatT), reinterpret_cast<const int32_t *>(aHat),
                      kMatvecK, kMatvecN);
    ok = WriteFile("./output/dst_a_hat_T.bin", aHatT, aHatBytes);
    if (!ok) return 63;
    std::printf("[EP02] CPU L0 SampleNTT+Host Âᵀ done\n");

    // -------- L1 Prep（coins→y）--------
    std::printf("[EP02] CPU L1 Prep(CBD y) begin\n");
    size_t sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigma, kSigmaBytes);
    if (!ok) return 31;
    ICPU_RUN_KF(enc_prep_cbd_real, stubBlockDim, dst, sigma, kMatvecK, kMatvecN, kMatvecQ, kPrepNonce0);
    ok = WriteFile("./output/dst_prep.bin", dst, prepOutBytes);
    if (!ok) return 32;
    std::memcpy(src, dst, prepOutBytes);
    std::printf("[EP02] CPU L1 Prep done\n");

    // -------- L2 NTT --------
    std::printf("[EP02] CPU L2 NTT begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 10;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_ntt.bin", dst, dstFileSize);
    if (!ok) return 11;
    std::memcpy(sHat, dst, sHatBytes);
    std::printf("[EP02] CPU L2 NTT done\n");

    // -------- L3a Matvec（Âᵀ）--------
    std::printf("[EP02] CPU L3a Matvec(Âᵀ∘ŷ) begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammas, gammaBytes);
    if (!ok) return 44;
    ICPU_RUN_KF(enc_matvec_real, stubBlockDim, dst, aHatT, sHat, gammas, kMatvecK, kMatvecN,
                kMatvecQ);
    ok = WriteFile("./output/dst_matvec.bin", dst, tHatBytes);
    if (!ok) return 42;
    std::memcpy(src, dst, tHatBytes); // û → INTT src
    std::printf("[EP02] CPU L3a Matvec done\n");

    // -------- L3b Dot --------
    std::printf("[EP02] CPU L3b Dot(⟨t̂,ŷ⟩) begin\n");
    size_t tPubRead = tHatBytes;
    ok = ReadFile("./input/t_hat.bin", tPubRead, tPub, tHatBytes);
    if (!ok) return 71;
    ICPU_RUN_KF(enc_dot_real, stubBlockDim, vHat, tPub, sHat, gammas, kMatvecK, kMatvecN, kMatvecQ);
    ok = WriteFile("./output/dst_dot.bin", vHat, vHatBytes);
    if (!ok) return 72;
    std::printf("[EP02] CPU L3b Dot done\n");

    // -------- L4a INTT(û) --------
    std::printf("[EP02] CPU L4a INTT(û) begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 20;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_intt_u.bin", dst, dstFileSize);
    if (!ok) return 21;
    std::memcpy(uNoisy, dst, packUBytes);
    std::printf("[EP02] CPU L4a INTT(û) done\n");

    // -------- L4b INTT(v̂)（垫成 K poly）--------
    std::printf("[EP02] CPU L4b INTT(v̂ pad) begin\n");
    std::memset(src, 0, srcFileSize);
    std::memcpy(src, vHat, vHatBytes);
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_intt_v_pad.bin", dst, dstFileSize);
    if (!ok) return 22;
    std::memcpy(vNoisy, dst, packVBytes); // 取 poly0
    ok = WriteFile("./output/dst_intt_v.bin", vNoisy, packVBytes);
    if (!ok) return 23;
    std::printf("[EP02] CPU L4b INTT(v̂) done\n");

    // -------- Host 加噪：u+=e1；v+=e2+μ --------
    std::printf("[EP02] CPU Host add e1/e2/μ\n");
    size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
    ok = ReadFile("./input/e1.bin", e1Read, e1, packUBytes);
    if (!ok) return 81;
    ok = ReadFile("./input/e2.bin", e2Read, e2, packVBytes);
    if (!ok) return 82;
    ok = ReadFile("./input/mu.bin", muRead, mu, packVBytes);
    if (!ok) return 83;
    HostAddModQ(reinterpret_cast<int32_t *>(uNoisy), reinterpret_cast<const int32_t *>(uNoisy),
                reinterpret_cast<const int32_t *>(e1),
                static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN), kMatvecQ);
    HostAddModQ(reinterpret_cast<int32_t *>(vNoisy), reinterpret_cast<const int32_t *>(vNoisy),
                reinterpret_cast<const int32_t *>(e2), static_cast<size_t>(kMatvecN), kMatvecQ);
    HostAddModQ(reinterpret_cast<int32_t *>(vNoisy), reinterpret_cast<const int32_t *>(vNoisy),
                reinterpret_cast<const int32_t *>(mu), static_cast<size_t>(kMatvecN), kMatvecQ);
    ok = WriteFile("./output/dst_u_noisy.bin", uNoisy, packUBytes);
    if (!ok) return 84;
    ok = WriteFile("./output/dst_v_noisy.bin", vNoisy, packVBytes);
    if (!ok) return 85;

    // -------- L5 Pack --------
    std::printf("[EP02] CPU L5 Pack begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    std::memcpy(src, uNoisy, packUBytes);
    ICPU_RUN_KF(enc_pack_compress_real, stubBlockDim, dst, src, vNoisy, kMatvecK, kMatvecN,
                kMatvecQ);
    ok = WriteFile("./output/dst_pack.bin", dst, kCipherBytes);
    if (!ok) return 52;
    ok = WriteFile("./output/c.bin", dst, kCipherBytes);
    if (!ok) return 53;
    std::printf("[EP02] CPU L5 Pack done (c=%zu B)\n", kCipherBytes);

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
        std::printf("[EP02] FATAL size mismatch\n");
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

    // -------- Host Encaps 头：H/G→K --------
    std::printf("[EP02] SIM Host H/G begin\n");
    {
        uint8_t *ekTmp, *mTmp, *hTmp, *kTmp, *rTmp;
        CHECK_ACL(aclrtMallocHost((void **)(&ekTmp), kCipherBytes));
        CHECK_ACL(aclrtMallocHost((void **)(&mTmp), 32));
        CHECK_ACL(aclrtMallocHost((void **)(&hTmp), 32));
        CHECK_ACL(aclrtMallocHost((void **)(&kTmp), 32));
        CHECK_ACL(aclrtMallocHost((void **)(&rTmp), 32));
        size_t ekR = kCipherBytes, mR = 32;
        ok = ReadFile("./input/ek_kem.bin", ekR, ekTmp, kCipherBytes);
        if (!ok) return 91;
        ok = ReadFile("./input/m.bin", mR, mTmp, 32);
        if (!ok) return 92;
        host_hg::H_Sha3_256(hTmp, ekTmp, kCipherBytes);
        host_hg::G_Sha3_512(kTmp, rTmp, mTmp, hTmp);
        ok = WriteFile("./output/K.bin", kTmp, 32);
        if (!ok) return 93;
        ok = WriteFile("./output/r_host.bin", rTmp, 32);
        if (!ok) return 94;
        CHECK_ACL(aclrtFreeHost(ekTmp));
        CHECK_ACL(aclrtFreeHost(mTmp));
        CHECK_ACL(aclrtFreeHost(hTmp));
        CHECK_ACL(aclrtFreeHost(kTmp));
        CHECK_ACL(aclrtFreeHost(rTmp));
    }
    std::printf("[EP02] SIM Host H/G done\n");

    // L0
    std::printf("[EP02] SIM L0 SampleNTT begin\n");
    size_t rhoRead = kRhoBytes;
    ok = ReadFile("./input/rho.bin", rhoRead, rhoHost, kRhoBytes);
    if (!ok) return 61;
    CHECK_ACL(aclrtMemcpy(rhoDevice, kRhoBytes, rhoHost, kRhoBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_samplentt_real)
    (stubBlockDim, stream, aHatDevice, rhoDevice, kMatvecK, kMatvecN, kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(aHatHost, aHatBytes, aHatDevice, aHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_a_hat.bin", aHatHost, aHatBytes);
    if (!ok) return 62;
    HostTransposeAHat(reinterpret_cast<int32_t *>(aHatTHost),
                      reinterpret_cast<const int32_t *>(aHatHost), kMatvecK, kMatvecN);
    CHECK_ACL(aclrtMemcpy(aHatTDevice, aHatBytes, aHatTHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = WriteFile("./output/dst_a_hat_T.bin", aHatTHost, aHatBytes);
    if (!ok) return 63;
    std::printf("[EP02] SIM L0 SampleNTT+Host Âᵀ done\n");

    // L1
    std::printf("[EP02] SIM L1 Prep begin\n");
    size_t sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigmaHost, kSigmaBytes);
    if (!ok) return 31;
    CHECK_ACL(aclrtMemcpy(sigmaDevice, kSigmaBytes, sigmaHost, kSigmaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_prep_cbd_real)
    (stubBlockDim, stream, dstDevice, sigmaDevice, kMatvecK, kMatvecN, kMatvecQ, kPrepNonce0);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, prepOutBytes, dstDevice, prepOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_prep.bin", dstHost, prepOutBytes);
    if (!ok) return 32;
    CHECK_ACL(aclrtMemcpy(srcDevice, prepOutBytes, dstDevice, prepOutBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EP02] SIM L1 Prep done\n");

    // L2
    std::printf("[EP02] SIM L2 NTT begin\n");
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 10;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_ntt.bin", dstHost, dstFileSize);
    if (!ok) return 11;
    CHECK_ACL(aclrtMemcpy(sHatDevice, sHatBytes, dstDevice, sHatBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EP02] SIM L2 NTT done\n");

    // L3a
    std::printf("[EP02] SIM L3a Matvec begin\n");
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammaHost, gammaBytes);
    if (!ok) return 44;
    CHECK_ACL(aclrtMemcpy(gammaDevice, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_matvec_real)
    (stubBlockDim, stream, dstDevice, aHatTDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN,
     kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, tHatBytes, dstDevice, tHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_matvec.bin", dstHost, tHatBytes);
    if (!ok) return 42;
    CHECK_ACL(aclrtMemcpy(srcDevice, tHatBytes, dstDevice, tHatBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EP02] SIM L3a Matvec done\n");

    // L3b
    std::printf("[EP02] SIM L3b Dot begin\n");
    size_t tPubRead = tHatBytes;
    ok = ReadFile("./input/t_hat.bin", tPubRead, tPubHost, tHatBytes);
    if (!ok) return 71;
    CHECK_ACL(aclrtMemcpy(tPubDevice, tHatBytes, tPubHost, tHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_dot_real)
    (stubBlockDim, stream, vHatDevice, tPubDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN,
     kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(vHatHost, vHatBytes, vHatDevice, vHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_dot.bin", vHatHost, vHatBytes);
    if (!ok) return 72;
    std::printf("[EP02] SIM L3b Dot done\n");

    // L4a
    std::printf("[EP02] SIM L4a INTT(û) begin\n");
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 20;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_intt_u.bin", dstHost, dstFileSize);
    if (!ok) return 21;
    std::memcpy(uHost, dstHost, packUBytes);
    std::printf("[EP02] SIM L4a INTT done\n");

    // L4b pad INTT(v̂)
    std::printf("[EP02] SIM L4b INTT(v̂ pad) begin\n");
    std::memset(srcHost, 0, srcFileSize);
    std::memcpy(srcHost, vHatHost, vHatBytes);
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_intt_v_pad.bin", dstHost, dstFileSize);
    if (!ok) return 22;
    std::memcpy(vHost, dstHost, packVBytes);
    ok = WriteFile("./output/dst_intt_v.bin", vHost, packVBytes);
    if (!ok) return 23;
    std::printf("[EP02] SIM L4b INTT done\n");

    // Host 加噪
    std::printf("[EP02] SIM Host add e1/e2/μ\n");
    size_t e1Read = packUBytes, e2Read = packVBytes, muRead = packVBytes;
    ok = ReadFile("./input/e1.bin", e1Read, e1Host, packUBytes);
    if (!ok) return 81;
    ok = ReadFile("./input/e2.bin", e2Read, e2Host, packVBytes);
    if (!ok) return 82;
    ok = ReadFile("./input/mu.bin", muRead, muHost, packVBytes);
    if (!ok) return 83;
    HostAddModQ(reinterpret_cast<int32_t *>(uHost), reinterpret_cast<const int32_t *>(uHost),
                reinterpret_cast<const int32_t *>(e1Host),
                static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN), kMatvecQ);
    HostAddModQ(reinterpret_cast<int32_t *>(vHost), reinterpret_cast<const int32_t *>(vHost),
                reinterpret_cast<const int32_t *>(e2Host), static_cast<size_t>(kMatvecN), kMatvecQ);
    HostAddModQ(reinterpret_cast<int32_t *>(vHost), reinterpret_cast<const int32_t *>(vHost),
                reinterpret_cast<const int32_t *>(muHost), static_cast<size_t>(kMatvecN), kMatvecQ);
    ok = WriteFile("./output/dst_u_noisy.bin", uHost, packUBytes);
    if (!ok) return 84;
    ok = WriteFile("./output/dst_v_noisy.bin", vHost, packVBytes);
    if (!ok) return 85;
    CHECK_ACL(aclrtMemcpy(uDevice, packUBytes, uHost, packUBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDevice, packVBytes, vHost, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // L5
    std::printf("[EP02] SIM L5 Pack begin\n");
    ACLRT_LAUNCH_KERNEL(enc_pack_compress_real)
    (stubBlockDim, stream, dstDevice, uDevice, vDevice, kMatvecK, kMatvecN, kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, kCipherBytes, dstDevice, kCipherBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_pack.bin", dstHost, kCipherBytes);
    if (!ok) return 52;
    ok = WriteFile("./output/c.bin", dstHost, kCipherBytes);
    if (!ok) return 53;
    std::printf("[EP02] SIM L5 Pack done\n");

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
    std::printf("[EP02] segments finished OK (Encaps→Encrypt pipeline)\n");
    return 0;
}
