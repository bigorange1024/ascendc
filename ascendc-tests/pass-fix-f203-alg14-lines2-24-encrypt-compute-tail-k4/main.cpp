/**
 * @file main.cpp
 * @brief pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4 — host 编排（Alg.14 行 2/16–24）。
 *
 * **定位**：prep 行 3–15 外素材已由 `input/` 提供；本程序验收 compute + tail pack → **c = c₁‖c₂**（1568B）。
 *
 * | 模式 | launch | 产出 |
 * |------|--------|------|
 * | SIM  | **1×** `f203_encrypt_l18_l19`（e₂+=μ + compute + 内联 pack） | u, v, c 全设备 |
 * | CPU  | 3× compute MIX + 1× `f203_encrypt_alg14_pack` | u/c 设备；v=golden |
 *
 * 对齐文档：docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md
 */
#include "ascendc_build_mode.hpp"
#include "data_utils.h"
#include "f203_encrypt_compute_tail_layout.h"
#include "f203_encrypt_tail_layout.h"
#include "f203_l18_l19_tiling.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <thread>

// 运行时 tiling 生成（见 f203_encrypt_tiling.cpp）；替代旧 Python input/tiling.bin。
extern void GenerateTiling(TilingData &data);

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_at_jp.h"
#include "aclrtlaunch_f203_encrypt_intt_e1.h"
#include "aclrtlaunch_f203_encrypt_l18_l19.h"
#include "aclrtlaunch_f203_encrypt_ntt_y.h"
#else
#include "tikicpulib.h"
#include "alg11_gammas.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat);
extern "C" void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_alg14_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm);
extern volatile int g_f203_ntt_y_mix_pass;
#endif

// NTT LUT 装入 ws（CPU/SIM 共用；写 ws 内 LUT_NTT_* 段）。
static bool LoadNttLutHost(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_ntt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_ntt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

#ifdef ASCENDC_CPU_DEBUG
// CPU 三 launch：INTT 段复用 LUT_NTT_* 区（phased，与 compute 探针 RunCpuThreeLaunch 一致）。
static bool LoadInttLutHostPhased(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

// CPU 固定三 launch（tikicpu MIX 串行 → 单 launch 死锁）；只产设备 u，v 由 golden 补。
static int32_t RunCpuThreeLaunch(TilingData tilingHost, uint8_t *uOut, uint8_t *ySrc, uint8_t *yHat, uint8_t *uNtt,
                                 uint8_t *aHat, uint8_t *e1, uint8_t *ws, size_t lutBytes)
{
    std::memset(ws, 0, tiling::wssize);
    if (!LoadNttLutHost(ws, lutBytes)) {
        return 12;
    }
    g_f203_ntt_y_mix_pass = tilingHost.mixPass;
    ICPU_RUN_KF(f203_encrypt_ntt_y, 1, yHat, ySrc, ws, tilingHost);

    ICPU_RUN_KF(f203_encrypt_at_jp, 2, uNtt, aHat, yHat);

    std::memset(ws, 0, tiling::wssize);
    if (!LoadInttLutHostPhased(ws, lutBytes)) {
        return 15;
    }
    ICPU_RUN_KF(f203_encrypt_intt_e1, 1, uOut, uNtt, e1, ws, tilingHost);
    return 0;
}
#else
static bool LoadInttLutHostFused(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_INTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_INTT_ODD_STACKED, lutBytes);
}

static int32_t RunSimFusedLaunch(aclrtStream stream, TilingData *tilingHost, uint8_t *uDev, uint8_t *vDev, uint8_t *cDev,
                                 uint8_t *yDev, uint8_t *yHatDev, uint8_t *uNttDev, uint8_t *uTrDev, uint8_t *aHatDev,
                                 uint8_t *mDev, uint8_t *e1Dev, uint8_t *e2Dev, uint8_t *ekPkeDev, uint8_t *tHatDev,
                                 uint8_t *trHatNttDev, uint8_t *wsDev, uint8_t *wsHost, size_t lutBytes)
{
    std::memset(wsHost, 0, tiling::wssize);
    if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
        return 20;
    }
    CHECK_ACL(aclrtMemcpy(wsDev, tiling::wssize, wsHost, tiling::wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    std::fprintf(stderr, "[ect] launch 1 f203_encrypt_l18_l19 (compute + inline tail pack -> c)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, mDev, e1Dev, e2Dev, wsDev, tilingHost, cDev,
                                              nullptr);
    return static_cast<int32_t>(aclrtSynchronizeStream(stream));
}
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t cBytes = F203_TAIL_C_BYTES;

    // 运行时生成 tiling（模板风格；数值集中于 f203_encrypt_tiling.cpp）
    TilingData tilingHost{};
    GenerateTiling(tilingHost);

    const size_t uSize = tiling::uFileBytes;
    const size_t vSize = tiling::vFileBytes;

#ifdef ASCENDC_CPU_DEBUG
    const size_t ySize = tiling::yFileBytes;
    const size_t yHatSize = tiling::yHatFileBytes;
    const size_t aHatSize = tiling::aHatFileBytes;
    const size_t uNttSize = tiling::uNttFileBytes;
    const size_t e1Size = tiling::e1FileBytes;
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;

    // ---- compute 三 launch 缓冲 ----
    uint8_t *uOut = (uint8_t *)AscendC::GmAlloc(uSize);
    uint8_t *vOut = (uint8_t *)AscendC::GmAlloc(vSize);
    uint8_t *cOut = (uint8_t *)AscendC::GmAlloc(cBytes);
    uint8_t *ySrc = (uint8_t *)AscendC::GmAlloc(ySize);
    uint8_t *yHat = (uint8_t *)AscendC::GmAlloc(yHatSize);
    uint8_t *uNtt = (uint8_t *)AscendC::GmAlloc(uNttSize);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatSize);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(e1Size);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsSize);

    size_t rs = 0;
    if (!ReadFile("./input/y.bin", rs, ySrc, ySize)) {
        return 9;
    }
    if (!ReadFile("./input/a_hat.bin", rs, aHat, aHatSize)) {
        return 10;
    }
    if (!ReadFile("./input/e1.bin", rs, e1, e1Size)) {
        return 11;
    }

    // Launch 1-3：ntt_y → at_jp → intt_e1，MIX 串行产设备 u（compute 探针 CPU 模式）
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    int32_t rc = RunCpuThreeLaunch(tilingHost, uOut, ySrc, yHat, uNtt, aHat, e1, ws, lutBytes);
    if (rc != 0) {
        return rc;
    }

    // v 由 golden 补：CPU 三 launch 无 k=8 INTT，不产 v（已含 μ+e₂，语义见 SIM 融合核）
    rs = vSize;
    if (!ReadFile("./output/golden_v.bin", rs, vOut, vSize)) {
        return 17;
    }

    // Launch 4：tail pack（Compress+ByteEncode），产密文 c
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_encrypt_alg14_pack, 1, uOut, vOut, cOut);

    if (!WriteFile("./output/u.bin", uOut, uSize)) {
        return 4;
    }
    if (!WriteFile("./output/v.bin", vOut, vSize)) {
        return 7;
    }
    if (!WriteFile("./output/c.bin", cOut, cBytes)) {
        return 20;
    }

    AscendC::GmFree(uOut);
    AscendC::GmFree(vOut);
    AscendC::GmFree(cOut);
    AscendC::GmFree(ySrc);
    AscendC::GmFree(yHat);
    AscendC::GmFree(uNtt);
    AscendC::GmFree(aHat);
    AscendC::GmFree(e1);
    AscendC::GmFree(ws);
#else
    constexpr size_t mBytes = F203_TAIL_MSG_BYTES;
    const size_t ySize = tiling::yFileBytes;
    const size_t yHatSize = tiling::yHatFileBytes;
    const size_t aHatSize = tiling::aHatFileBytes;
    const size_t uNttSize = tiling::uNttFileBytes;
    const size_t e1Size = tiling::e1FileBytes;
    const size_t e2Size = tiling::e2FileBytes;
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;
    const size_t ekPkeSize = 4 * 384;
    const size_t uTrSize = tiling::uTrFileBytes;
    const size_t trHatNttSize = tiling::n * sizeof(int32_t);

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    constexpr size_t tilingSize = 64;
    TilingData *tilingPinned = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPinned), tilingSize));
    std::memcpy(tilingPinned, &tilingHost, sizeof(TilingData));

    uint8_t *mHost = nullptr;
    uint8_t *yHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *ekPkeHost = nullptr;
    uint8_t *e1Host = nullptr;
    uint8_t *e2Host = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *uHost = nullptr;
    uint8_t *vHost = nullptr;
    uint8_t *cHost = nullptr;

    uint8_t *mDev = nullptr;
    uint8_t *yDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *ekPkeDev = nullptr;
    uint8_t *e1Dev = nullptr;
    uint8_t *e2Dev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *yHatDev = nullptr;
    uint8_t *uNttDev = nullptr;
    uint8_t *uTrDev = nullptr;
    uint8_t *trHatNttDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *cDev = nullptr;
    uint8_t *tHatDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), mBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHost), ySize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), aHatSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), ekPkeSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e1Host), e1Size));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e2Host), e2Size));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHost), uSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&vHost), vSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), cBytes));

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yDev), ySize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), aHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), ekPkeSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&e1Dev), e1Size, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&e2Dev), e2Size, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yHatDev), yHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uNttDev), uNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uTrDev), uTrSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&trHatNttDev), trHatNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), uSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), vSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), cBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mHost, mBytes) || rs != mBytes) {
        return 8;
    }
    if (!ReadFile("./input/y.bin", rs, yHost, ySize)) {
        return 9;
    }
    if (!ReadFile("./input/a_hat.bin", rs, aHatHost, aHatSize)) {
        return 10;
    }
    if (!ReadFile("./input/e1.bin", rs, e1Host, e1Size)) {
        return 11;
    }
    if (!ReadFile("./input/e2.bin", rs, e2Host, e2Size)) {
        return 14;
    }
    if (!ReadFile("./input/ek_pke.bin", rs, ekPkeHost, ekPkeSize)) {
        return 13;
    }

    CHECK_ACL(aclrtMemcpy(mDev, mBytes, mHost, mBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(yDev, ySize, yHost, ySize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(aHatDev, aHatSize, aHatHost, aHatSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekPkeDev, ekPkeSize, ekPkeHost, ekPkeSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e1Dev, e1Size, e1Host, e1Size, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e2Dev, e2Size, e2Host, e2Size, ACL_MEMCPY_HOST_TO_DEVICE));

    int32_t rc = RunSimFusedLaunch(stream, tilingPinned, uDev, vDev, cDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, mDev,
                                   e1Dev, e2Dev, ekPkeDev, tHatDev, trHatNttDev, wsDev, wsHost, lutBytes);
    if (rc != 0) {
        return rc;
    }

    CHECK_ACL(aclrtMemcpy(uHost, uSize, uDev, uSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, vSize, vDev, vSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cHost, cBytes, cDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/u.bin", uHost, uSize)) {
        return 4;
    }
    if (!WriteFile("./output/v.bin", vHost, vSize)) {
        return 7;
    }
    if (!WriteFile("./output/c.bin", cHost, cBytes)) {
        return 20;
    }

    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(yDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFree(e2Dev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(yHatDev));
    CHECK_ACL(aclrtFree(uNttDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(trHatNttDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(yHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(e1Host));
    CHECK_ACL(aclrtFreeHost(e2Host));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
