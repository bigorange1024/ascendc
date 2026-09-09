/**
 * @file main.cpp
 * @brief RB-D02 Host：预喂 u/ŝ/ζ/γ + mat → 单 launch MIX → 落盘 û/ŵ / TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM：ACLRT_LAUNCH_KERNEL。
 * 禁预填最终 û/ŵ（启动前清零）；验收靠设备写出后与 golden 对拍。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_dec_ntt_dot_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void dec_ntt_dot_custom(GM_ADDR out, GM_ADDR uHatOut, GM_ADDR wHatOut, GM_ADDR ws,
                                   TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 GATE / NTT / DONE。
 */
static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE        = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] AIV0_PRE_SET4   = 0x%08X", SLOT_AIV0_PRE_SET4, tr[SLOT_AIV0_PRE_SET4]);
    INFO_LOG("  [%u] AIV0_PRE_SET1   = 0x%08X", SLOT_AIV0_PRE_SET1, tr[SLOT_AIV0_PRE_SET1]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3 = 0x%08X", SLOT_AIV0_POST_WAIT3, tr[SLOT_AIV0_POST_WAIT3]);
    INFO_LOG("  [%u] AIV0_DONE       = 0x%08X", SLOT_AIV0_DONE, tr[SLOT_AIV0_DONE]);
    INFO_LOG("  [%u] HOST_POST_SYNC  = 0x%08X", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);

    const bool gateSeen =
        ((tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4) ||
         (tr[SLOT_AIV1_PRE_SET4] == MAGIC_AIV1_PRE_SET4));
    const bool nttDone =
        ((tr[SLOT_AIV0_PRE_SET1] == MAGIC_AIV0_PRE_SET1) ||
         (tr[SLOT_AIV1_PRE_SET1] == MAGIC_AIV1_PRE_SET1)) &&
        ((tr[SLOT_AIV0_POST_WAIT3] == MAGIC_AIV0_POST_WAIT3) ||
         (tr[SLOT_AIV1_POST_WAIT3] == MAGIC_AIV1_POST_WAIT3));
    const bool done = (tr[SLOT_AIV0_DONE] == MAGIC_AIV0_DONE);

    if (gateSeen) {
        INFO_LOG("causal: GATE SET4 seen");
    }
    if (nttDone) {
        INFO_LOG("causal: NTT SET1+WAIT3 seen");
    }
    if (nttDone && done) {
        INFO_LOG("causal: device NTT(u)+su_dot wrote û/ŵ");
        INFO_LOG("phases: GATE(4)->NTT(1/3)->DOT all linked");
    }
}

/**
 * 装填 Host 侧 ws：u + ŝ + ζ + γ + mat_a/b；**不**写入最终 û/ŵ。
 * @return false 读盘失败
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    if (!ReadFile("./input/u.bin", got, ws + OFF_U, kUBytes) || got != kUBytes) {
        return false;
    }
    if (!ReadFile("./input/s_hat.bin", got, ws + OFF_S_HAT, kSHatBytes) || got != kSHatBytes) {
        return false;
    }
    if (!ReadFile("./input/zetas.bin", got, ws + OFF_ZETAS, kZetasBytes) || got != kZetasBytes) {
        return false;
    }
    if (!ReadFile("./input/gammas.bin", got, ws + OFF_GAMMAS, kGammasBytes) ||
        got != kGammasBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    // 明确清零 û/ŵ 区，防止 Host 残留冒充设备结果
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes);
    std::memset(ws + OFF_W_HAT, 0, kWHatBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 主流程：读 input → launch → 落盘 out/u_hat/w_hat/trace/mat_c。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    using namespace tiling;
    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    std::memset(tilingHost, 0, tilingSize);
    TilingData *tiling = reinterpret_cast<TilingData *>(tilingHost);

    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutBytes > 1024 ? kOutBytes : 1024));
    uint8_t *uHatOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kUHatBytes > 1024 ? kUHatBytes : 1024));
    uint8_t *wHatOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWHatBytes > 1024 ? kWHatBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(out, 0, kOutBytes);
    std::memset(uHatOut, 0, kUHatBytes);
    std::memset(wHatOut, 0, kWHatBytes);
    std::memset(ws, 0, wssize);

    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX decrypt-ntt-dot (CPU twin)");
    ICPU_RUN_KF(dec_ntt_dot_custom, blockDim, out, uHatOut, wHatOut, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/u_hat.bin", uHatOut, kUHatBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/w_hat.bin", wHatOut, kWHatBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/mat_c.bin", ws + OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 18;
    }
    AscendC::GmFree(out);
    AscendC::GmFree(uHatOut);
    AscendC::GmFree(wHatOut);
    AscendC::GmFree(ws);
    AscendC::GmFree(tilingHost);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tiling = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tiling), tilingSize));
    std::memset(tiling, 0, tilingSize);

    uint8_t *outHost = nullptr, *uHatHost = nullptr, *wHatHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *uHatDev = nullptr, *wHatDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHatHost), kUHatBytes));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&uHatDev), kUHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wHatHost), kWHatBytes));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&wHatDev), kWHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, kOutBytes);
    std::memset(uHatHost, 0, kUHatBytes);
    std::memset(wHatHost, 0, kWHatBytes);
    std::memset(wsHost, 0, wssize);

    if (!LoadWorkspaceHost(wsHost)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uHatDev, kUHatBytes, uHatHost, kUHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wHatDev, kWHatBytes, wHatHost, kWHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX decrypt-ntt-dot (SIM)");
    ACLRT_LAUNCH_KERNEL(dec_ntt_dot_custom)
    (blockDim, stream, outDev, uHatDev, wHatDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHatHost, kUHatBytes, uHatDev, kUHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wHatHost, kWHatBytes, wHatDev, kWHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/u_hat.bin", uHatHost, kUHatBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/w_hat.bin", wHatHost, kWHatBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/trace.bin", wsHost + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/mat_c.bin", wsHost + OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 18;
    }

    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(uHatDev));
    CHECK_ACL(aclrtFreeHost(uHatHost));
    CHECK_ACL(aclrtFree(wHatDev));
    CHECK_ACL(aclrtFreeHost(wHatHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
