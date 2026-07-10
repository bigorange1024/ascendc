/**
 * @file main.cpp
 * @brief Alg.14 compute 行 2/18/19/21 Host 编排（不含 μ / 不含 prep）。
 *
 * 流水线位置：Encrypt compute 探针 host 入口；读 `input/`（y/a_hat/e1/e2/ek_pke + NTT/INTT LUT），
 * 按 CPU/SIM 路径 launch 设备核，写出中间态对拍 bin（y_hat/u_ntt/u/…）。
 * 探针：pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4（2026-07-07 晋级 pass-）。
 *
 * CPU / SIM 分叉（全仓统一宏，见 library/shared/ascendc_build_mode.hpp）：
 *   ASCENDC_BUILD_CPU → RunCpuThreeLaunch()（3 launch；tikicpu 不得融合，仅 û/u 子集）
 *   ASCENDC_BUILD_SIM → 默认 RunSimFusedSingleLaunch()（单 launch 全量含 v）
 *                       ASCENDC_SIM_HOST_MODE=phased_launch → RunSimThreeLaunch()（调试）
 *
 * 与 golden：scripts/gen_data.py 生成期望中间态；run.sh + cmp 对拍 output/。
 * tiling：`f203_encrypt_tiling.cpp` 运行时 GenerateTiling（替代旧 Python tiling.bin）。
 */
#include "ascendc_build_mode.hpp"
#include "data_utils.h"
#include "f203_l18_l19_tiling.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <thread>

/** 运行时 tiling 生成（见 f203_encrypt_tiling.cpp）；替代旧 Python input/tiling.bin。 */
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
extern volatile int g_f203_ntt_y_mix_pass;
#endif

#ifdef ASCENDC_CPU_DEBUG
/**
 * CPU 侧 Barrett 约化到 [0,q)：供 Host 参考 MultiplyNTTs / tr_hat_ntt 对拍。
 * 与 scripts/gen_data.py::multiply_ntts 所用约化同构；非设备路径。
 */
static inline int32_t CpuBarrettRedCoeff(int32_t x)
{
    constexpr int32_t q = 3329;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * Host 标量 Alg.11 MultiplyNTTs：h ← f ⊙ g（NTT 域 basemul）。
 * 仅 CPU 路径算 tr_hat_ntt golden；SIM 由设备核产出。
 */
static inline void CpuMultiplyNtts(int32_t *h, const int32_t *f, const int32_t *g)
{
    constexpr int32_t kN = static_cast<int32_t>(tiling::n);
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[2 * i];
        const int32_t a1 = f[2 * i + 1];
        const int32_t b0 = g[2 * i];
        const int32_t b1 = g[2 * i + 1];
        const int32_t a1b1 = CpuBarrettRedCoeff(a1 * b1);
        h[2 * i] = CpuBarrettRedCoeff(a0 * b0 + a1b1 * gamma);
        h[2 * i + 1] = CpuBarrettRedCoeff(a0 * b1 + a1 * b0);
    }
}
#endif

// ---------------------------------------------------------------------------
// 共用：LUT 装入 host ws（与 CPU/SIM 无关）
// ---------------------------------------------------------------------------

/**
 * 将 NTT 正变换 LUT（even/odd planar-stacked）装入 workspace 的 LUT_NTT_* 段。
 * @param ws host/device 可见的 workspace 基址；@param lutBytes 单份 even/odd 字节数
 * @return 两份文件均读成功为 true
 */
static bool LoadNttLutHost(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_ntt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_ntt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

/**
 * 三 launch / phased：INTT LUT 覆盖写入同一 LUT_NTT_* 区（NTT 完成后才跑 INTT，可安全覆盖）。
 * 与 intt_e1 布局一致。
 */
static bool LoadInttLutHostPhased(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

#ifndef ASCENDC_CPU_DEBUG
/**
 * SIM 融合单 launch：INTT LUT 写入独立 LUT_INTT_* 段，与 NTT LUT 并存于同一 ws。
 */
static bool LoadInttLutHostFused(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_INTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_INTT_ODD_STACKED, lutBytes);
}
#endif

#if ASCENDC_BUILD_CPU
// ===========================================================================
// CPU 路径：固定 3 launch（tikicpu MIX 串行 → 单 launch 死锁）
// ===========================================================================

/**
 * CPU：ntt_y → at_jp → intt_e1；仅对拍 y_hat/u_ntt/u（无 v/u_tr）。
 * @return 0 成功；非 0 为 LUT 读失败码
 */
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
// ===========================================================================
// SIM 路径：默认单 launch；ASCENDC_SIM_HOST_MODE=phased_launch → 3 launch 调试
// ===========================================================================

constexpr int32_t kFusedTraceSlots = 16;
constexpr const char *kFusedTraceNames[kFusedTraceSlots] = {
    "AIV_NTT_SPLIT",    "AIC_NTT_MMAD",     "AIV_NTT_YHAT",     "AIV_AT_JP_START", "AIV_AT_JP_DONE",
    "AIV_IP_SIGNAL",    "AIC_IP_WAIT_DONE", "AIV_INTT_SPLIT",   "AIC_INTT_MMAD",   "AIV_INTT_U",
    "AIV_E1_DONE",      "AIC_AT_JP_GATE",   "AIV_AT_JP_GATE",   "AIV_DECODE_T",    nullptr,
};

/** 返回 trace 中最高已置位槽下标；全 0 则 -1 */
static int32_t FusedTraceHighest(const int32_t *trace)
{
    for (int32_t i = kFusedTraceSlots - 1; i >= 0; --i) {
        if (trace[i] != 0) {
            return i;
        }
    }
    return -1;
}

/** 轮询打印 fused FSM 进度（stall 或前进） */
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

/** Host ws（含 LUT）整段 H2D */
static void CopyWsLutToDevice(uint8_t *wsDev, const uint8_t *wsHost, size_t wsSize, size_t lutBytes)
{
    CHECK_ACL(aclrtMemcpy(wsDev, wsSize, wsHost, wsSize, ACL_MEMCPY_HOST_TO_DEVICE));
    (void)lutBytes;
}

/** SIM 调试：三 launch 分段（与 CPU 同口径，非生产） */
static int32_t RunSimThreeLaunch(aclrtStream stream, TilingData *tilingHost, uint8_t *uDev, uint8_t *yDev,
                                 uint8_t *yHatDev, uint8_t *uNttDev, uint8_t *aHatDev, uint8_t *eDev, uint8_t *wsDev,
                                 uint8_t *wsHost, size_t lutBytes)
{
    std::fprintf(stderr, "[sim-phased] 3 launch 调试路径（ntt_y → at_jp → intt_e1）\n");
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

/**
 * SIM 生产：单 launch f203_encrypt_l18_l19；后台线程每 5s 轮询 trace 防挂死诊断。
 * 覆盖行 2/16–21（含 v）；须同时装 NTT+INTT LUT。
 */
static int32_t RunSimFusedSingleLaunch(aclrtStream stream, TilingData *tilingHost, uint8_t *uDev, uint8_t *vDev,
                                       uint8_t *yDev, uint8_t *yHatDev, uint8_t *uNttDev, uint8_t *uTrDev,
                                       uint8_t *aHatDev, uint8_t *e1Dev, uint8_t *e2Dev, uint8_t *ekPkeDev,
                                       uint8_t *tHatDev, uint8_t *trHatNttDev, uint8_t *wsDev, uint8_t *wsHost,
                                       uint8_t *traceDev, int32_t *traceHost, size_t lutBytes)
{
    std::memset(wsHost, 0, tiling::wssize);
    std::memset(traceHost, 0, kFusedTraceSlots * sizeof(int32_t));
    if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
        return 20;
    }
    CopyWsLutToDevice(wsDev, wsHost, tiling::wssize, lutBytes);
    CHECK_ACL(aclrtMemcpy(traceDev, kFusedTraceSlots * sizeof(int32_t), traceHost,
                          kFusedTraceSlots * sizeof(int32_t), ACL_MEMCPY_HOST_TO_DEVICE));

    std::fprintf(stderr, "[sim-fused] launch f203_encrypt_l18_l19 (blockDim=1)\n");
    const auto t0 = std::chrono::steady_clock::now();
    std::atomic<bool> pollDone{false};
    int32_t prevHigh = -1;
    std::thread poller([&]() {
        while (!pollDone.load()) {
            const aclError cpRc = aclrtMemcpy(traceHost, kFusedTraceSlots * sizeof(int32_t), traceDev,
                                              kFusedTraceSlots * sizeof(int32_t), ACL_MEMCPY_DEVICE_TO_HOST);
            if (cpRc == ACL_SUCCESS) {
                const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                PrintFusedTrace(traceHost, prevHigh, sec);
                prevHigh = FusedTraceHighest(traceHost);
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });

    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, e1Dev, e2Dev, wsDev, tilingHost, traceDev);
    const aclError syncRc = aclrtSynchronizeStream(stream);
    pollDone.store(true);
    poller.join();

    CHECK_ACL(aclrtMemcpy(traceHost, kFusedTraceSlots * sizeof(int32_t), traceDev, kFusedTraceSlots * sizeof(int32_t),
                          ACL_MEMCPY_DEVICE_TO_HOST));
    const double totalSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "[sim-fused] trace slots:");
    for (int32_t i = 0; i < kFusedTraceSlots; ++i) {
        if (traceHost[i] != 0) {
            std::fprintf(stderr, " %d:%s", i, (kFusedTraceNames[i] != nullptr) ? kFusedTraceNames[i] : "?");
        }
    }
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "[sim-fused] sync ret=%d total=%.1fs final=%s\n", static_cast<int>(syncRc), totalSec,
                 (FusedTraceHighest(traceHost) >= 0 && kFusedTraceNames[FusedTraceHighest(traceHost)] != nullptr)
                     ? kFusedTraceNames[FusedTraceHighest(traceHost)]
                     : "none");
    CHECK_ACL(syncRc);
    return 0;
}

/** 按 ASCENDC_SIM_HOST_MODE 选择 fused 或 phased */
static int32_t RunSimFeasibility(aclrtStream stream, TilingData *tilingHost, uint8_t *uDev, uint8_t *vDev,
                                 uint8_t *yDev, uint8_t *yHatDev, uint8_t *uNttDev, uint8_t *uTrDev, uint8_t *aHatDev,
                                 uint8_t *e1Dev, uint8_t *e2Dev, uint8_t *wsDev, uint8_t *ekPkeDev, uint8_t *tHatDev,
                                 uint8_t *trHatNttDev, uint8_t *wsHost, uint8_t *traceDev, int32_t *traceHost,
                                 size_t lutBytes)
{
    if (ascendc::SimHostEncryptFeasPhasedLaunch()) {
        return RunSimThreeLaunch(stream, tilingHost, uDev, yDev, yHatDev, uNttDev, aHatDev, e1Dev, wsDev, wsHost,
                                 lutBytes);
    }
    return RunSimFusedSingleLaunch(stream, tilingHost, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, e1Dev,
                                   e2Dev, ekPkeDev, tHatDev, trHatNttDev, wsDev, wsHost, traceDev, traceHost,
                                   lutBytes);
}
#endif

/**
 * Host 主函数：按编译宏走 CPU 三 launch 或 SIM fused/phased，写出 compute 中间态 bin。
 * 输入：y / a_hat / e1 / e2 / ek_pke + LUT；返回 0 成功，非 0 为读/写失败码。
 */
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
    const size_t ekPkeSize = 4 * 384;
    const size_t e1Size = tiling::e1FileBytes;
    const size_t e2Size = tiling::e2FileBytes;
    const size_t uSize = tiling::uFileBytes;
    const size_t vSize = tiling::vFileBytes;
    const size_t trHatNttSize = tiling::n * sizeof(int32_t);
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;

    // 运行时生成 tiling（模板风格；数值集中于 f203_encrypt_tiling.cpp）
    TilingData tilingHost{};
    GenerateTiling(tilingHost);

#ifdef ASCENDC_CPU_DEBUG
    // --- CPU 孪生：GmAlloc + 读 input → 三 launch → 写 output 中间态 ---
    AscendC::SetKernelMode(KernelMode::MIX_MODE);

    uint8_t *uOut = (uint8_t *)AscendC::GmAlloc(uSize > 1024 ? uSize : 1024);
    uint8_t *ySrc = (uint8_t *)AscendC::GmAlloc(ySize > 1024 ? ySize : 1024);
    uint8_t *yHat = (uint8_t *)AscendC::GmAlloc(yHatSize > 1024 ? yHatSize : 1024);
    uint8_t *uNtt = (uint8_t *)AscendC::GmAlloc(uNttSize > 1024 ? uNttSize : 1024);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatSize > 1024 ? aHatSize : 1024);
    uint8_t *ekPke = (uint8_t *)AscendC::GmAlloc(ekPkeSize > 1024 ? ekPkeSize : 1024);
    uint8_t *e1 = (uint8_t *)AscendC::GmAlloc(e1Size > 1024 ? e1Size : 1024);
    uint8_t *e2 = (uint8_t *)AscendC::GmAlloc(e2Size > 1024 ? e2Size : 1024);
    uint8_t *trHatNtt = (uint8_t *)AscendC::GmAlloc(trHatNttSize > 1024 ? trHatNttSize : 1024);
    uint8_t *vOut = (uint8_t *)AscendC::GmAlloc(vSize > 1024 ? vSize : 1024);
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
    size_t e2Read = e2Size;
    if (!ReadFile("./input/e2.bin", e2Read, e2, e2Size)) {
        return 14;
    }
    size_t ekRead = ekPkeSize;
    if (!ReadFile("./input/ek_pke.bin", ekRead, ekPke, ekPkeSize)) {
        return 13;
    }

    // CPU：Alg.14 行 2 decode_t_hat（ek_pke[0:1536] → t_hat），仅用于 Host 算 tr_hat_ntt；不落盘 t_hat。
    constexpr int32_t kN = static_cast<int32_t>(tiling::n);
    constexpr int32_t kK = static_cast<int32_t>(tiling::kK);
    int32_t tHatI32[kK * kN];
    {
        const auto *ek = reinterpret_cast<const uint8_t *>(ekPke);
        for (int32_t j = 0; j < kK; ++j) {
            for (int32_t i = 0; i < kN / 2; ++i) {
                const int32_t b0 = static_cast<int32_t>(ek[j * 384 + 3 * i + 0]);
                const int32_t b1 = static_cast<int32_t>(ek[j * 384 + 3 * i + 1]);
                const int32_t b2 = static_cast<int32_t>(ek[j * 384 + 3 * i + 2]);
                const int32_t t0 = b0 | ((b1 & 0x0F) << 8);
                const int32_t t1 = (b1 >> 4) | (b2 << 4);
                tHatI32[j * kN + 2 * i] = t0;
                tHatI32[j * kN + 2 * i + 1] = t1;
            }
        }
    }

    // ntt_y → at_jp → intt_e1（仅 û/u 子集；无设备侧 v）
    const int32_t rc = RunCpuThreeLaunch(tilingHost, uOut, ySrc, yHat, uNtt, aHat, e1, ws, lutBytes);
    if (rc != 0) {
        return rc;
    }

    // Host 补算 tr_hat_ntt = Σ_j MultiplyNTTs(t̂_j, ŷ_j) mod q（3 launch 无设备内积核）
    {
        const auto *tI32 = tHatI32;
        const auto *yHatI32 = reinterpret_cast<const int32_t *>(yHat);
        auto *outI32 = reinterpret_cast<int32_t *>(trHatNtt);
        int32_t prod[kN];
        int64_t acc[kN];
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] = 0;
        }
        for (int32_t j = 0; j < kK; ++j) {
            CpuMultiplyNtts(prod, tI32 + j * kN, yHatI32 + j * kN);
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] += static_cast<int64_t>(prod[c]);
            }
        }
        constexpr int64_t q64 = 3329;
        for (int32_t c = 0; c < kN; ++c) {
            int64_t rem = acc[c] % q64;
            if (rem < 0) {
                rem += q64;
            }
            outI32[c] = static_cast<int32_t>(rem);
        }
    }

    // CPU v：读 golden 对拍用（设备 3 launch 仅 u）；与 gen_data INTT(tr̂)+e₂ 一致需 Python 级 stage123。
    // 此处用 uNtt 已算 û，v 由 tr_hat_ntt 经 host 不展开 INTT；写 output/v.bin 占位由 run.sh 对 golden_v。
    (void)vOut;

    if (!WriteFile("./output/y_hat.bin", yHat, yHatSize)) {
        return 2;
    }
    if (!WriteFile("./output/u_ntt.bin", uNtt, uNttSize)) {
        return 3;
    }
    if (!WriteFile("./output/u.bin", uOut, uSize)) {
        return 4;
    }
    if (!WriteFile("./output/tr_hat_ntt.bin", trHatNtt, trHatNttSize)) {
        return 5;
    }

    AscendC::GmFree(uOut);
    AscendC::GmFree(ySrc);
    AscendC::GmFree(yHat);
    AscendC::GmFree(uNtt);
    AscendC::GmFree(aHat);
    AscendC::GmFree(ekPke);
    AscendC::GmFree(e1);
    AscendC::GmFree(e2);
    AscendC::GmFree(trHatNtt);
    AscendC::GmFree(vOut);
    AscendC::GmFree(ws);
#else
    // --- SIM/NPU：ACL 分配 + H2D → fused/phased launch → D2H 写 output ---
    const size_t uTrSize = tiling::uTrFileBytes;

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *yHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *ekPkeHost = nullptr;
    uint8_t *e1Host = nullptr;
    uint8_t *e2Host = nullptr;
    uint8_t *trHatNttHost = nullptr;
    uint8_t *uTrHost = nullptr;
    uint8_t *vHost = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *yHatHost = nullptr;
    uint8_t *uNttHost = nullptr;
    uint8_t *uHost = nullptr;
    TilingData *tilingPinned = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPinned), tilingSize));
    std::memcpy(tilingPinned, &tilingHost, sizeof(TilingData));

    uint8_t *yDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *ekPkeDev = nullptr;
    uint8_t *e1Dev = nullptr;
    uint8_t *e2Dev = nullptr;
    uint8_t *trHatNttDev = nullptr;
    uint8_t *uTrDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *yHatDev = nullptr;
    uint8_t *uNttDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *traceDev = nullptr;
    int32_t *traceHost = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHost), ySize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), aHatSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), ekPkeSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e1Host), e1Size));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e2Host), e2Size));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&trHatNttHost), trHatNttSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uTrHost), uTrSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&vHost), vSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHatHost), yHatSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uNttHost), uNttSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHost), uSize));

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yDev), ySize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), aHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), ekPkeSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&e1Dev), e1Size, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&e2Dev), e2Size, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&trHatNttDev), trHatNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uTrDev), uTrSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), vSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yHatDev), yHatSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uNttDev), uNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), uSize, ACL_MEM_MALLOC_HUGE_FIRST));

    // 生产默认 fused：需 trace 缓冲；phased 调试路径不分配 trace
    const bool simPhasedDebug = ascendc::SimHostEncryptFeasPhasedLaunch();
    if (!simPhasedDebug) {
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
    size_t e2Read = e2Size;
    if (!ReadFile("./input/e2.bin", e2Read, e2Host, e2Size)) {
        return 14;
    }
    size_t ekRead = ekPkeSize;
    if (!ReadFile("./input/ek_pke.bin", ekRead, ekPkeHost, ekPkeSize)) {
        return 13;
    }

    CHECK_ACL(aclrtMemcpy(yDev, ySize, yHost, ySize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(aHatDev, aHatSize, aHatHost, aHatSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekPkeDev, ekPkeSize, ekPkeHost, ekPkeSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e1Dev, e1Size, e1Host, e1Size, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e2Dev, e2Size, e2Host, e2Size, ACL_MEMCPY_HOST_TO_DEVICE));

    // tHatDev 由 fused 核内部 decode；Host 传空指针占位（与布局约定一致）
    uint8_t *tHatDev = nullptr;
    const int32_t rc = RunSimFeasibility(stream, tilingPinned, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev,
                                         e1Dev, e2Dev, wsDev, ekPkeDev, tHatDev, trHatNttDev, wsHost, traceDev,
                                         traceHost, lutBytes);
    if (rc != 0) {
        return rc;
    }

    // D2H：中间态落盘供 run.sh cmp（生产 Encrypt 全链不落这些；本探针验收需要）
    CHECK_ACL(aclrtMemcpy(yHatHost, yHatSize, yHatDev, yHatSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uNttHost, uNttSize, uNttDev, uNttSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHost, uSize, uDev, uSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uTrHost, uTrSize, uTrDev, uTrSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, vSize, vDev, vSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(trHatNttHost, trHatNttSize, trHatNttDev, trHatNttSize, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/y_hat.bin", yHatHost, yHatSize)) {
        return 2;
    }
    if (!WriteFile("./output/u_ntt.bin", uNttHost, uNttSize)) {
        return 3;
    }
    if (!WriteFile("./output/u.bin", uHost, uSize)) {
        return 4;
    }
    if (!WriteFile("./output/u_tr.bin", uTrHost, uTrSize)) {
        return 6;
    }
    if (!WriteFile("./output/v.bin", vHost, vSize)) {
        return 7;
    }
    if (!WriteFile("./output/tr_hat_ntt.bin", trHatNttHost, trHatNttSize)) {
        return 5;
    }

    CHECK_ACL(aclrtFree(yDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFree(e2Dev));
    CHECK_ACL(aclrtFree(trHatNttDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(yHatDev));
    CHECK_ACL(aclrtFree(uNttDev));
    CHECK_ACL(aclrtFree(uDev));
    if (!simPhasedDebug) {
        CHECK_ACL(aclrtFree(traceDev));
        CHECK_ACL(aclrtFreeHost(traceHost));
    }
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(yHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(e1Host));
    CHECK_ACL(aclrtFreeHost(e2Host));
    CHECK_ACL(aclrtFreeHost(trHatNttHost));
    CHECK_ACL(aclrtFreeHost(uTrHost));
    CHECK_ACL(aclrtFreeHost(vHost));
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
