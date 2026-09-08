/**
 * @file main.cpp
 * @brief RB-T10 Host：预喂 Â/ŷ/t̂/e/μ/ζ/γ + mat → 单 launch MIX → 落盘 u,v / TRACE。
 *
 * CPU：ICPU_RUN_KF + MIX_MODE；SIM：ACLRT_LAUNCH_KERNEL。
 * 禁预填最终 u,v（uvOut 启动前清零）；验收靠设备写出后与 golden 对拍。
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
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR uvOut, GM_ADDR ws, TilingData tiling);
#endif

/**
 * 打印 TRACE 槽，并据因果链显式打出 NTT / MUL / GATE / INTT / UV。
 */
static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE             = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] AIV0_PRE_SET1_NTT    = 0x%08X", SLOT_AIV0_PRE_SET1_NTT, tr[SLOT_AIV0_PRE_SET1_NTT]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3_NTT  = 0x%08X", SLOT_AIV0_POST_WAIT3_NTT, tr[SLOT_AIV0_POST_WAIT3_NTT]);
    INFO_LOG("  [%u] AIV0_MUL_DONE        = 0x%08X", SLOT_AIV0_MUL_DONE, tr[SLOT_AIV0_MUL_DONE]);
    INFO_LOG("  [%u] AIV0_PRE_SET4        = 0x%08X", SLOT_AIV0_PRE_SET4, tr[SLOT_AIV0_PRE_SET4]);
    INFO_LOG("  [%u] AIV0_PRE_SET1_INTT   = 0x%08X", SLOT_AIV0_PRE_SET1_INTT, tr[SLOT_AIV0_PRE_SET1_INTT]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3_INTT = 0x%08X", SLOT_AIV0_POST_WAIT3_INTT, tr[SLOT_AIV0_POST_WAIT3_INTT]);
    INFO_LOG("  [%u] AIV0_UV_DONE         = 0x%08X", SLOT_AIV0_UV_DONE, tr[SLOT_AIV0_UV_DONE]);
    INFO_LOG("  [%u] HOST_POST_SYNC       = 0x%08X", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);

    const bool nttDone =
        ((tr[SLOT_AIV0_PRE_SET1_NTT] == MAGIC_AIV0_PRE_SET1_NTT) ||
         (tr[SLOT_AIV1_PRE_SET1_NTT] == MAGIC_AIV1_PRE_SET1_NTT)) &&
        ((tr[SLOT_AIV0_POST_WAIT3_NTT] == MAGIC_AIV0_POST_WAIT3_NTT) ||
         (tr[SLOT_AIV1_POST_WAIT3_NTT] == MAGIC_AIV1_POST_WAIT3_NTT));
    const bool mulDone = (tr[SLOT_AIV0_MUL_DONE] == MAGIC_AIV0_MUL_DONE);
    const bool gateSeen = (tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4) ||
                          (tr[SLOT_AIV1_PRE_SET4] == MAGIC_AIV1_PRE_SET4);
    const bool inttDone =
        ((tr[SLOT_AIV0_PRE_SET1_INTT] == MAGIC_AIV0_PRE_SET1_INTT) ||
         (tr[SLOT_AIV1_PRE_SET1_INTT] == MAGIC_AIV1_PRE_SET1_INTT)) &&
        ((tr[SLOT_AIV0_POST_WAIT3_INTT] == MAGIC_AIV0_POST_WAIT3_INTT) ||
         (tr[SLOT_AIV1_POST_WAIT3_INTT] == MAGIC_AIV1_POST_WAIT3_INTT));
    const bool uvDone = (tr[SLOT_AIV0_UV_DONE] == MAGIC_AIV0_UV_DONE);

    if (nttDone) {
        INFO_LOG("causal: NTT SET1+WAIT3 seen");
    }
    if (nttDone && mulDone) {
        INFO_LOG("causal: MultiplyNTTs (device) after NTT handshake");
    }
    if (nttDone && gateSeen) {
        INFO_LOG("causal: GATE SET4 after mul");
    }
    if (nttDone && gateSeen && inttDone) {
        INFO_LOG("causal: INTT SET1+WAIT3 (1/3 reuse) linked");
    }
    if (uvDone) {
        INFO_LOG("causal: device INTT+noise wrote u,v");
        INFO_LOG("phases: NTT(1/3)->MUL->GATE(4)->INTT(1/3)->UV all linked");
    }
}

/**
 * 装填 Host 侧 ws：拓扑输入 + ζ/γ + mat_a/b；**不**写入最终 u,v。
 * @return false 读盘失败
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    if (!ReadFile("./input/a_hat.bin", got, ws + OFF_A_HAT, kAHatBytes) || got != kAHatBytes) {
        return false;
    }
    if (!ReadFile("./input/y_hat.bin", got, ws + OFF_Y_HAT, kYHatBytes) || got != kYHatBytes) {
        return false;
    }
    if (!ReadFile("./input/t_hat.bin", got, ws + OFF_T_HAT, kTHatBytes) || got != kTHatBytes) {
        return false;
    }
    if (!ReadFile("./input/e1.bin", got, ws + OFF_E1, kE1Bytes) || got != kE1Bytes) {
        return false;
    }
    if (!ReadFile("./input/e2.bin", got, ws + OFF_E2, kE2Bytes) || got != kE2Bytes) {
        return false;
    }
    if (!ReadFile("./input/mu.bin", got, ws + OFF_MU, kMuBytes) || got != kMuBytes) {
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
    // 明确清零中间/输出区，防止 Host 残留冒充设备结果
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes + kVHatBytes + kUBytes + kVBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 主流程：读 input → launch → 落盘 out/uv/trace/mat_c_*。
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
    uint8_t *uvOut =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kUvOutBytes > 1024 ? kUvOutBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(out, 0, kOutBytes);
    std::memset(uvOut, 0, kUvOutBytes);
    std::memset(ws, 0, wssize);

    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX uv-device-mix (CPU twin)");
    ICPU_RUN_KF(mmad_custom, blockDim, out, uvOut, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/u.bin", uvOut, kUBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/v.bin", uvOut + kUBytes, kVBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", ws + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/mat_c_intt.bin", ws + OFF_MAT_C_INTT, kMatCBytes);
    if (!ok) {
        return 19;
    }
    AscendC::GmFree(out);
    AscendC::GmFree(uvOut);
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

    uint8_t *outHost = nullptr, *uvHost = nullptr, *wsHost = nullptr;
    uint8_t *outDev = nullptr, *uvDev = nullptr, *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uvHost), kUvOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uvDev), kUvOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(outHost, 0, kOutBytes);
    std::memset(uvHost, 0, kUvOutBytes);
    std::memset(wsHost, 0, wssize);

    if (!LoadWorkspaceHost(wsHost)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uvDev, kUvOutBytes, uvHost, kUvOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: TRACE HOST_PRE marked, launching MIX uv-device-mix (SIM)");
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDev, uvDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uvHost, kUvOutBytes, uvDev, kUvOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST_SYNC;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/u.bin", uvHost, kUBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/v.bin", uvHost + kUBytes, kVBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/trace.bin", wsHost + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", wsHost + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/mat_c_intt.bin", wsHost + OFF_MAT_C_INTT, kMatCBytes);
    if (!ok) {
        return 19;
    }

    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(uvDev));
    CHECK_ACL(aclrtFreeHost(uvHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
