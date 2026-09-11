/**
 * EN03-encrypt-host-skel · Encrypt 形 Host 五段编排
 *
 * 段序（同进程、同 session；NTT/INTT 独立 launch，禁融胖 MIX）：
 *   L1 Prep   — enc_prep_stub   （AIV 轻桩）
 *   L2 NTT    — mmad_custom     （EN02 正向积木 + M4_ntt）
 *   L3 Matvec — enc_matvec_stub （AIV 有界 Vec MAC）
 *   L4 INTT   — mmad_custom     （换 M4_intt）
 *   L5 Pack   — enc_pack_stub   （AIV 轻桩）
 * 主门禁：五段均正常返回（不挂）。CPU/SIM；禁 npu。
 */
#include "data_utils.h"
#include "tiling.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include "aclrtlaunch_enc_prep_stub.h"
#include "aclrtlaunch_enc_matvec_stub.h"
#include "aclrtlaunch_enc_pack_stub.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" void enc_prep_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem);
extern "C" void enc_matvec_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem, int32_t scale, int32_t bias);
extern "C" void enc_pack_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem, int32_t shift_bits);
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
    // 桩段标量：Matvec MAC scale/bias；Pack 右移位数（外形，非正确性）
    constexpr int32_t kMatvecScale = 1;
    constexpr int32_t kMatvecBias = 0;
    constexpr int32_t kPackShift = 0;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize != sizeof(TilingData)) return 8;
    TilingData *tiling = (TilingData *)tiling_data;

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    const int32_t nElem = static_cast<int32_t>(srcFileSize / sizeof(int32_t));

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(std::max(dstFileSize, (size_t)1024));
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, (size_t)1024));
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));

    // -------- L1 Prep 桩 --------
    std::printf("[EN03] CPU L1 Prep begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ok = ReadFile("./input/src_prep.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 31;
    ICPU_RUN_KF(enc_prep_stub, stubBlockDim, dst, src, nElem);
    ok = WriteFile("./output/dst_prep.bin", dst, dstFileSize);
    if (!ok) return 32;
    std::printf("[EN03] CPU L1 Prep done\n");

    // -------- L2 NTT 真积木 --------
    std::printf("[EN03] CPU L2 NTT begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/src_ntt.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 9;
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 10;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_ntt.bin", dst, dstFileSize);
    if (!ok) return 11;
    std::printf("[EN03] CPU L2 NTT done\n");

    // -------- L3 Matvec 桩 --------
    std::printf("[EN03] CPU L3 Matvec begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ok = ReadFile("./input/src_matvec.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 41;
    ICPU_RUN_KF(enc_matvec_stub, stubBlockDim, dst, src, nElem, kMatvecScale, kMatvecBias);
    ok = WriteFile("./output/dst_matvec.bin", dst, dstFileSize);
    if (!ok) return 42;
    std::printf("[EN03] CPU L3 Matvec done\n");

    // -------- L4 INTT 真积木 --------
    std::printf("[EN03] CPU L4 INTT begin\n");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ok = ReadFile("./input/src_intt.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 19;
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, ws + 0, matMFileSize);
    if (!ok) return 20;
    ICPU_RUN_KF(mmad_custom, mixBlockDim, dst, src, ws, *tiling);
    ok = WriteFile("./output/dst_intt.bin", dst, dstFileSize);
    if (!ok) return 21;
    std::printf("[EN03] CPU L4 INTT done\n");

    // -------- L5 Pack 桩 --------
    std::printf("[EN03] CPU L5 Pack begin\n");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ok = ReadFile("./input/src_pack.bin", srcFileSize, src, srcFileSize);
    if (!ok) return 51;
    ICPU_RUN_KF(enc_pack_stub, stubBlockDim, dst, src, nElem, kPackShift);
    ok = WriteFile("./output/dst_pack.bin", dst, dstFileSize);
    if (!ok) return 52;
    std::printf("[EN03] CPU L5 Pack done\n");

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    size_t srcFileSize = 0, dstFileSize = 0, wsFileSize = 0;
    ResolveNttSizes(tiling, &srcFileSize, &dstFileSize, &wsFileSize);
    const int32_t nElem = static_cast<int32_t>(srcFileSize / sizeof(int32_t));

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    // -------- L1 Prep 桩（同 stream；桩 SIM 为 MIX 占位但 Host blockDim=1）--------
    std::printf("[EN03] SIM L1 Prep begin\n");
    ok = ReadFile("./input/src_prep.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 31;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_prep_stub)(stubBlockDim, stream, dstDevice, srcDevice, nElem);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_prep.bin", dstHost, dstFileSize);
    if (!ok) return 32;
    std::printf("[EN03] SIM L1 Prep done\n");

    // -------- L2 NTT --------
    std::printf("[EN03] SIM L2 NTT begin\n");
    ok = ReadFile("./input/src_ntt.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 9;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/M4_ntt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 10;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_ntt.bin", dstHost, dstFileSize);
    if (!ok) return 11;
    std::printf("[EN03] SIM L2 NTT done\n");

    // -------- L3 Matvec 桩（NTT 之后；不 recreate stream，禁 B3）--------
    std::printf("[EN03] SIM L3 Matvec begin\n");
    ok = ReadFile("./input/src_matvec.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 41;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_matvec_stub)
    (stubBlockDim, stream, dstDevice, srcDevice, nElem, kMatvecScale, kMatvecBias);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_matvec.bin", dstHost, dstFileSize);
    if (!ok) return 42;
    std::printf("[EN03] SIM L3 Matvec done\n");

    // -------- L4 INTT --------
    std::printf("[EN03] SIM L4 INTT begin\n");
    ok = ReadFile("./input/src_intt.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 19;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/M4_intt.bin", matMFileSize, wsHost, matMFileSize);
    if (!ok) return 20;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(mixBlockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_intt.bin", dstHost, dstFileSize);
    if (!ok) return 21;
    std::printf("[EN03] SIM L4 INTT done\n");

    // -------- L5 Pack 桩 --------
    std::printf("[EN03] SIM L5 Pack begin\n");
    ok = ReadFile("./input/src_pack.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) return 51;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_pack_stub)(stubBlockDim, stream, dstDevice, srcDevice, nElem, kPackShift);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_pack.bin", dstHost, dstFileSize);
    if (!ok) return 52;
    std::printf("[EN03] SIM L5 Pack done\n");

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EN03] five segments finished OK\n");
    return 0;
}
