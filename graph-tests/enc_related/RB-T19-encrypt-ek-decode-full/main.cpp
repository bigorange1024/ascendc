/**
 * @file main.cpp
 * @brief RB-T19 Host：双 launch — prep(AIV: coins+ek+ρ) → Synchronize → compute(MIX 全设备链)。
 *
 * Host 预喂：ek/coins/μ/ζ/γ/mat；禁预填最终 t̂/y/e/Â/ŷ/u/v/c。
 * CPU：AIV_MODE + MIX_MODE；SIM/NPU：同 session 单库两次 ACLRT_LAUNCH_KERNEL。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_compute_custom.h"
#include "aclrtlaunch_prep_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void prep_custom(GM_ADDR coinsIn, GM_ADDR ekIn, GM_ADDR ws, TilingData tiling);
extern "C" void compute_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ws, TilingData tiling);
#endif

static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map dump (slot=magic):");
    INFO_LOG("  [%u] HOST_PRE             = 0x%08X", SLOT_HOST_PRE, tr[SLOT_HOST_PRE]);
    INFO_LOG("  [%u] PREP_DONE            = 0x%08X", SLOT_PREP_DONE, tr[SLOT_PREP_DONE]);
    INFO_LOG("  [%u] AIV0_PRE_SET1_NTT    = 0x%08X", SLOT_AIV0_PRE_SET1_NTT, tr[SLOT_AIV0_PRE_SET1_NTT]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3_NTT  = 0x%08X", SLOT_AIV0_POST_WAIT3_NTT, tr[SLOT_AIV0_POST_WAIT3_NTT]);
    INFO_LOG("  [%u] AIV0_BD12_DONE       = 0x%08X", SLOT_AIV0_BD12_DONE, tr[SLOT_AIV0_BD12_DONE]);
    INFO_LOG("  [%u] AIV0_CBD_DONE        = 0x%08X", SLOT_AIV0_CBD_DONE, tr[SLOT_AIV0_CBD_DONE]);
    INFO_LOG("  [%u] AIV0_AHAT_DONE       = 0x%08X", SLOT_AIV0_AHAT_DONE, tr[SLOT_AIV0_AHAT_DONE]);
    INFO_LOG("  [%u] AIV0_YHAT_DONE       = 0x%08X", SLOT_AIV0_YHAT_DONE, tr[SLOT_AIV0_YHAT_DONE]);
    INFO_LOG("  [%u] AIV0_MUL_DONE        = 0x%08X", SLOT_AIV0_MUL_DONE, tr[SLOT_AIV0_MUL_DONE]);
    INFO_LOG("  [%u] AIV0_PRE_SET4        = 0x%08X", SLOT_AIV0_PRE_SET4, tr[SLOT_AIV0_PRE_SET4]);
    INFO_LOG("  [%u] HOST_MID_SYNC        = 0x%08X", SLOT_HOST_MID_SYNC, tr[SLOT_HOST_MID_SYNC]);
    INFO_LOG("  [%u] AIV0_PRE_SET1_INTT   = 0x%08X", SLOT_AIV0_PRE_SET1_INTT, tr[SLOT_AIV0_PRE_SET1_INTT]);
    INFO_LOG("  [%u] AIV0_POST_WAIT3_INTT = 0x%08X", SLOT_AIV0_POST_WAIT3_INTT, tr[SLOT_AIV0_POST_WAIT3_INTT]);
    INFO_LOG("  [%u] AIV0_UV_DONE         = 0x%08X", SLOT_AIV0_UV_DONE, tr[SLOT_AIV0_UV_DONE]);
    INFO_LOG("  [%u] AIV0_PACK_DONE       = 0x%08X", SLOT_AIV0_PACK_DONE, tr[SLOT_AIV0_PACK_DONE]);
    INFO_LOG("  [%u] HOST_POST_SYNC       = 0x%08X", SLOT_HOST_POST_SYNC, tr[SLOT_HOST_POST_SYNC]);

    const bool prepOk = (tr[SLOT_PREP_DONE] == MAGIC_PREP_DONE);
    const bool nttDone = (tr[SLOT_AIV0_PRE_SET1_NTT] == MAGIC_AIV0_PRE_SET1_NTT) &&
                         (tr[SLOT_AIV0_POST_WAIT3_NTT] == MAGIC_AIV0_POST_WAIT3_NTT);
    const bool bd12Done = (tr[SLOT_AIV0_BD12_DONE] == MAGIC_AIV0_BD12_DONE);
    const bool cbdDone = (tr[SLOT_AIV0_CBD_DONE] == MAGIC_AIV0_CBD_DONE);
    const bool ahatDone = (tr[SLOT_AIV0_AHAT_DONE] == MAGIC_AIV0_AHAT_DONE);
    const bool yhatDone = (tr[SLOT_AIV0_YHAT_DONE] == MAGIC_AIV0_YHAT_DONE);
    const bool mulDone = (tr[SLOT_AIV0_MUL_DONE] == MAGIC_AIV0_MUL_DONE);
    const bool gateSeen = (tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4);
    const bool inttDone = (tr[SLOT_AIV0_PRE_SET1_INTT] == MAGIC_AIV0_PRE_SET1_INTT) &&
                          (tr[SLOT_AIV0_POST_WAIT3_INTT] == MAGIC_AIV0_POST_WAIT3_INTT);
    const bool uvDone = (tr[SLOT_AIV0_UV_DONE] == MAGIC_AIV0_UV_DONE);
    const bool packDone = (tr[SLOT_AIV0_PACK_DONE] == MAGIC_AIV0_PACK_DONE);
    if (prepOk) {
        INFO_LOG("causal: Launch1 PREP_DONE seen");
    }
    if (nttDone && bd12Done) {
        INFO_LOG("causal: device ByteDecode12(ek) wrote t_hat");
    }
    if (nttDone && cbdDone) {
        INFO_LOG("causal: device CBD(coins) wrote y/e");
    }
    if (nttDone && ahatDone && yhatDone) {
        INFO_LOG("causal: device SampleNTT+NTT wrote Â,ŷ");
    }
    if (nttDone && mulDone && gateSeen && inttDone) {
        INFO_LOG("causal: Launch2 CBD->AHAT/YHAT->MUL->GATE->INTT linked");
    }
    if (uvDone) {
        INFO_LOG("causal: device INTT+noise wrote u,v");
    }
    if (packDone) {
        INFO_LOG("causal: AIV0 pack(u,v->c) done after UV");
    }
}

/**
 * 装填 Host 侧 ws：μ/ζ/γ + mat；清零 t̂/Â/ŷ/YEE/u/v/c。
 * 禁预喂最终 t̂（由设备 Decode₁₂ 写出）；coins/ek 由独立 buffer 进 prep。
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
    std::memset(ws + OFF_Y_E1_E2, 0, kYe1e2Bytes);
    std::memset(ws + OFF_A_HAT, 0, kAHatBytes);
    std::memset(ws + OFF_Y_HAT, 0, kYHatBytes);
    // 相对 T17：不再 ReadFile t_hat.bin；OFF_T_HAT 清零由设备 Decode₁₂ 填充
    std::memset(ws + OFF_T_HAT, 0, kTHatBytes);
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
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes + kVHatBytes + kUBytes + kVBytes + kCBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

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
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    std::memset(tilingHost, 0, tilingSize);
    TilingData *tiling = reinterpret_cast<TilingData *>(tilingHost);

    uint8_t *coinsIn =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCoinsBytes > 1024 ? kCoinsBytes : 1024));
    uint8_t *ekIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kEkBytes > 1024 ? kEkBytes : 1024));
    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutBytes > 1024 ? kOutBytes : 1024));
    uint8_t *cOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(coinsIn, 0, kCoinsBytes);
    std::memset(ekIn, 0, kEkBytes);
    std::memset(out, 0, kOutBytes);
    std::memset(cOut, 0, kCBytes);
    std::memset(ws, 0, wssize);

    size_t got = 0;
    ok = ReadFile("./input/coins.bin", got, coinsIn, kCoinsBytes);
    if (!ok || got != kCoinsBytes) {
        return 1;
    }
    ok = ReadFile("./input/ek.bin", got, ekIn, kEkBytes);
    if (!ok || got != kEkBytes) {
        return 2;
    }
    if (!LoadWorkspaceHost(ws)) {
        return 3;
    }

    INFO_LOG("Host: Launch1 prep_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(prep_custom, blockDim, coinsIn, ekIn, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    INFO_LOG("Host: mid-sync after Launch1, Launch2 compute_custom (CPU MIX twin)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(compute_custom, blockDim, out, cOut, ws, *tiling);

    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", out, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/c.bin", cOut, kCBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/u.bin", ws + OFF_U, kUBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/v.bin", ws + OFF_V, kVBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/a_hat.bin", ws + OFF_A_HAT, kAHatBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/y_hat.bin", ws + OFF_Y_HAT, kYHatBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/t_hat_dev.bin", ws + OFF_T_HAT, kTHatBytes);
    if (!ok) {
        return 25;
    }
    ok = WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", ws + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 21;
    }
    ok = WriteFile("./output/mat_c_intt.bin", ws + OFF_MAT_C_INTT, kMatCBytes);
    if (!ok) {
        return 22;
    }
    ok = WriteFile("./output/y_e1_e2_dev.bin", ws + OFF_Y_E1_E2, kYe1e2Bytes);
    if (!ok) {
        return 23;
    }
    ok = WriteFile("./output/rho_dev.bin", ws + OFF_RHO, kRhoBytes);
    if (!ok) {
        return 24;
    }
    AscendC::GmFree(coinsIn);
    AscendC::GmFree(ekIn);
    AscendC::GmFree(out);
    AscendC::GmFree(cOut);
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

    uint8_t *coinsHost = nullptr, *ekHost = nullptr, *outHost = nullptr, *cHost = nullptr,
            *wsHost = nullptr;
    uint8_t *coinsDev = nullptr, *ekDev = nullptr, *outDev = nullptr, *cDev = nullptr, *wsDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&coinsHost), kCoinsBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&coinsDev), kCoinsBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(coinsHost, 0, kCoinsBytes);
    std::memset(ekHost, 0, kEkBytes);
    std::memset(outHost, 0, kOutBytes);
    std::memset(cHost, 0, kCBytes);
    std::memset(wsHost, 0, wssize);

    size_t got = 0;
    ok = ReadFile("./input/coins.bin", got, coinsHost, kCoinsBytes);
    if (!ok || got != kCoinsBytes) {
        return 1;
    }
    ok = ReadFile("./input/ek.bin", got, ekHost, kEkBytes);
    if (!ok || got != kEkBytes) {
        return 2;
    }
    if (!LoadWorkspaceHost(wsHost)) {
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(coinsDev, kCoinsBytes, coinsHost, kCoinsBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, kCBytes, cHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch1 prep_custom (SIM/NPU)");
    ACLRT_LAUNCH_KERNEL(prep_custom)(blockDim, stream, coinsDev, ekDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    CHECK_ACL(aclrtMemcpy(wsDev + OFF_TRACE, kTraceBytes, wsHost + OFF_TRACE, kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch2 compute_custom (SIM/NPU)");
    ACLRT_LAUNCH_KERNEL(compute_custom)(blockDim, stream, outDev, cDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned after Launch2");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cHost, kCBytes, cDev, kCBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    tr[SLOT_HOST_POST_SYNC] = MAGIC_HOST_POST;
    PrintTrace(tr);

    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/c.bin", cHost, kCBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/u.bin", wsHost + OFF_U, kUBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/v.bin", wsHost + OFF_V, kVBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/a_hat.bin", wsHost + OFF_A_HAT, kAHatBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/y_hat.bin", wsHost + OFF_Y_HAT, kYHatBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/t_hat_dev.bin", wsHost + OFF_T_HAT, kTHatBytes);
    if (!ok) {
        return 25;
    }
    ok = WriteFile("./output/trace.bin", wsHost + OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_ntt.bin", wsHost + OFF_MAT_C_NTT, kMatCBytes);
    if (!ok) {
        return 21;
    }
    ok = WriteFile("./output/mat_c_intt.bin", wsHost + OFF_MAT_C_INTT, kMatCBytes);
    if (!ok) {
        return 22;
    }
    ok = WriteFile("./output/y_e1_e2_dev.bin", wsHost + OFF_Y_E1_E2, kYe1e2Bytes);
    if (!ok) {
        return 23;
    }
    ok = WriteFile("./output/rho_dev.bin", wsHost + OFF_RHO, kRhoBytes);
    if (!ok) {
        return 24;
    }

    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFreeHost(coinsHost));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
