/**
 * @file main_decrypt_g4_run.cpp
 * @brief Alg.15 Decrypt Host 编排：单 kernel launch；仅 D2H m[32]。
 *
 * 流水线位置：main_decrypt.cpp → 本文件 → f203_decrypt_device_fused（设备）。
 * 对齐 customspec §编排；基线 pass-fix-f203-alg15-pke-decrypt-device-k4。
 *
 * Host 职责（非密码学）：
 *   1) 分配设备 GM：生产输入 dk/c、输出 m、中间态 u/v/ŝ/û/ŵ/…、NTT/INTT ws、softSync
 *   2) H2D：dk、c、LUT→ws、softSync 清零
 *   3) 启动 f203_decrypt_device_fused（blockDim=1，MIX）
 *   4) D2H：仅 m；写 output/m.bin
 *
 * 中间态 GM（u/v/ŝ/…）仅设备内使用，禁止作为生产落盘；Host 不读回。
 */
#include "f203_decrypt_g4_run.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_trace_layout.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
#include "aclrtlaunch_f203_decrypt_device_fused.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" __global__ __aicore__ void f203_decrypt_device_fused(
    GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm,
    GM_ADDR wPaddedGm, GM_ADDR wTimeGm, GM_ADDR mGm, GM_ADDR nttWsGm, GM_ADDR inttWsGm, GM_ADDR softSyncGm,
    GM_ADDR traceGm, TilingData tiling);
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#ifndef ASCENDC_CPU_DEBUG
#include <atomic>
#include <chrono>
#include <thread>
#endif

bool WriteFile(const std::string &filePath, const void *buffer, size_t size);

#ifndef ASCENDC_CPU_DEBUG
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);
#endif

namespace {

constexpr uint32_t kBlockDim = 1U;
/** softSyncGm：int32[2] 哨兵（prep / su_dot+pad）；Host 须清零，否则 AIV1 自旋死等。 */
constexpr size_t kSoftSyncBytes = 64U;

#ifndef ASCENDC_CPU_DEBUG
/** TRACE 缓冲字节（见 f203_decrypt_trace_layout.h）。 */
constexpr size_t kDecryptTraceDevBytes = f203_decrypt_trace::kTraceDevBytes;
constexpr int kDecryptTraceStages = static_cast<int>(f203_decrypt_trace::kTraceSlots);
/**
 * 轮询 decrypt TRACE（每槽 stride=8 int32）+ 可选 softSync[0..1]；卡死时 stderr 定位段。
 */
static aclError SynchronizeDecryptMaybeTrace(aclrtStream stream, void *traceDev, int32_t *traceHost, void *softSyncDev,
                                             int32_t *softSyncHost)
{
    if (traceDev == nullptr || traceHost == nullptr) {
        return aclrtSynchronizeStream(stream);
    }

    std::atomic<aclError> syncRc{ACL_ERROR_NONE};
    std::atomic<bool> done{false};
    std::thread syncer([&]() {
        syncRc.store(aclrtSynchronizeStream(stream), std::memory_order_release);
        done.store(true, std::memory_order_release);
    });

    int lastPop = -1;
    int32_t lastSoft0 = -1;
    int32_t lastSoft1 = -1;
    constexpr int kPollMs = 500;
    constexpr uint32_t kAlign = f203_decrypt_trace::kAlignInts;

    while (!done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        if (done.load(std::memory_order_acquire)) {
            break;
        }
        std::memset(traceHost, 0, kDecryptTraceDevBytes);
        (void)aclrtMemcpy(traceHost, kDecryptTraceDevBytes, traceDev, kDecryptTraceDevBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST);
        int pop = 0;
        for (int i = 0; i < kDecryptTraceStages; ++i) {
            if (traceHost[i * static_cast<int>(kAlign)] != 0) {
                ++pop;
            }
        }
        if (softSyncDev != nullptr && softSyncHost != nullptr) {
            (void)aclrtMemcpy(softSyncHost, 2 * sizeof(int32_t), softSyncDev, 2 * sizeof(int32_t),
                              ACL_MEMCPY_DEVICE_TO_HOST);
        }
        const bool softChanged =
            softSyncHost != nullptr && (softSyncHost[0] != lastSoft0 || softSyncHost[1] != lastSoft1);
        if (pop != lastPop || softChanged) {
            lastPop = pop;
            if (softSyncHost != nullptr) {
                lastSoft0 = softSyncHost[0];
                lastSoft1 = softSyncHost[1];
            }
            std::fprintf(stderr, "[decrypt-trace] stages set=%d/%d :", pop, kDecryptTraceStages);
            for (int i = 0; i < kDecryptTraceStages; ++i) {
                if (traceHost[i * static_cast<int>(kAlign)] != 0) {
                    std::fprintf(stderr, " %d", i);
                }
            }
            if (softSyncHost != nullptr) {
                std::fprintf(stderr, " | softSync=[%d,%d]", softSyncHost[0], softSyncHost[1]);
            }
            std::fprintf(stderr, "\n");
            std::fflush(stderr);
        }
    }
    syncer.join();
    return syncRc.load(std::memory_order_acquire);
}

/** Host 预填 trace 末尾 ones 模板，供 AIC DataCopy 写槽。 */
static void fill_decrypt_trace_ones(int32_t *traceHost)
{
    auto *ones = traceHost + static_cast<int>(f203_decrypt_trace::kOnesOffBytes / sizeof(int32_t));
    for (uint32_t i = 0; i < f203_decrypt_trace::kAlignInts; ++i) {
        ones[i] = 1;
    }
}
#endif

/**
 * 将 even/odd stacked LUT 拷入 NTT/INTT workspace 固定偏移。
 * @param ws      设备侧 workspace 主机镜像（或 CPU GmAlloc 缓冲）
 * @param wsBytes workspace 总字节（tiling::wssize）
 * @param lut_even / lut_odd  各 tiling::lutEvenOddFileBytes
 */
static void fill_ntt_ws(uint8_t *ws, size_t wsBytes, const uint8_t *lut_even, const uint8_t *lut_odd)
{
    std::memset(ws, 0, wsBytes);
    std::memcpy(ws + tiling::LUT_EVEN_STACKED, lut_even, tiling::lutEvenOddFileBytes);
    std::memcpy(ws + tiling::LUT_ODD_STACKED, lut_odd, tiling::lutEvenOddFileBytes);
}

/**
 * 一次设备会话：H2D → kernel → D2H m。
 * CPU 路径用 AscendC::GmAlloc + ICPU_RUN_KF；SIM/NPU 用 ACL。
 */
int run_device_session(const uint8_t *dk, const uint8_t *c, const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                       const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *m_out)
{
    using namespace tiling;
    TilingData tilingData{};
    tilingData.tileLength = static_cast<int32_t>(n); /* N=256 */
    tilingData.kPolys = static_cast<int32_t>(kK);    /* k=4 */
    tilingData.mixPass = 3;                          /* 全量 mixPass（生产默认） */

#ifdef ASCENDC_CPU_DEBUG
    /* ---- CPU 孪生：GmAlloc 模拟设备缓冲 ---- */
    uint8_t *dkGm = (uint8_t *)AscendC::GmAlloc(F203_DK_PKE_BYTES);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(F203_U_POLYVEC_BYTES);   /* u' polyvec */
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_V_POLY_BYTES);      /* v' poly */
    uint8_t *sHatGm = (uint8_t *)AscendC::GmAlloc(F203_S_HAT_BYTES);    /* ŝ */
    uint8_t *uHatGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);        /* û（NTT 出） */
    uint8_t *wHatGm = (uint8_t *)AscendC::GmAlloc(F203_W_HAT_BYTES);    /* ŵ = ⟨ŝ,û⟩ */
    uint8_t *wPaddedGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);     /* INTT 输入 pad */
    uint8_t *wTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);       /* w 时域（INTT 出） */
    uint8_t *nttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *inttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *softSyncGm = (uint8_t *)AscendC::GmAlloc(kSoftSyncBytes);
    std::memset(softSyncGm, 0, kSoftSyncBytes);

    std::memcpy(dkGm, dk, F203_DK_PKE_BYTES);
    std::memcpy(cGm, c, F203_CT_PKE_BYTES);
    fill_ntt_ws(nttWsGm, wssize, lut_ntt_even, lut_ntt_odd);
    fill_ntt_ws(inttWsGm, wssize, lut_intt_even, lut_intt_odd);

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_decrypt_device_fused, kBlockDim, dkGm, cGm, uGm, vGm, sHatGm, uHatGm, wHatGm, wPaddedGm,
                wTimeGm, mGm, nttWsGm, inttWsGm, softSyncGm, static_cast<GM_ADDR>(nullptr), tilingData);

    std::memcpy(m_out, mGm, F203_MSG_BYTES);

    AscendC::GmFree(dkGm);
    AscendC::GmFree(cGm);
    AscendC::GmFree(mGm);
    AscendC::GmFree(uGm);
    AscendC::GmFree(vGm);
    AscendC::GmFree(sHatGm);
    AscendC::GmFree(uHatGm);
    AscendC::GmFree(wHatGm);
    AscendC::GmFree(wPaddedGm);
    AscendC::GmFree(wTimeGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(inttWsGm);
    AscendC::GmFree(softSyncGm);
    return 0;
#else
    /* ---- SIM / NPU：ACL 分配 + H2D/D2H ---- */
    CHECK_ACL(aclInit(nullptr));
    // 设备号：读 ASCEND_DEVICE_ID；缺省 0（标准默认；探针挂死脏退后同卡会连环挂，见 acl_session；需换卡时再 export）。SIM 由 run.sh 强制 export=0。
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    // 早退 / SIGINT / SIGTERM 均会 ResetDevice+Finalize，减轻同卡污染
    ascendc_acl::DeviceGuard aclGuard(deviceId);
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *cDev = nullptr;
    uint8_t *dkDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *sHatDev = nullptr;
    uint8_t *uHatDev = nullptr;
    uint8_t *wHatDev = nullptr;
    uint8_t *wPaddedDev = nullptr;
    uint8_t *wTimeDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    uint8_t *inttWsDev = nullptr;
    uint8_t *softSyncDev = nullptr;
    uint8_t *traceDev = nullptr;
    int32_t *traceHost = nullptr;
    int32_t *softSyncPollHost = nullptr;
    TilingData *tilingHost = nullptr;
    const bool decryptTrace = ascendc_acl::EnvFlagOn("F203_DECRYPT_TRACE");

    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dkDev, F203_DK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, F203_U_POLYVEC_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_V_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sHatDev, F203_S_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wHatDev, F203_W_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&softSyncDev, kSoftSyncBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    if (decryptTrace) {
        CHECK_ACL(aclrtMalloc((void **)&traceDev, kDecryptTraceDevBytes, ACL_MEM_MALLOC_HUGE_FIRST));
        CHECK_ACL(aclrtMallocHost((void **)&traceHost, kDecryptTraceDevBytes));
        CHECK_ACL(aclrtMallocHost((void **)&softSyncPollHost, 2 * sizeof(int32_t)));
        std::memset(traceHost, 0, kDecryptTraceDevBytes);
        fill_decrypt_trace_ones(traceHost);
        CHECK_ACL(aclrtMemcpy(traceDev, kDecryptTraceDevBytes, traceHost, kDecryptTraceDevBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
        std::fprintf(stderr, "[main_decrypt] F203_DECRYPT_TRACE=1：轮询 fused-trace + softSync\n");
    }
    CHECK_ACL(aclrtMallocHost((void **)&tilingHost, sizeof(TilingData)));
    *tilingHost = tilingData;

    CHECK_ACL(aclrtMemcpy(cDev, F203_CT_PKE_BYTES, c, F203_CT_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, F203_DK_PKE_BYTES, dk, F203_DK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    {
        std::vector<uint8_t> zeros(kSoftSyncBytes, 0);
        CHECK_ACL(aclrtMemcpy(softSyncDev, kSoftSyncBytes, zeros.data(), kSoftSyncBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        std::vector<uint8_t> wsHost(wssize, 0);
        fill_ntt_ws(wsHost.data(), wssize, lut_ntt_even, lut_ntt_odd);
        CHECK_ACL(aclrtMemcpy(nttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        fill_ntt_ws(wsHost.data(), wssize, lut_intt_even, lut_intt_odd);
        CHECK_ACL(aclrtMemcpy(inttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    }

    const uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_device_fused)(
        kBlockDim, stream, dkDev, cDev, uDev, vDev, sHatDev, uHatDev, wHatDev, wPaddedDev, wTimeDev, mDev, nttWsDev,
        inttWsDev, softSyncDev, traceDev, tilingHost);
    if (ret != 0) {
        return 30;
    }
    if (decryptTrace) {
        CHECK_ACL(SynchronizeDecryptMaybeTrace(stream, traceDev, traceHost, softSyncDev, softSyncPollHost));
    } else {
        CHECK_ACL(aclrtSynchronizeStream(stream));
    }
    /* 生产契约：仅回传 m；中间态不 D2H */
    CHECK_ACL(aclrtMemcpy(m_out, F203_MSG_BYTES, mDev, F203_MSG_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFree(uHatDev));
    CHECK_ACL(aclrtFree(wHatDev));
    CHECK_ACL(aclrtFree(wPaddedDev));
    CHECK_ACL(aclrtFree(wTimeDev));
    CHECK_ACL(aclrtFree(nttWsDev));
    CHECK_ACL(aclrtFree(inttWsDev));
    CHECK_ACL(aclrtFree(softSyncDev));
    if (traceDev != nullptr) {
        CHECK_ACL(aclrtFree(traceDev));
    }
    if (traceHost != nullptr) {
        CHECK_ACL(aclrtFreeHost(traceHost));
    }
    if (softSyncPollHost != nullptr) {
        CHECK_ACL(aclrtFreeHost(softSyncPollHost));
    }
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    // ResetDevice+Finalize 由 aclGuard 析构统一执行（含早退路径）
    return 0;
#endif
}

} // namespace

/**
 * 对外入口：跑设备全链并写 output/m.bin。
 * @param case_dir 用例根（含 output/）
 * @param dk/c/lut_*  Host 侧生产输入缓冲
 * @param m_out      32B 明文缓冲（同时写盘）
 * @return 0 成功；非 0 见调用点错误码
 */
int run_decrypt_device_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *m_out)
{
    const int rc = run_device_session(dk, c, lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd, m_out);
    if (rc != 0) {
        return rc;
    }
    if (!WriteFile(case_dir + "/output/m.bin", m_out, F203_MSG_BYTES)) {
        return 40;
    }
    std::printf("[main_decrypt] device-k4 **1-kernel** done m.bin=%uB (no mid D2H)\n", F203_MSG_BYTES);
    return 0;
}
