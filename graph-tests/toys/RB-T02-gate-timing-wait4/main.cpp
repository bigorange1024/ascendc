/**
 * @file main.cpp
 * @brief RB-T02 Host：装填 MAT_A/B + TRACE 预标 → 单 launch → SynchronizeStream → 打印 TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM：ACLRT_LAUNCH_KERNEL（本刀不跑 NPU）。
 * 验收：exit 0 + TRACE 槽可见 SET4 / WAIT3 / sync 后魔数；causal 覆盖 Wait4+1/3。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR unused, GM_ADDR ws, TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 Wait4 / Wait1 / Set3。
 * @param tr TRACE 数组
 */
static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE        = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] AIV0_PRE_SET4   = 0x%08X  (SET4)", SLOT_AIV0_PRE_SET4, tr[SLOT_AIV0_PRE_SET4]);
    INFO_LOG("  [%u] AIV1_PRE_SET4   = 0x%08X  (SET4)", SLOT_AIV1_PRE_SET4, tr[SLOT_AIV1_PRE_SET4]);
    INFO_LOG("  [%u] AIC_POST_WAIT4  = 0x%08X  (WAIT4 direct)", SLOT_AIC_POST_WAIT4,
             tr[SLOT_AIC_POST_WAIT4]);
    INFO_LOG("  [%u] AIC_PRE_SET3    = 0x%08X  (SET3 direct)", SLOT_AIC_PRE_SET3, tr[SLOT_AIC_PRE_SET3]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3 = 0x%08X  (after WAIT3)", SLOT_AIV0_POST_WAIT3,
             tr[SLOT_AIV0_POST_WAIT3]);
    INFO_LOG("  [%u] AIV1_POST_WAIT3 = 0x%08X  (after WAIT3)", SLOT_AIV1_POST_WAIT3,
             tr[SLOT_AIV1_POST_WAIT3]);
    INFO_LOG("  [%u] HOST_POST_SYNC  = 0x%08X  (sync 后)", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);
    INFO_LOG("  [%u] AIV0_PRE_SET1   = 0x%08X  (SET1)", SLOT_AIV0_PRE_SET1, tr[SLOT_AIV0_PRE_SET1]);
    INFO_LOG("  [%u] AIV1_PRE_SET1   = 0x%08X  (SET1)", SLOT_AIV1_PRE_SET1, tr[SLOT_AIV1_PRE_SET1]);
    INFO_LOG("  [%u] AIC_POST_WAIT1  = 0x%08X  (WAIT1 direct)", SLOT_AIC_POST_WAIT1,
             tr[SLOT_AIC_POST_WAIT1]);

    // 因果：AIV 已 SET4 且已过 WAIT3 ⇒ AIC 必已 WAIT4 + Cube + WAIT1 + SET3。
    // 成对槽 AIV0|AIV1 任一命中即可（NPU TRACE 可能落 AIV1，勿硬绑 AIV0 / KB X7/X9）。
    const bool set4 = (tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4) ||
                      (tr[SLOT_AIV1_PRE_SET4] == MAGIC_AIV1_PRE_SET4);
    const bool wait3 = (tr[SLOT_AIV0_POST_WAIT3] == MAGIC_AIV0_POST_WAIT3) ||
                       (tr[SLOT_AIV1_POST_WAIT3] == MAGIC_AIV1_POST_WAIT3);
    if (set4 && wait3) {
        INFO_LOG("causal: SET4 seen + WAIT3 seen => AIC WAIT4 + Cube + WAIT1 + SET3 completed");
        INFO_LOG("gate: AIC entered Wait(4) before AIV Set(4) (expected; woken by Set)");
    }
    if (tr[SLOT_AIC_POST_WAIT4] == MAGIC_AIC_POST_WAIT4) {
        INFO_LOG("direct: WAIT4 TRACE slot set");
    }
    if (tr[SLOT_AIC_PRE_SET3] == MAGIC_AIC_PRE_SET3) {
        INFO_LOG("direct: SET3 TRACE slot set");
    }
}

/**
 * 主流程：读 input → launch → 落盘 out/trace/mat_c → 打印 TRACE。
 * @return 0 成功；非 0 读盘/写盘失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    using namespace tiling;
    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t outFileSize = kOutBytes;
    size_t unusedSize = 64;
    size_t wsFileSize = wssize;
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    std::memset(tilingHost, 0, tilingSize);
    TilingData *tiling = reinterpret_cast<TilingData *>(tilingHost);

    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024));
    uint8_t *unused =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(unusedSize > 1024 ? unusedSize : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024));
    std::memset(out, 0, outFileSize);
    std::memset(unused, 0, unusedSize);
    std::memset(ws, 0, wsFileSize);

    size_t got = 0;
    ok = ReadFile("./input/mat_a.bin", got, ws + MAT_A, kMatABytes);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/mat_b.bin", got, ws + MAT_B, kMatBBytes);
    if (!ok) {
        return 10;
    }

    auto *tr = reinterpret_cast<uint32_t *>(ws + TRACE);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX GATE kernel (CPU twin)");
    ICPU_RUN_KF(mmad_custom, blockDim, out, unused, ws, *tiling);

    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/trace.bin", ws + TRACE, kTraceBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/mat_c.bin", ws + MAT_C, kMatCBytes);
    if (!ok) {
        return 16;
    }
    AscendC::GmFree(out);
    AscendC::GmFree(unused);
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

    uint8_t *outHost = nullptr, *unusedHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *unusedDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&unusedHost), unusedSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&unusedDev), unusedSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, outFileSize);
    std::memset(unusedHost, 0, unusedSize);
    std::memset(wsHost, 0, wsFileSize);

    size_t got = 0;
    ok = ReadFile("./input/mat_a.bin", got, wsHost + MAT_A, kMatABytes);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/mat_b.bin", got, wsHost + MAT_B, kMatBBytes);
    if (!ok) {
        return 10;
    }
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + TRACE);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;

    CHECK_ACL(aclrtMemcpy(outDev, outFileSize, outHost, outFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(unusedDev, unusedSize, unusedHost, unusedSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX GATE kernel (SIM)");
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDev, unusedDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned (sync 后)");

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDev, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wsFileSize, wsDev, wsFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/trace.bin", wsHost + TRACE, kTraceBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/mat_c.bin", wsHost + MAT_C, kMatCBytes);
    if (!ok) {
        return 16;
    }

    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(unusedDev));
    CHECK_ACL(aclrtFreeHost(unusedHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
