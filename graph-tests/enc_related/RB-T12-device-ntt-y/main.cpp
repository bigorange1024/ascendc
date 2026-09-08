/**
 * @file main.cpp
 * @brief RB-T12 Host：预喂 y/ζ + mat → 单 launch MIX → 落盘 ŷ / TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM：ACLRT_LAUNCH_KERNEL。
 * 禁预填最终 ŷ（yHatOut 启动前清零）；验收靠设备写出后与 golden 对拍。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR yHatOut, GM_ADDR ws, TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 NTT / YHAT。
 */
static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE             = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] AIV0_PRE_SET1_NTT    = 0x%08X", SLOT_AIV0_PRE_SET1_NTT, tr[SLOT_AIV0_PRE_SET1_NTT]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3_NTT  = 0x%08X", SLOT_AIV0_POST_WAIT3_NTT, tr[SLOT_AIV0_POST_WAIT3_NTT]);
    INFO_LOG("  [%u] AIV0_YHAT_DONE       = 0x%08X", SLOT_AIV0_YHAT_DONE, tr[SLOT_AIV0_YHAT_DONE]);
    INFO_LOG("  [%u] HOST_POST_SYNC       = 0x%08X", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);

    const bool nttDone =
        ((tr[SLOT_AIV0_PRE_SET1_NTT] == MAGIC_AIV0_PRE_SET1_NTT) ||
         (tr[SLOT_AIV1_PRE_SET1_NTT] == MAGIC_AIV1_PRE_SET1_NTT)) &&
        ((tr[SLOT_AIV0_POST_WAIT3_NTT] == MAGIC_AIV0_POST_WAIT3_NTT) ||
         (tr[SLOT_AIV1_POST_WAIT3_NTT] == MAGIC_AIV1_POST_WAIT3_NTT));
    const bool yhatDone = (tr[SLOT_AIV0_YHAT_DONE] == MAGIC_AIV0_YHAT_DONE);

    if (nttDone) {
        INFO_LOG("causal: NTT SET1+WAIT3 seen");
    }
    if (nttDone && yhatDone) {
        INFO_LOG("causal: device NTT(y) wrote ŷ");
        INFO_LOG("phases: NTT(1/3)->YHAT all linked");
    }
}

/**
 * 装填 Host 侧 ws：y + ζ + mat_a/b；**不**写入最终 ŷ。
 * @return false 读盘失败
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    if (!ReadFile("./input/y.bin", got, ws + OFF_Y, kYBytes) || got != kYBytes) {
        return false;
    }
    if (!ReadFile("./input/zetas.bin", got, ws + OFF_ZETAS, kZetasBytes) || got != kZetasBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    // 明确清零 ŷ 区，防止 Host 残留冒充设备结果
    std::memset(ws + OFF_Y_HAT, 0, kYHatBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 主流程：读 input → launch → 落盘 out/y_hat/trace/mat_c_ntt。
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
    uint8_t *yHatOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kYHatBytes > 1024 ? kYHatBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(out, 0, kOutBytes);
    std::memset(yHatOut, 0, kYHatBytes);
    std::memset(ws, 0, wssize);

    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX device-ntt-y (CPU twin)");
    ICPU_RUN_KF(mmad_custom, blockDim, out, yHatOut, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/y_hat.bin", yHatOut, kYHatBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", ws + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 17;
    }
    AscendC::GmFree(out);
    AscendC::GmFree(yHatOut);
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

    uint8_t *outHost = nullptr, *yHatHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *yHatDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yHatHost), kYHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yHatDev), kYHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, kOutBytes);
    std::memset(yHatHost, 0, kYHatBytes);
    std::memset(wsHost, 0, wssize);

    if (!LoadWorkspaceHost(wsHost)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(yHatDev, kYHatBytes, yHatHost, kYHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX device-ntt-y (SIM)");
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDev, yHatDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(yHatHost, kYHatBytes, yHatDev, kYHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/y_hat.bin", yHatHost, kYHatBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/trace.bin", wsHost + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", wsHost + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 17;
    }

    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(yHatDev));
    CHECK_ACL(aclrtFreeHost(yHatHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
