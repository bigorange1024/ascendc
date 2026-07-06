/**
 * @file main.cpp
 * @brief 行 18–19 可行性：默认 3 launch（CPU/SIM）；F203_FEAS_FUSED=1 试单 launch。
 */
#include "data_utils.h"
#include "f203_l18_l19_tiling.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <thread>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_at_jp.h"
#include "aclrtlaunch_f203_encrypt_intt_e1.h"
#include "aclrtlaunch_f203_encrypt_l18_l19.h"
#include "aclrtlaunch_f203_encrypt_ntt_y.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat);
extern "C" void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_l18_l19(GM_ADDR uOut, GM_ADDR ySrc, GM_ADDR yHat, GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR e1,
                                     GM_ADDR ws, TilingData tiling, GM_ADDR traceGm);
extern volatile int g_f203_ntt_y_mix_pass;
#endif

static bool LoadNttLutHost(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_ntt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_ntt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

static bool LoadInttLutHostPhased(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    // 三 launch：INTT 核独占 ws，LUT @ offset 0（与 kem intt / stage123 一致）。
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

#ifndef ASCENDC_CPU_DEBUG
static bool LoadInttLutHostFused(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    // 单 launch：NTT LUT @ 0，INTT LUT @ wsCoreBytes 之后（与 f203_l18_l19_tiling.h 一致）。
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_INTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_INTT_ODD_STACKED, lutBytes);
}
#endif

#ifdef ASCENDC_CPU_DEBUG
static int32_t RunPhasedCpu(TilingData tilingHost, uint8_t *uOut, uint8_t *ySrc, uint8_t *yHat, uint8_t *uNtt,
                            uint8_t *aHat, uint8_t *e1, uint8_t *ws, size_t lutBytes, bool fused)
{
    if (fused) {
        // CPU tikicpu 串行先跑 AIC，单 launch FSM 在 WAIT(IP_AIV_DONE) 死锁；仅 SIM 验收。
        std::fprintf(stderr, "[WARN] F203_FEAS_FUSED=1: CPU 路径不支持，请用 SIM\n");
        return 21;
    }

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
constexpr int32_t kFusedTraceSlots = 16;
constexpr const char *kFusedTraceNames[kFusedTraceSlots] = {
    "AIV_NTT_SPLIT",    "AIC_NTT_MMAD",     "AIV_NTT_YHAT",     "AIV_AT_JP_START", "AIV_AT_JP_DONE",
    "AIV_IP_SIGNAL",    "AIC_IP_WAIT_DONE", "AIV_INTT_SPLIT",   "AIC_INTT_MMAD",   "AIV_INTT_U",
    "AIV_E1_DONE",      "AIC_AT_JP_GATE",   "AIV_AT_JP_GATE",   nullptr,           nullptr,
};

static int32_t FusedTraceHighest(const int32_t *trace)
{
    for (int32_t i = kFusedTraceSlots - 1; i >= 0; --i) {
        if (trace[i] != 0) {
            return i;
        }
    }
    return -1;
}

static void PrintFusedTrace(const int32_t *trace, int32_t prevHigh, double elapsedSec)
{
    const int32_t high = FusedTraceHighest(trace);
    if (high <= prevHigh) {
        std::fprintf(stderr, "[fused-trace] t=%.1fs stall@%s\n", elapsedSec,
                     (prevHigh >= 0 && kFusedTraceNames[prevHigh] != nullptr) ? kFusedTraceNames[prevHigh] : "?");
        return;
    }
    std::fprintf(stderr, "[fused-trace] t=%.1fs -> %s", elapsedSec,
                 (kFusedTraceNames[high] != nullptr) ? kFusedTraceNames[high] : "?");
    for (int32_t i = prevHigh + 1; i <= high; ++i) {
        if (trace[i] != 0 && i != high && kFusedTraceNames[i] != nullptr) {
            std::fprintf(stderr, " (%s)", kFusedTraceNames[i]);
        }
    }
    std::fprintf(stderr, "\n");
}

static void CopyWsLutToDevice(uint8_t *wsDev, const uint8_t *wsHost, size_t wsSize, size_t lutBytes)
{
    CHECK_ACL(aclrtMemcpy(wsDev, wsSize, wsHost, wsSize, ACL_MEMCPY_HOST_TO_DEVICE));
    (void)lutBytes;
}

static int32_t RunPhasedSim(aclrtStream stream, TilingData *tilingHost, uint8_t *uDev, uint8_t *yDev, uint8_t *yHatDev,
                            uint8_t *uNttDev, uint8_t *aHatDev, uint8_t *eDev, uint8_t *wsDev, uint8_t *wsHost,
                            uint8_t *traceDev, int32_t *traceHost, size_t lutBytes, bool fused)
{
    if (fused) {
        std::memset(wsHost, 0, tiling::wssize);
        std::memset(traceHost, 0, kFusedTraceSlots * sizeof(int32_t));
        if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
            return 20;
        }
        CopyWsLutToDevice(wsDev, wsHost, tiling::wssize, lutBytes);
        CHECK_ACL(aclrtMemcpy(traceDev, kFusedTraceSlots * sizeof(int32_t), traceHost,
                              kFusedTraceSlots * sizeof(int32_t), ACL_MEMCPY_HOST_TO_DEVICE));

        std::fprintf(stderr, "[fused] launch f203_encrypt_l18_l19 (blockDim=1)\n");
        const auto t0 = std::chrono::steady_clock::now();
        std::atomic<bool> pollDone{false};
        int32_t prevHigh = -1;
        std::thread poller([&]() {
            while (!pollDone.load()) {
                const aclError cpRc = aclrtMemcpy(traceHost, kFusedTraceSlots * sizeof(int32_t), traceDev,
                                                  kFusedTraceSlots * sizeof(int32_t), ACL_MEMCPY_DEVICE_TO_HOST);
                if (cpRc == ACL_SUCCESS) {
                    const double sec =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                    PrintFusedTrace(traceHost, prevHigh, sec);
                    prevHigh = FusedTraceHighest(traceHost);
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });

        ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, yDev, yHatDev, uNttDev, aHatDev, eDev, wsDev,
                                                tilingHost, traceDev);
        const aclError syncRc = aclrtSynchronizeStream(stream);
        pollDone.store(true);
        poller.join();

        CHECK_ACL(aclrtMemcpy(traceHost, kFusedTraceSlots * sizeof(int32_t), traceDev,
                              kFusedTraceSlots * sizeof(int32_t), ACL_MEMCPY_DEVICE_TO_HOST));
        const double totalSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::fprintf(stderr, "[fused] trace slots:");
        for (int32_t i = 0; i < kFusedTraceSlots; ++i) {
            if (traceHost[i] != 0) {
                std::fprintf(stderr, " %d:%s", i,
                             (kFusedTraceNames[i] != nullptr) ? kFusedTraceNames[i] : "?");
            }
        }
        std::fprintf(stderr, "\n");
        std::fprintf(stderr, "[fused] sync ret=%d total=%.1fs final=%s\n", static_cast<int>(syncRc), totalSec,
                     (FusedTraceHighest(traceHost) >= 0 && kFusedTraceNames[FusedTraceHighest(traceHost)] != nullptr)
                         ? kFusedTraceNames[FusedTraceHighest(traceHost)]
                         : "none");
        CHECK_ACL(syncRc);
        return 0;
    }

    std::memset(wsHost, 0, tiling::wssize);
    if (!LoadNttLutHost(wsHost, lutBytes)) {
        return 12;
    }
    CopyWsLutToDevice(wsDev, wsHost, tiling::wssize, lutBytes);
    ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_y)(1, stream, yHatDev, yDev, wsDev, tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ACLRT_LAUNCH_KERNEL(f203_encrypt_at_jp)(2, stream, uNttDev, aHatDev, yHatDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    std::memset(wsHost, 0, tiling::wssize);
    if (!LoadInttLutHostPhased(wsHost, lutBytes)) {
        return 15;
    }
    CopyWsLutToDevice(wsDev, wsHost, tiling::wssize, lutBytes);
    ACLRT_LAUNCH_KERNEL(f203_encrypt_intt_e1)(1, stream, uDev, uNttDev, eDev, wsDev, tilingHost);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    return 0;
}
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= tilingSize, "");

    const size_t ySize = tiling::yFileBytes;
    const size_t yHatSize = tiling::yHatFileBytes;
    const size_t aHatSize = tiling::aHatFileBytes;
    const size_t uNttSize = tiling::uNttFileBytes;
    const size_t e1Size = tiling::e1FileBytes;
    const size_t uSize = tiling::uFileBytes;
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;

    TilingData tilingHost{};
    tilingHost.tileLength = static_cast<int32_t>(tiling::n);
    tilingHost.kPolys = static_cast<int32_t>(tiling::kK);
    tilingHost.mixPass = 3;

    const bool fused = std::getenv("F203_FEAS_FUSED") != nullptr;

    uint8_t tilingBuf[tilingSize] = {};
    size_t tilingRead = tilingSize;
    if (!ReadFile("./input/tiling.bin", tilingRead, tilingBuf, sizeof(tilingBuf)) ||
        tilingRead < sizeof(TilingData)) {
        return 1;
    }
    std::memcpy(&tilingHost, tilingBuf, sizeof(TilingData));

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);

    uint8_t *uOut = (uint8_t *)AscendC::GmAlloc(uSize > 1024 ? uSize : 1024);
    uint8_t *ySrc = (uint8_t *)AscendC::GmAlloc(ySize > 1024 ? ySize : 1024);
    uint8_t *yHat = (uint8_t *)AscendC::GmAlloc(yHatSize > 1024 ? yHatSize : 1024);
    uint8_t *uNtt = (uint8_t *)AscendC::GmAlloc(uNttSize > 1024 ? uNttSize : 1024);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatSize > 1024 ? aHatSize : 1024);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(e1Size > 1024 ? e1Size : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsSize > 1024 ? wsSize : 1024);

    size_t yRead = ySize;
    if (!ReadFile("./input/y.bin", yRead, ySrc, ySize)) {
        return 9;
    }
    size_t aHatRead = aHatSize;
    if (!ReadFile("./input/a_hat.bin", aHatRead, aHat, aHatSize)) {
        return 10;
    }
    size_t e1Read = e1Size;
    if (!ReadFile("./input/e1.bin", e1Read, e1, e1Size)) {
        return 11;
    }

    const int32_t rc = RunPhasedCpu(tilingHost, uOut, ySrc, yHat, uNtt, aHat, e1, ws, lutBytes, fused);
    if (rc != 0) {
        return rc;
    }

    if (!WriteFile("./output/y_hat.bin", yHat, yHatSize)) {
        return 2;
    }
    if (!WriteFile("./output/u_ntt.bin", uNtt, uNttSize)) {
        return 3;
    }
    if (!WriteFile("./output/u.bin", uOut, uSize)) {
        return 4;
    }

    AscendC::GmFree(uOut);
    AscendC::GmFree(ySrc);
    AscendC::GmFree(yHat);
    AscendC::GmFree(uNtt);
    AscendC::GmFree(aHat);
    AscendC::GmFree(e1);
    AscendC::GmFree(ws);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *yHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *e1Host = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *yHatHost = nullptr;
    uint8_t *uNttHost = nullptr;
    uint8_t *uHost = nullptr;
    TilingData *tilingPinned = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPinned), tilingSize));
    std::memcpy(tilingPinned, &tilingHost, sizeof(TilingData));

    uint8_t *yDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *e1Dev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *yHatDev = nullptr;
    uint8_t *uNttDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *traceDev = nullptr;
    int32_t *traceHost = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHost), ySize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), aHatSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e1Host), e1Size));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHatHost), yHatSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uNttHost), uNttSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHost), uSize));

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yDev), ySize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), aHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&e1Dev), e1Size, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yHatDev), yHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uNttDev), uNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), uSize, ACL_MEM_MALLOC_HUGE_FIRST));
    if (fused) {
        CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&traceHost), kFusedTraceSlots * sizeof(int32_t)));
        CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&traceDev), kFusedTraceSlots * sizeof(int32_t),
                              ACL_MEM_MALLOC_HUGE_FIRST));
    }

    size_t yRead = ySize;
    if (!ReadFile("./input/y.bin", yRead, yHost, ySize)) {
        return 9;
    }
    size_t aHatRead = aHatSize;
    if (!ReadFile("./input/a_hat.bin", aHatRead, aHatHost, aHatSize)) {
        return 10;
    }
    size_t e1Read = e1Size;
    if (!ReadFile("./input/e1.bin", e1Read, e1Host, e1Size)) {
        return 11;
    }

    CHECK_ACL(aclrtMemcpy(yDev, ySize, yHost, ySize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(aHatDev, aHatSize, aHatHost, aHatSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e1Dev, e1Size, e1Host, e1Size, ACL_MEMCPY_HOST_TO_DEVICE));

    const int32_t rc = RunPhasedSim(stream, tilingPinned, uDev, yDev, yHatDev, uNttDev, aHatDev, e1Dev, wsDev, wsHost,
                                    traceDev, traceHost, lutBytes, fused);
    if (rc != 0) {
        return rc;
    }

    CHECK_ACL(aclrtMemcpy(yHatHost, yHatSize, yHatDev, yHatSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uNttHost, uNttSize, uNttDev, uNttSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHost, uSize, uDev, uSize, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/y_hat.bin", yHatHost, yHatSize)) {
        return 2;
    }
    if (!WriteFile("./output/u_ntt.bin", uNttHost, uNttSize)) {
        return 3;
    }
    if (!WriteFile("./output/u.bin", uHost, uSize)) {
        return 4;
    }

    CHECK_ACL(aclrtFree(yDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(yHatDev));
    CHECK_ACL(aclrtFree(uNttDev));
    CHECK_ACL(aclrtFree(uDev));
    if (fused) {
        CHECK_ACL(aclrtFree(traceDev));
        CHECK_ACL(aclrtFreeHost(traceHost));
    }
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(yHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(e1Host));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(yHatHost));
    CHECK_ACL(aclrtFreeHost(uNttHost));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
