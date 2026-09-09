/**
 * @file main.cpp
 * @brief RB-K02 Host：预喂 ŝ/ê/ζ + mat → 单 launch MIX → 落盘 NTT(ŝ)/NTT(ê)/TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM/NPU：ACLRT_LAUNCH_KERNEL。
 * 禁预填最终 NTT 结果（sNttOut/eNttOut 启动前清零）；验收靠设备写出后与 golden 对拍。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_kg_ntt_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void kg_ntt_custom(GM_ADDR out, GM_ADDR sNttOut, GM_ADDR eNttOut, GM_ADDR ws,
                              TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 GATE/NTT/DONE。
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
    const bool seDone = (tr[SLOT_AIV0_DONE] == MAGIC_AIV0_DONE);

    if (gateSeen) {
        INFO_LOG("causal: GATE SET4 seen");
    }
    if (nttDone) {
        INFO_LOG("causal: NTT SET1+WAIT3 seen");
    }
    if (nttDone && seDone) {
        INFO_LOG("causal: device NTT(s)+NTT(e) wrote");
        INFO_LOG("phases: GATE(4)->NTT(1/3)->SE_NTT all linked");
    }
}

/**
 * 装填 Host 侧 ws：ŝ + ê + ζ + mat_a/b；**不**写入最终 NTT 结果。
 * @return false 读盘失败
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    if (!ReadFile("./input/s.bin", got, ws + OFF_S, kSBytes) || got != kSBytes) {
        return false;
    }
    if (!ReadFile("./input/e.bin", got, ws + OFF_E, kEBytes) || got != kEBytes) {
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
    // 明确清零 NTT 区，防止 Host 残留冒充设备结果
    std::memset(ws + OFF_S_NTT, 0, kSNttBytes);
    std::memset(ws + OFF_E_NTT, 0, kENttBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 主流程：读 input → launch → 落盘 out / s_ntt / e_ntt / trace / mat_c。
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
    uint8_t *sNttOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSNttBytes > 1024 ? kSNttBytes : 1024));
    uint8_t *eNttOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kENttBytes > 1024 ? kENttBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(out, 0, kOutBytes);
    std::memset(sNttOut, 0, kSNttBytes);
    std::memset(eNttOut, 0, kENttBytes);
    std::memset(ws, 0, wssize);

    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX kg-ntt (CPU twin)");
    ICPU_RUN_KF(kg_ntt_custom, blockDim, out, sNttOut, eNttOut, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/s_ntt.bin", sNttOut, kSNttBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/e_ntt.bin", eNttOut, kENttBytes);
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
    AscendC::GmFree(sNttOut);
    AscendC::GmFree(eNttOut);
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

    uint8_t *outHost = nullptr, *sNttHost = nullptr, *eNttHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *sNttDev = nullptr, *eNttDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&sNttHost), kSNttBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sNttDev), kSNttBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&eNttHost), kENttBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&eNttDev), kENttBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, kOutBytes);
    std::memset(sNttHost, 0, kSNttBytes);
    std::memset(eNttHost, 0, kENttBytes);
    std::memset(wsHost, 0, wssize);

    if (!LoadWorkspaceHost(wsHost)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sNttDev, kSNttBytes, sNttHost, kSNttBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(eNttDev, kENttBytes, eNttHost, kENttBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX kg-ntt (SIM/NPU)");
    ACLRT_LAUNCH_KERNEL(kg_ntt_custom)(blockDim, stream, outDev, sNttDev, eNttDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(sNttHost, kSNttBytes, sNttDev, kSNttBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(eNttHost, kENttBytes, eNttDev, kENttBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/s_ntt.bin", sNttHost, kSNttBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/e_ntt.bin", eNttHost, kENttBytes);
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
    CHECK_ACL(aclrtFree(sNttDev));
    CHECK_ACL(aclrtFreeHost(sNttHost));
    CHECK_ACL(aclrtFree(eNttDev));
    CHECK_ACL(aclrtFreeHost(eNttHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
