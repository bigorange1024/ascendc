/**
 * EN07-pipeline-wired · Encrypt 形 Host 五段 + 段间真数据贯通
 *
 * 段序（同进程、同 session；五段各自独立 launch，禁融胖 MIX GATE）：
 *   L1 Prep   — enc_prep_cbd_real        （σ → y[4,256]）
 *   L2 NTT    — mmad_custom + M4_ntt     （输入 = L1 的 y，非独立随机）
 *   L3 Matvec — enc_matvec_real          （ŝ/ŷ = L2 输出；Â/γ 仍 Host）
 *   L4 INTT   — mmad_custom + M4_intt    （输入 = L3 的 û/t̂）
 *   L5 Pack   — enc_pack_compress_real   （u = L4 输出；v 仍 Host 噪声形）
 *
 * 贯通：Prep→NTT→Matvec→INTT→Pack(u)；Â/SampleNTT/噪声 v 本刀仍 Host。
 * 主门禁：五段均正常返回（不挂）。CPU/SIM；禁 npu。
 */
#include "data_utils.h"
#include "tiling.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
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

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
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
    const size_t packUBytes = sHatBytes; // u [K·N]
    const size_t packVBytes = static_cast<size_t>(kMatvecN) * sizeof(int32_t);
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    // bench=K=4：NTT/INTT 张量与 Prep/Matvec/Pack(u) 同字节数
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN07] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
                    prepOutBytes, tHatBytes);
        return 7;
    }

    const size_t hostScratch = std::max(
        dstFileSize, std::max(tHatBytes, std::max(aHatBytes, std::max(kCipherBytes, (size_t)1024))));
    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(hostScratch);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, std::max(aHatBytes, packUBytes)));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));
    uint8_t *sHat = (uint8_t *)AscendC::GmAlloc(sHatBytes); // 贯通：存 L2 ŷ 供 L3
    uint8_t *gammas = (uint8_t *)AscendC::GmAlloc(gammaBytes);
    uint8_t *sigma = (uint8_t *)AscendC::GmAlloc(kSigmaBytes);
    uint8_t *vPack = (uint8_t *)AscendC::GmAlloc(packVBytes);

    // -------- L1 Prep 真 CBD（σ → y[4,256]）--------
    std::printf("[EN07] CPU L1 Prep(CBD η=2) begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t sigmaRead = kSigmaBytes;
    ok = ReadFile("./input/sigma.bin", sigmaRead, sigma, kSigmaBytes);
    if (!ok) return 31;
    ICPU_RUN_KF(enc_prep_cbd_real, stubBlockDim, dst, sigma, kMatvecK, kMatvecN, kMatvecQ, kPrepNonce0);
    ok = WriteFile("./output/dst_prep.bin", dst, prepOutBytes);
    if (!ok) return 32;
    // 贯通：y → NTT src（同布局 memcpy，非重新随机造数）
    std::memcpy(src, dst, prepOutBytes);
    ok = WriteFile("./output/prep_ntt_layout.bin", src, prepOutBytes);
    if (!ok) return 33;
    std::printf("[EN07] CPU L1 Prep done; wired y→NTT src (elems=%zu)\n",
                prepOutBytes / sizeof(int32_t));

    // -------- L2 NTT 真积木（src = Prep y）--------
    std::printf("[EN07] CPU L2 NTT begin (src=Prep.y)\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 10;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_ntt.bin", dst, dstFileSize);
    if (!ok) return 11;
    // 贯通：ŷ → Matvec ŝ
    std::memcpy(sHat, dst, sHatBytes);
    std::printf("[EN07] CPU L2 NTT done; wired ŷ→Matvec.s_hat\n");

    // -------- L3 Matvec 真积木（ŝ=ŷ；Â/γ Host）--------
    std::printf("[EN07] CPU L3 Matvec begin (s_hat=NTT.ŷ; A/γ Host)\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t aHatRead = aHatBytes;
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/a_hat.bin", aHatRead, src, aHatBytes);
    if (!ok) return 41;
    ok = ReadFile("./input/gammas.bin", gammaRead, gammas, gammaBytes);
    if (!ok) return 44;
    ICPU_RUN_KF(enc_matvec_real, stubBlockDim, dst, src, sHat, gammas, kMatvecK, kMatvecN, kMatvecQ);
    ok = WriteFile("./output/dst_matvec.bin", dst, tHatBytes);
    if (!ok) return 42;
    // 贯通：t̂/û → INTT src
    std::memcpy(src, dst, tHatBytes);
    std::printf("[EN07] CPU L3 Matvec done; wired t̂→INTT src\n");

    // -------- L4 INTT 真积木（src = Matvec t̂）--------
    std::printf("[EN07] CPU L4 INTT begin (src=Matvec.t_hat)\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 20;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_intt.bin", dst, dstFileSize);
    if (!ok) return 21;
    // 贯通：u → Pack
    std::memcpy(src, dst, packUBytes);
    std::printf("[EN07] CPU L4 INTT done; wired u→Pack.u\n");

    // -------- L5 Pack（u=INTT；v 仍 Host）--------
    std::printf("[EN07] CPU L5 Pack begin (u=INTT; v=Host)\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    size_t vRead = packVBytes;
    ok = ReadFile("./input/v_pack.bin", vRead, vPack, packVBytes);
    if (!ok) return 53;
    ICPU_RUN_KF(enc_pack_compress_real, stubBlockDim, dst, src, vPack, kMatvecK, kMatvecN, kMatvecQ);
    ok = WriteFile("./output/dst_pack.bin", dst, kCipherBytes);
    if (!ok) return 52;
    std::printf("[EN07] CPU L5 Pack done (c=%zu B)\n", kCipherBytes);

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)sHat);
    AscendC::GmFree((void *)gammas);
    AscendC::GmFree((void *)sigma);
    AscendC::GmFree((void *)vPack);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost, *sHatHost, *gammaHost, *sigmaHost, *vHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice, *sHatDevice, *gammaDevice, *sigmaDevice, *vDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    if (srcFileSize != prepOutBytes || srcFileSize != tHatBytes) {
        std::printf("[EN07] FATAL size mismatch src=%zu prep=%zu tHat=%zu\n", srcFileSize,
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
    CHECK_ACL(aclrtMallocHost((void **)(&gammaHost), gammaBytes));
    CHECK_ACL(aclrtMalloc((void **)&gammaDevice, gammaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&sigmaHost), kSigmaBytes));
    CHECK_ACL(aclrtMalloc((void **)&sigmaDevice, kSigmaBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), packVBytes));
    CHECK_ACL(aclrtMalloc((void **)&vDevice, packVBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    // -------- L1 Prep 真 CBD --------
    std::printf("[EN07] SIM L1 Prep(CBD η=2) begin\n");
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
    // 贯通：设备侧 y → NTT src（D2D）；Host 侧落盘对照
    CHECK_ACL(aclrtMemcpy(srcDevice, prepOutBytes, dstDevice, prepOutBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    ok = WriteFile("./output/prep_ntt_layout.bin", dstHost, prepOutBytes);
    if (!ok) return 33;
    std::printf("[EN07] SIM L1 Prep done; wired y→NTT src\n");

    // -------- L2 NTT（src = Prep y）--------
    std::printf("[EN07] SIM L2 NTT begin (src=Prep.y)\n");
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 10;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_ntt.bin", dstHost, dstFileSize);
    if (!ok) return 11;
    CHECK_ACL(aclrtMemcpy(sHatDevice, sHatBytes, dstDevice, sHatBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EN07] SIM L2 NTT done; wired ŷ→Matvec.s_hat\n");

    // -------- L3 Matvec（ŝ=ŷ；Â/γ Host）--------
    std::printf("[EN07] SIM L3 Matvec begin (s_hat=NTT.ŷ; A/γ Host)\n");
    size_t aHatRead = aHatBytes;
    size_t gammaRead = gammaBytes;
    ok = ReadFile("./input/a_hat.bin", aHatRead, srcHost, aHatBytes);
    if (!ok) return 41;
    CHECK_ACL(aclrtMemcpy(srcDevice, aHatBytes, srcHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/gammas.bin", gammaRead, gammaHost, gammaBytes);
    if (!ok) return 44;
    CHECK_ACL(aclrtMemcpy(gammaDevice, gammaBytes, gammaHost, gammaBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_matvec_real)
    (stubBlockDim, stream, dstDevice, srcDevice, sHatDevice, gammaDevice, kMatvecK, kMatvecN, kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, tHatBytes, dstDevice, tHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_matvec.bin", dstHost, tHatBytes);
    if (!ok) return 42;
    CHECK_ACL(aclrtMemcpy(srcDevice, tHatBytes, dstDevice, tHatBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EN07] SIM L3 Matvec done; wired t̂→INTT src\n");

    // -------- L4 INTT（src = Matvec t̂）--------
    std::printf("[EN07] SIM L4 INTT begin (src=Matvec.t_hat)\n");
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 20;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_intt.bin", dstHost, dstFileSize);
    if (!ok) return 21;
    CHECK_ACL(aclrtMemcpy(srcDevice, packUBytes, dstDevice, packUBytes, ACL_MEMCPY_DEVICE_TO_DEVICE));
    std::printf("[EN07] SIM L4 INTT done; wired u→Pack.u\n");

    // -------- L5 Pack（u=INTT；v Host）--------
    std::printf("[EN07] SIM L5 Pack begin (u=INTT; v=Host)\n");
    size_t vRead = packVBytes;
    ok = ReadFile("./input/v_pack.bin", vRead, vHost, packVBytes);
    if (!ok) return 53;
    CHECK_ACL(aclrtMemcpy(vDevice, packVBytes, vHost, packVBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_pack_compress_real)
    (stubBlockDim, stream, dstDevice, srcDevice, vDevice, kMatvecK, kMatvecN, kMatvecQ);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, kCipherBytes, dstDevice, kCipherBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_pack.bin", dstHost, kCipherBytes);
    if (!ok) return 52;
    std::printf("[EN07] SIM L5 Pack done\n");

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFree(sHatDevice));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFree(gammaDevice));
    CHECK_ACL(aclrtFreeHost(gammaHost));
    CHECK_ACL(aclrtFree(sigmaDevice));
    CHECK_ACL(aclrtFreeHost(sigmaHost));
    CHECK_ACL(aclrtFree(vDevice));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN07] five segments finished OK (pipeline wired)\n");
    return 0;
}
