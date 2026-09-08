/**
 * @file main.cpp
 * @brief RB-T16 Host：预喂 coins + mat → 单 launch MIX → 落盘 y_e1_e2 / TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM/NPU：ACLRT_LAUNCH_KERNEL。
 * 禁预填最终 y/e1/e2（yeeOut 启动前清零）；验收靠设备写出后与 golden 对拍。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR yeeOut, GM_ADDR ws, TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 CBD 握手 / CBD_DONE。
 */
static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE          = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] AIV0_PRE_SET1     = 0x%08X", SLOT_AIV0_PRE_SET1, tr[SLOT_AIV0_PRE_SET1]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3   = 0x%08X", SLOT_AIV0_POST_WAIT3, tr[SLOT_AIV0_POST_WAIT3]);
    INFO_LOG("  [%u] AIV0_CBD_DONE     = 0x%08X", SLOT_AIV0_CBD_DONE, tr[SLOT_AIV0_CBD_DONE]);
    INFO_LOG("  [%u] HOST_POST_SYNC    = 0x%08X", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);

    const bool handDone =
        ((tr[SLOT_AIV0_PRE_SET1] == MAGIC_AIV0_PRE_SET1) ||
         (tr[SLOT_AIV1_PRE_SET1] == MAGIC_AIV1_PRE_SET1)) &&
        ((tr[SLOT_AIV0_POST_WAIT3] == MAGIC_AIV0_POST_WAIT3) ||
         (tr[SLOT_AIV1_POST_WAIT3] == MAGIC_AIV1_POST_WAIT3));
    const bool cbdDone = (tr[SLOT_AIV0_CBD_DONE] == MAGIC_AIV0_CBD_DONE);

    if (handDone) {
        INFO_LOG("causal: CBD SET1+WAIT3 seen");
    }
    if (handDone && cbdDone) {
        INFO_LOG("causal: device CBD(coins) wrote y/e1/e2");
        INFO_LOG("phases: CBD(1/3)->YEE all linked");
    }
}

/**
 * 装填 Host 侧 ws：coins + mat_a/b；**不**写入最终 y/e1/e2。
 * @return false 读盘失败
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    if (!ReadFile("./input/coins.bin", got, ws + OFF_COINS, kCoinsBytes) || got != kCoinsBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    // 明确清零 PRF / YEE 区，防止 Host 残留冒充设备结果
    std::memset(ws + OFF_PRF, 0, kPrfTotalBytes);
    std::memset(ws + OFF_YEE, 0, kYeeBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 主流程：读 input → launch → 落盘 out/y_e1_e2/y/e1/e2/trace/mat_c。
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
    uint8_t *yeeOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kYeeBytes > 1024 ? kYeeBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(out, 0, kOutBytes);
    std::memset(yeeOut, 0, kYeeBytes);
    std::memset(ws, 0, wssize);

    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX device-cbd-y-e (CPU twin)");
    ICPU_RUN_KF(mmad_custom, blockDim, out, yeeOut, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/y_e1_e2.bin", yeeOut, kYeeBytes);
    if (!ok) {
        return 15;
    }
    // 拆分落盘对齐 T07 消费方
    ok = WriteFile("./output/y.bin", yeeOut, kYBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/e1.bin", yeeOut + kYBytes, kE1Bytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/e2.bin", yeeOut + kYBytes + kE1Bytes, kE2Bytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c.bin", ws + OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 20;
    }
    AscendC::GmFree(out);
    AscendC::GmFree(yeeOut);
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

    uint8_t *outHost = nullptr, *yeeHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *yeeDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&yeeHost), kYeeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yeeDev), kYeeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, kOutBytes);
    std::memset(yeeHost, 0, kYeeBytes);
    std::memset(wsHost, 0, wssize);

    if (!LoadWorkspaceHost(wsHost)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(yeeDev, kYeeBytes, yeeHost, kYeeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX device-cbd-y-e (SIM/NPU)");
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDev, yeeDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(yeeHost, kYeeBytes, yeeDev, kYeeBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/y_e1_e2.bin", yeeHost, kYeeBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/y.bin", yeeHost, kYBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/e1.bin", yeeHost + kYBytes, kE1Bytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/e2.bin", yeeHost + kYBytes + kE1Bytes, kE2Bytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace.bin", wsHost + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c.bin", wsHost + OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 20;
    }

    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFree(yeeDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFreeHost(yeeHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif

    INFO_LOG("RB-T16 device-cbd-y-e done");
    return 0;
}
