/**
 * EN08-wired-sticky-rounds · 贯通五段粘性多轮（自 EN07 复制）
 *
 * 在同一进程、同一 acl session / 同一 stream 上，将贯通链整段重复 R 轮（默认 4，
 * 环境变量 EN08_ROUNDS 可覆盖）：
 *   for r in 1..R:
 *     Prep → NTT → Matvec → INTT → Pack
 *
 * 粘性约束：不 recreate stream、不重 aclInit；轮间可换 σ/nonce/Â/v，但仍段间贯通。
 * 正确性：第 1 轮落盘 dst_*.bin 供 golden；后续轮只要求跑完不挂。
 * 主门禁：R 轮全部完成不挂。CPU/SIM；禁 npu；禁融胖 MIX GATE。
 *
 * 对照旧 hang 线 ER05「粘性双 COMPUTE」：本刀是真积木贯通链多轮，不是空壳。
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
#include "aclrtlaunch_enc_prep_cbd_real.h"
#include "aclrtlaunch_enc_matvec_real.h"
#include "aclrtlaunch_enc_pack_compress_real.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" void enc_prep_cbd_real(GM_ADDR dst, GM_ADDR sigma, int32_t k, int32_t n, int32_t q,
                                  int32_t nonce0);
extern "C" void enc_matvec_real(GM_ADDR t_hat, GM_ADDR a_hat, GM_ADDR s_hat, GM_ADDR gammas,
                                int32_t k, int32_t n, int32_t q);
extern "C" void enc_pack_compress_real(GM_ADDR c_out, GM_ADDR u_in, GM_ADDR v_in, int32_t k,
                                       int32_t n, int32_t q);
#endif

/** 按 tiling 计算 NTT/INTT 段 src/dst/ws 字节数（与 EN02 ResolveSizes 同构）。 */
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

/** 解析 EN08_ROUNDS；非法或越界时回落默认 4。 */
static int32_t ResolveRounds()
{
    const char *env = std::getenv("EN08_ROUNDS");
    if (env == nullptr || env[0] == '\0') {
        return 4;
    }
    char *end = nullptr;
    long v = std::strtol(env, &end, 10);
    if (end == env || v < 1 || v > 64) {
        std::printf("[EN08] WARN EN08_ROUNDS='%s' invalid → default 4\n", env);
        return 4;
    }
    return static_cast<int32_t>(v);
}

/**
 * 轮间换输入：第 1 轮保持 gen_data 原样（供 golden）；r≥2 扰动 σ/Â/v。
 * 背景：任务允许轮间换 seed/输入但仍须贯通；未采用 recreate stream / 重 aclInit。
 */
static void MutateHostInputsForRound(int32_t round, uint8_t *sigma, size_t sigmaBytes,
                                     uint8_t *aHat, size_t aHatBytes, uint8_t *vPack,
                                     size_t vBytes)
{
    if (round <= 1) {
        return;
    }
    sigma[(static_cast<size_t>(round) * 7U) % sigmaBytes] ^=
        static_cast<uint8_t>(17U * static_cast<unsigned>(round));
    auto *a32 = reinterpret_cast<int32_t *>(aHat);
    const size_t aElems = aHatBytes / sizeof(int32_t);
    for (size_t i = 0; i < 8 && i < aElems; ++i) {
        a32[i] = (a32[i] + static_cast<int32_t>(round) * 13 + static_cast<int32_t>(i)) % 3329;
    }
    auto *v32 = reinterpret_cast<int32_t *>(vPack);
    const size_t vElems = vBytes / sizeof(int32_t);
    for (size_t i = 0; i < 4 && i < vElems; ++i) {
        v32[i] = (v32[i] + static_cast<int32_t>(round) * 19) % 3329;
    }
}

/** 落盘第 r 轮完成标记（供 STATUS/日志对照）。 */
static bool WriteRoundDone(int32_t round)
{
    char path[64];
    std::snprintf(path, sizeof(path), "./output/round_%02d_done.txt", round);
    char body[96];
    int n = std::snprintf(body, sizeof(body), "EN08 round %d finished OK\n", round);
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
    // 已锁：K=4、N=256、q=3329；Pack d_u=11、d_v=5 → c=1568B
    constexpr int32_t kMatvecK = 4;
    constexpr int32_t kMatvecN = 256;
    constexpr int32_t kMatvecQ = 3329;
    constexpr int32_t kPrepNonce0 = 0;
    constexpr size_t kSigmaBytes = 32;
    constexpr size_t kC1PolyBytes = 352;
    constexpr size_t kC2Bytes = 160;
    constexpr size_t kCipherBytes = kMatvecK * kC1PolyBytes + kC2Bytes; // 1568
    const size_t prepOutBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t aHatBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) *
        sizeof(int32_t);
    const size_t sHatBytes =
        static_cast<size_t>(kMatvecK) * static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    const size_t tHatBytes = sHatBytes;
    const size_t gammaBytes = (static_cast<size_t>(kMatvecN) / 2U) * sizeof(int32_t);
    const size_t packUBytes = sHatBytes;
    const size_t packVBytes = static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    bool ok;

    std::printf("[EN08] sticky wired rounds begin R=%d (no stream recreate / no re-aclInit)\n",
                rounds);

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN08] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
                    prepOutBytes, tHatBytes);
        return 7;
    }

    const size_t hostScratch = std::max(
        dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, (size_t)1024))));
    // 粘性：下列缓冲只分配一次，R 轮复用（不释放重分配）
    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(hostScratch);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, std::max(aHatBytes, packUBytes)));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));
    uint8_t *sHat = (uint8_t *)AscendC::GmAlloc(sHatBytes);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatBytes); // Â 独立缓冲，避免与 NTT src 抢占
    uint8_t *gammas = (uint8_t *)AscendC::GmAlloc(gammaBytes);
    uint8_t *sigma = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *vPack = (uint8_t *)AscendC::GmAlloc(packVBytes);
    uint8_t *sigmaBase = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *aHatBase = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *vBase = (uint8_t *)AscendC::GmAlloc(packVBytes);

    size_t sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigmaBase, kSigmaBytes);
    if (!ok) return 31;
    size_t aHatRead = aHatBytes;
    ok = ReadFile("./input/a_hat.bin", aHatRead, aHatBase, aHatBytes);
    if (!ok) return 41;
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammas, gammaBytes);
    if (!ok) return 44;
    size_t vRead = packVBytes;
    ok = ReadFile("./input/v_pack.bin", vRead, vBase, packVBytes);
    if (!ok) return 53;

    for (int32_t r = 1; r <= rounds; ++r) {
        std::printf("[EN08] CPU round %d/%d begin\n", r, rounds);
        std::memcpy(sigma, sigmaBase, kSigmaBytes);
        std::memcpy(aHat, aHatBase, aHatBytes);
        std::memcpy(vPack, vBase, packVBytes);
        MutateHostInputsForRound(r, sigma, kSigmaBytes, aHat, aHatBytes, vPack, packVBytes);
        const int32_t nonce = kPrepNonce0 + (r - 1);

        // -------- L1 Prep（σ → y）--------
        std::printf("[EN08] CPU r%d L1 Prep begin (nonce=%d)\n", r, nonce);
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        ICPU_RUN_KF(enc_prep_cbd_real, stubBlockDim, dst, sigma, kMatvecK, kMatvecN, kMatvecQ, nonce);
        if (r == 1) {
            ok = WriteFile("./output/dst_prep.bin", dst, prepOutBytes);
            if (!ok) return 32;
            ok = WriteFile("./output/prep_ntt_layout.bin", dst, prepOutBytes);
            if (!ok) return 33;
        }
        std::memcpy(src, dst, prepOutBytes); // 贯通：y → NTT src
        std::printf("[EN08] CPU r%d L1 Prep done; wired y→NTT\n", r);

        // -------- L2 NTT（src = Prep y）--------
        std::printf("[EN08] CPU r%d L2 NTT begin\n", r);
        AscendC::SetKernelMode(KernelMode::MIX_MODE);
        ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
        if (!ok) return 10;
        ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
        if (r == 1) {
            ok = WriteFile("./output/dst_ntt.bin", dst, dstFileSize);
            if (!ok) return 11;
        }
        std::memcpy(sHat, dst, sHatBytes); // 贯通：ŷ → Matvec ŝ
        std::printf("[EN08] CPU r%d L2 NTT done; wired ŷ→Matvec\n", r);

        // -------- L3 Matvec（ŝ=ŷ；Â/γ Host，Â 用独立缓冲）--------
        std::printf("[EN08] CPU r%d L3 Matvec begin\n", r);
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        ICPU_RUN_KF(enc_matvec_real, stubBlockDim, dst, aHat, sHat, gammas, kMatvecK, kMatvecN,
                    kMatvecQ);
        if (r == 1) {
            ok = WriteFile("./output/dst_matvec.bin", dst, tHatBytes);
            if (!ok) return 42;
        }
        std::memcpy(src, dst, tHatBytes); // 贯通：t̂ → INTT src
        std::printf("[EN08] CPU r%d L3 Matvec done; wired t̂→INTT\n", r);

        // -------- L4 INTT（src = Matvec t̂）--------
        std::printf("[EN08] CPU r%d L4 INTT begin\n", r);
        AscendC::SetKernelMode(KernelMode::MIX_MODE);
        ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
        if (!ok) return 20;
        ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
        if (r == 1) {
            ok = WriteFile("./output/dst_intt.bin", dst, dstFileSize);
            if (!ok) return 21;
        }
        std::memcpy(src, dst, packUBytes); // 贯通：u → Pack
        std::printf("[EN08] CPU r%d L4 INTT done; wired u→Pack\n", r);

        // -------- L5 Pack（u=INTT；v Host）--------
        std::printf("[EN08] CPU r%d L5 Pack begin\n", r);
        AscendC::SetKernelMode(KernelMode::AIV_MODE);
        ICPU_RUN_KF(enc_pack_compress_real, stubBlockDim, dst, src, vPack, kMatvecK, kMatvecN,
                    kMatvecQ);
        if (r == 1) {
            ok = WriteFile("./output/dst_pack.bin", dst, kCipherBytes);
            if (!ok) return 52;
        }
        std::printf("[EN08] CPU r%d L5 Pack done\n", r);

        if (!WriteRoundDone(r)) return 60;
        std::printf("[EN08] CPU round %d/%d finished OK\n", r, rounds);
    }

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)sHat);
    AscendC::GmFree((void *)aHat);
    AscendC::GmFree((void *)gammas);
    AscendC::GmFree((void *)sigma);
    AscendC::GmFree((void *)vPack);
    AscendC::GmFree((void *)sigmaBase);
    AscendC::GmFree((void *)aHatBase);
    AscendC::GmFree((void *)vBase);
    AscendC::GmFree((void *)tiling_data);
#else
    // 粘性：aclInit / CreateStream / 设备缓冲只做一次；R 轮循环内不 Destroy/Finalize
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost, *sHatHost, *aHatHost, *gammaHost, *sigmaHost, *vHost;
    uint8_t *sigmaBaseHost, *aHatBaseHost, *vBaseHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice, *sHatDevice, *aHatDevice, *gammaDevice, *sigmaDevice,
        *vDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN08] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
                    prepOutBytes, tHatBytes);
        return 7;
    }
    const size_t hostScratch = std::max(
        dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, packUBytes))));

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
    CHECK_ACL(aclrtMallocHost((void **)(&gammaHost), gammaBytes));
    CHECK_ACL(aclrtMalloc((void **)&gammaDevice, gammaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaHost), kSigmaBytes));
    CHECK_ACL(aclrtMalloc((void **)&sigmaDevice, kSigmaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), packVBytes));
    CHECK_ACL(aclrtMalloc((void **)&vDevice, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaBaseHost), kSigmaBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatBaseHost), aHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&vBaseHost), packVBytes));

    size_t sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigmaBaseHost, kSigmaBytes);
    if (!ok) return 31;
    size_t aHatRead = aHatBytes;
    ok = ReadFile("./input/a_hat.bin", aHatRead, aHatBaseHost, aHatBytes);
    if (!ok) return 41;
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammaHost, gammaBytes);
    if (!ok) return 44;
    CHECK_ACL(aclrtMemcpy(gammaDevice, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    size_t vRead = packVBytes;
    ok = ReadFile("./input/v_pack.bin", vRead, vBaseHost, packVBytes);
    if (!ok) return 53;

    for (int32_t r = 1; r <= rounds; ++r) {
        std::printf("[EN08] SIM round %d/%d begin (same stream)\n", r, rounds);
        std::memcpy(sigmaHost, sigmaBaseHost, kSigmaBytes);
        std::memcpy(aHatHost, aHatBaseHost, aHatBytes);
        std::memcpy(vHost, vBaseHost, packVBytes);
        MutateHostInputsForRound(r, sigmaHost, kSigmaBytes, aHatHost, aHatBytes, vHost, packVBytes);
        const int32_t nonce = kPrepNonce0 + (r - 1);

        // -------- L1 Prep --------
        std::printf("[EN08] SIM r%d L1 Prep begin (nonce=%d)\n", r, nonce);
        CHECK_ACL(aclrtMemcpy(sigmaDevice, kSigmaBytes, sigmaHost, kSigmaBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_prep_cbd_real)
        (stubBlockDim, stream, dstDevice, sigmaDevice, kMatvecK, kMatvecN, kMatvecQ, nonce);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        if (r == 1) {
            CHECK_ACL(aclrtMemcpy(dstHost, prepOutBytes, dstDevice, prepOutBytes,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            ok = WriteFile("./output/dst_prep.bin", dstHost, prepOutBytes);
            if (!ok) return 32;
            ok = WriteFile("./output/prep_ntt_layout.bin", dstHost, prepOutBytes);
            if (!ok) return 33;
        }
        // 贯通：设备侧 y → NTT src（D2D）
        CHECK_ACL(aclrtMemcpy(srcDevice, prepOutBytes, dstDevice, prepOutBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));
        std::printf("[EN08] SIM r%d L1 Prep done; wired y→NTT\n", r);

        // -------- L2 NTT --------
        std::printf("[EN08] SIM r%d L2 NTT begin\n", r);
        ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
        if (!ok) return 10;
        CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        if (r == 1) {
            CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            ok = WriteFile("./output/dst_ntt.bin", dstHost, dstFileSize);
            if (!ok) return 11;
        }
        CHECK_ACL(aclrtMemcpy(sHatDevice, sHatBytes, dstDevice, sHatBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));
        std::printf("[EN08] SIM r%d L2 NTT done; wired ŷ→Matvec\n", r);

        // -------- L3 Matvec --------
        std::printf("[EN08] SIM r%d L3 Matvec begin\n", r);
        CHECK_ACL(aclrtMemcpy(aHatDevice, aHatBytes, aHatHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_matvec_real)
        (stubBlockDim, stream, dstDevice, aHatDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN,
         kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        if (r == 1) {
            CHECK_ACL(aclrtMemcpy(dstHost, tHatBytes, dstDevice, tHatBytes,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            ok = WriteFile("./output/dst_matvec.bin", dstHost, tHatBytes);
            if (!ok) return 42;
        }
        CHECK_ACL(aclrtMemcpy(srcDevice, tHatBytes, dstDevice, tHatBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));
        std::printf("[EN08] SIM r%d L3 Matvec done; wired t̂→INTT\n", r);

        // -------- L4 INTT --------
        std::printf("[EN08] SIM r%d L4 INTT begin\n", r);
        ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
        if (!ok) return 20;
        CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        if (r == 1) {
            CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            ok = WriteFile("./output/dst_intt.bin", dstHost, dstFileSize);
            if (!ok) return 21;
        }
        CHECK_ACL(aclrtMemcpy(srcDevice, packUBytes, dstDevice, packUBytes,
                              ACL_MEMCPY_DEVICE_TO_DEVICE));
        std::printf("[EN08] SIM r%d L4 INTT done; wired u→Pack\n", r);

        // -------- L5 Pack --------
        std::printf("[EN08] SIM r%d L5 Pack begin\n", r);
        CHECK_ACL(aclrtMemcpy(vDevice, packVBytes, vHost, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        ACLRT_LAUNCH_KERNEL(enc_pack_compress_real)
        (stubBlockDim, stream, dstDevice, srcDevice, vDevice, kMatvecK, kMatvecN, kMatvecQ);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        if (r == 1) {
            CHECK_ACL(aclrtMemcpy(dstHost, kCipherBytes, dstDevice, kCipherBytes,
                                  ACL_MEMCPY_DEVICE_TO_HOST));
            ok = WriteFile("./output/dst_pack.bin", dstHost, kCipherBytes);
            if (!ok) return 52;
        }
        std::printf("[EN08] SIM r%d L5 Pack done\n", r);

        if (!WriteRoundDone(r)) return 60;
        std::printf("[EN08] SIM round %d/%d finished OK\n", r, rounds);
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
    CHECK_ACL(aclrtFree(gammaDevice));
    CHECK_ACL(aclrtFreeHost(gammaHost));
    CHECK_ACL(aclrtFree(sigmaDevice));
    CHECK_ACL(aclrtFreeHost(sigmaHost));
    CHECK_ACL(aclrtFree(vDevice));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFreeHost(sigmaBaseHost));
    CHECK_ACL(aclrtFreeHost(aHatBaseHost));
    CHECK_ACL(aclrtFreeHost(vBaseHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN08] sticky R=%d rounds finished OK (pipeline wired)\n", rounds);
    return 0;
}
