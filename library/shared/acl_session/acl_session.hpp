/**
 * @file acl_session.hpp
 * @brief Host ACL 会话守卫：保证 aclFinalize / ResetDevice 在任何退出路径都会执行。
 *
 * ## 背景（2026-08-03 借入机）
 * 现象：先跑 KEM 探针，encaps/decaps 卡在 `f203_encrypt_l18_l19` 的 SynchronizeStream；
 * Ctrl+C 或 `timeout` 杀进程后，**同一 ASCEND_DEVICE_ID** 上后续 stable 全部再卡。
 * 换另一张未被跑过的卡（如 device 0）则正常 —— 说明是**进程脏退导致该卡被污染**，
 * 不是「某张卡硬件坏了」。
 *
 * 代码缺口：
 * 1. `aclInit` 之后多处 `return N` 不走 `ResetDevice`/`Finalize`；
 * 2. `CHECK_ACL` 只打印不中止；
 * 3. `kernel-run-timeout.sh` / Ctrl+C 默认 SIGTERM/SIGINT 无清理钩子。
 *
 * ## 用法（NPU/SIM host 分支，aclInit 成功后立刻构造）
 * ```cpp
 * CHECK_ACL(aclInit(nullptr));
 * int32_t deviceId = ...;
 * CHECK_ACL(aclrtSetDevice(deviceId));
 * ascendc_acl::DeviceGuard guard(deviceId);  // 此后任意 return / 信号都会 teardown
 * ...
 * // 成功路径：可继续手动 Free / DestroyStream；不必再调 ResetDevice/Finalize（交给析构）
 * // 若成功路径已自行 Finalize，调用 guard.disarm() 避免二次 Finalize
 * ```
 */
#pragma once

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <thread>

#include "acl/acl.h"

namespace ascendc_acl {

/** 进程内当前守卫（信号处理器用）；同一时刻只应有一个活跃 NPU/SIM session。 */
inline std::atomic<class DeviceGuard *> g_active_guard{nullptr};

/**
 * 设备会话守卫。
 * 析构或信号到达时：尽力 `aclrtResetDevice` + `aclFinalize`，释放驱动侧会话，
 * 降低「挂死 → 杀进程 → 同卡后续全挂」的污染概率。
 */
class DeviceGuard {
public:
    explicit DeviceGuard(int32_t deviceId) : deviceId_(deviceId), armed_(true)
    {
        g_active_guard.store(this, std::memory_order_release);
        // 挂接信号：timeout(1) 默认 SIGTERM；交互 Ctrl+C 为 SIGINT。
        // ACL 并非严格 async-signal-safe，但这是杀进程前唯一能清设备的窗口。
        prevInt_ = std::signal(SIGINT, &DeviceGuard::onSignal);
        prevTerm_ = std::signal(SIGTERM, &DeviceGuard::onSignal);
    }

    DeviceGuard(const DeviceGuard &) = delete;
    DeviceGuard &operator=(const DeviceGuard &) = delete;

    ~DeviceGuard()
    {
        teardown("scope_exit");
        restoreSignals();
    }

    /** 成功路径若已自行 Finalize，调用以免析构时二次 Finalize。 */
    void disarm()
    {
        armed_ = false;
        if (g_active_guard.load(std::memory_order_acquire) == this) {
            g_active_guard.store(nullptr, std::memory_order_release);
        }
    }

    int32_t deviceId() const { return deviceId_; }

    void teardown(const char *why)
    {
        if (!armed_) {
            return;
        }
        armed_ = false;
        if (g_active_guard.load(std::memory_order_acquire) == this) {
            g_active_guard.store(nullptr, std::memory_order_release);
        }
        std::fprintf(stderr, "[acl_session] teardown (%s) device=%d\n", why, deviceId_);
        std::fflush(stderr);
        // Reset 失败也继续 Finalize：目标是尽量丢掉该卡上的会话残留。
        (void)aclrtResetDevice(deviceId_);
        (void)aclFinalize();
    }

private:
    static void onSignal(int sig)
    {
        DeviceGuard *g = g_active_guard.load(std::memory_order_acquire);
        if (g != nullptr) {
            g->teardown(sig == SIGINT ? "SIGINT" : "SIGTERM");
        }
        // 恢复默认并重抛，让 timeout / shell 仍能看到非 0 退出。
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }

    void restoreSignals()
    {
        if (prevInt_ != SIG_ERR) {
            std::signal(SIGINT, prevInt_);
        }
        if (prevTerm_ != SIG_ERR) {
            std::signal(SIGTERM, prevTerm_);
        }
    }

    int32_t deviceId_;
    bool armed_;
    void (*prevInt_)(int) = SIG_DFL;
    void (*prevTerm_)(int) = SIG_DFL;
};

/** 进程内 KernelLaunch 序号（NPU/SIM host 逐 launch 计时用）。 */
inline std::atomic<int> g_launch_seq{0};

/**
 * 把一次 stream 同步写成 `[npu_launch]` 行 + JSONL。
 * 文件默认 `./output/npu_launch_metrics.jsonl`，可用 `NPU_LAUNCH_METRICS_FILE` 覆盖。
 * stdout 行会被 `msprof_run.sh` tee 进 `output/run_metrics.txt`。
 */
inline void EmitLaunchMetric(const char *name, double durationUs, aclError rc)
{
    const int seq = g_launch_seq.fetch_add(1, std::memory_order_relaxed) + 1;
    const char *nm = (name != nullptr && name[0] != '\0') ? name : "unnamed";
    std::fprintf(stdout, "[npu_launch] seq=%d name=%s duration_us=%.1f rc=%d\n", seq, nm, durationUs,
                 static_cast<int>(rc));
    std::fflush(stdout);

    const char *path = std::getenv("NPU_LAUNCH_METRICS_FILE");
    if (path == nullptr || path[0] == '\0') {
        path = "./output/npu_launch_metrics.jsonl";
    }
    // 尽力建 output/；失败仍保留 stdout 行。
    (void)mkdir("output", 0755);
    FILE *fp = std::fopen(path, "a");
    if (fp != nullptr) {
        std::fprintf(fp, "{\"seq\":%d,\"name\":\"%s\",\"duration_us\":%.1f,\"rc\":%d}\n", seq, nm, durationUs,
                     static_cast<int>(rc));
        std::fclose(fp);
    }
}

/**
 * KernelLaunch 之后的同步 + 逐 launch 计时。
 * 同 stream 上多次 launch 各调一次，得到每段 host 可见墙钟（含 ACL）；
 * 设备侧逐 kernel 仍以 msprof `kernel_details.csv` 为准。
 */
inline aclError TimedSynchronizeStream(aclrtStream stream, const char *launchName)
{
    const auto t0 = std::chrono::steady_clock::now();
    const aclError e = aclrtSynchronizeStream(stream);
    const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    EmitLaunchMetric(launchName, us, e);
    return e;
}

/** 环境开关：值为 "1" 时为真（如 F203_L18_TRACE=1）。 */
inline bool EnvFlagOn(const char *name)
{
    const char *v = std::getenv(name);
    return v != nullptr && v[0] == '1' && v[1] == '\0';
}

/**
 * 阻塞等 stream 结束；若 traceDev 非空，每隔 pollMs 把 int32 槽位 D2H 打到 stderr，
 * 用于定位 `f203_encrypt_l18_l19` CrossCore 卡在哪一 FusedTraceStage。
 * @return aclrtSynchronizeStream 的返回码（轮询线程侧）。
 */
inline aclError SynchronizeStreamMaybeTrace(aclrtStream stream, void *traceDev, int32_t *traceHost, int nStages,
                                            int pollMs = 500)
{
    if (traceDev == nullptr || traceHost == nullptr || nStages <= 0) {
        return aclrtSynchronizeStream(stream);
    }

    std::atomic<aclError> syncRc{ACL_ERROR_NONE};
    std::atomic<bool> done{false};
    std::thread syncer([&]() {
        syncRc.store(aclrtSynchronizeStream(stream), std::memory_order_release);
        done.store(true, std::memory_order_release);
    });

    int lastPop = -1;
    while (!done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
        if (done.load(std::memory_order_acquire)) {
            break;
        }
        std::memset(traceHost, 0, static_cast<size_t>(nStages) * sizeof(int32_t));
        (void)aclrtMemcpy(traceHost, static_cast<size_t>(nStages) * sizeof(int32_t), traceDev,
                          static_cast<size_t>(nStages) * sizeof(int32_t), ACL_MEMCPY_DEVICE_TO_HOST);
        int pop = 0;
        for (int i = 0; i < nStages; ++i) {
            if (traceHost[i] != 0) {
                ++pop;
            }
        }
        if (pop != lastPop) {
            lastPop = pop;
            std::fprintf(stderr, "[l18-trace] stages set=%d/%d :", pop, nStages);
            for (int i = 0; i < nStages; ++i) {
                if (traceHost[i] != 0) {
                    std::fprintf(stderr, " %d", i);
                }
            }
            std::fprintf(stderr, "\n");
            std::fflush(stderr);
        }
    }
    syncer.join();
    return syncRc.load(std::memory_order_acquire);
}

/**
 * 与 SynchronizeStreamMaybeTrace 相同，额外打 `[npu_launch]`。
 * l18 卡住时 trace 仍走 stderr；duration_us 含等待时间（卡死则接近超时预算）。
 */
inline aclError TimedSynchronizeStreamMaybeTrace(aclrtStream stream, void *traceDev, int32_t *traceHost, int nStages,
                                                const char *launchName, int pollMs = 500)
{
    const auto t0 = std::chrono::steady_clock::now();
    const aclError e = SynchronizeStreamMaybeTrace(stream, traceDev, traceHost, nStages, pollMs);
    const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    EmitLaunchMetric(launchName != nullptr ? launchName : "l18_l19", us, e);
    return e;
}

}  // namespace ascendc_acl
