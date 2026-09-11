/**
 * @file main.cpp
 * @brief RB-T25 Host：三 launch Decrypt — prep → sync → NTT → sync → INTT+extract。
 *
 * 输入：dk_pke + c + ζ/γ/mat；禁预喂最终 m。
 * 输出：m[32] + TRACE；golden 由 liboqs PKE Decrypt 提供。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_intt_custom.h"
#include "aclrtlaunch_ntt_custom.h"
#include "aclrtlaunch_prep_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void prep_custom(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws, TilingData tiling);
extern "C" void ntt_custom(GM_ADDR out, GM_ADDR ws, TilingData tiling);
extern "C" void intt_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws, TilingData tiling);
#endif

static void PrintTrace(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("TRACE map:");
    INFO_LOG("  PREP_DONE           = 0x%08X", tr[SLOT_PREP_DONE]);
    INFO_LOG("  AIV0_POST_WAIT3_NTT = 0x%08X", tr[SLOT_AIV0_POST_WAIT3_NTT]);
    INFO_LOG("  AIV0_NTT_DOT_DONE   = 0x%08X", tr[SLOT_AIV0_NTT_DOT_DONE]);
    INFO_LOG("  AIV0_POST_WAIT3_INTT= 0x%08X", tr[SLOT_AIV0_POST_WAIT3_INTT]);
    INFO_LOG("  AIV0_EXTRACT_DONE   = 0x%08X", tr[SLOT_AIV0_EXTRACT_DONE]);

    const bool prep = (tr[SLOT_PREP_DONE] == MAGIC_PREP_DONE);
    const bool ntt = (tr[SLOT_AIV0_POST_WAIT3_NTT] == MAGIC_AIV0_POST_WAIT3_NTT) &&
                     (tr[SLOT_AIV0_NTT_DOT_DONE] == MAGIC_AIV0_NTT_DOT_DONE);
    const bool intt = (tr[SLOT_AIV0_POST_WAIT3_INTT] == MAGIC_AIV0_POST_WAIT3_INTT) &&
                      (tr[SLOT_AIV0_EXTRACT_DONE] == MAGIC_AIV0_EXTRACT_DONE);
    if (prep) {
        INFO_LOG("causal: prep wrote s_hat/u/v");
    }
    if (ntt) {
        INFO_LOG("causal: NTT+su_dot wrote u_hat/w_hat");
    }
    if (intt) {
        INFO_LOG("causal: INTT+extract wrote m");
    }
    if (prep && ntt && intt) {
        INFO_LOG("phases: PREP->NTT->INTT all linked");
    }
}

/**
 * Host 预装 ζ/γ/mat；清零结果区；不写最终 m/ŝ/u/v。
 */
static bool LoadWorkspaceHost(uint8_t *ws)
{
    using namespace tiling;
    size_t got = 0;
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
    std::memset(ws + OFF_S_HAT, 0, kSHatBytes);
    std::memset(ws + OFF_U, 0, kUBytes);
    std::memset(ws + OFF_V, 0, kVBytes);
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes);
    std::memset(ws + OFF_W_HAT, 0, kWHatBytes);
    std::memset(ws + OFF_W, 0, kWBytes);
    std::memset(ws + OFF_M, 0, kMBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

static bool DumpOutputs(uint8_t *out, uint8_t *mOut, uint8_t *ws)
{
    using namespace tiling;
    if (!WriteFile("./output/out.bin", out, kOutBytes)) {
        return false;
    }
    if (!WriteFile("./output/m.bin", mOut, kMBytes)) {
        return false;
    }
    if (!WriteFile("./output/trace.bin", ws + OFF_TRACE, kTraceBytes)) {
        return false;
    }
    if (!WriteFile("./output/s_hat.bin", ws + OFF_S_HAT, kSHatBytes)) {
        return false;
    }
    if (!WriteFile("./output/u.bin", ws + OFF_U, kUBytes)) {
        return false;
    }
    if (!WriteFile("./output/v.bin", ws + OFF_V, kVBytes)) {
        return false;
    }
    if (!WriteFile("./output/u_hat.bin", ws + OFF_U_HAT, kUHatBytes)) {
        return false;
    }
    if (!WriteFile("./output/w_hat.bin", ws + OFF_W_HAT, kWHatBytes)) {
        return false;
    }
    if (!WriteFile("./output/w.bin", ws + OFF_W, kWBytes)) {
        return false;
    }
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

    uint8_t *dkIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkBytes > 1024 ? kDkBytes : 1024));
    uint8_t *cIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutBytes > 1024 ? kOutBytes : 1024));
    uint8_t *mOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kMBytes > 1024 ? kMBytes : 1024));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wssize > 1024 ? wssize : 1024));
    std::memset(dkIn, 0, kDkBytes);
    std::memset(cIn, 0, kCBytes);
    std::memset(out, 0, kOutBytes);
    std::memset(mOut, 0, kMBytes);
    std::memset(ws, 0, wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk_pke.bin", got, dkIn, kDkBytes);
    if (!ok || got != kDkBytes) {
        ERROR_LOG("read dk_pke failed");
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cIn, kCBytes);
    if (!ok || got != kCBytes) {
        ERROR_LOG("read c failed");
        return 3;
    }
    if (!LoadWorkspaceHost(ws)) {
        ERROR_LOG("LoadWorkspaceHost failed");
        return 4;
    }

    INFO_LOG("Host: Launch1 prep_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(prep_custom, blockDim, dkIn, cIn, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    tr[SLOT_HOST_MID1] = MAGIC_HOST_MID1;
    INFO_LOG("Host: mid-sync1 → Launch2 ntt_custom (CPU MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(ntt_custom, blockDim, out, ws, *tiling);

    tr[SLOT_HOST_MID2] = MAGIC_HOST_MID2;
    INFO_LOG("Host: mid-sync2 → Launch3 intt_custom (CPU MIX)");
    ICPU_RUN_KF(intt_custom, blockDim, out, mOut, ws, *tiling);

    tr[SLOT_HOST_POST] = MAGIC_HOST_POST;
    PrintTrace(tr);

    if (!DumpOutputs(out, mOut, ws)) {
        return 14;
    }
    AscendC::GmFree(dkIn);
    AscendC::GmFree(cIn);
    AscendC::GmFree(out);
    AscendC::GmFree(mOut);
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

    uint8_t *dkHost = nullptr, *cHost = nullptr, *outHost = nullptr, *mHost = nullptr,
            *wsHost = nullptr;
    uint8_t *dkDev = nullptr, *cDev = nullptr, *outDev = nullptr, *mDev = nullptr, *wsDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), kDkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), kDkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), kMBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), kMBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(dkHost, 0, kDkBytes);
    std::memset(cHost, 0, kCBytes);
    std::memset(outHost, 0, kOutBytes);
    std::memset(mHost, 0, kMBytes);
    std::memset(wsHost, 0, wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk_pke.bin", got, dkHost, kDkBytes);
    if (!ok || got != kDkBytes) {
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cHost, kCBytes);
    if (!ok || got != kCBytes) {
        return 3;
    }
    if (!LoadWorkspaceHost(wsHost)) {
        return 4;
    }

    CHECK_ACL(aclrtMemcpy(dkDev, kDkBytes, dkHost, kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, kCBytes, cHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, kMBytes, mHost, kMBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wssize, wsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch1 prep_custom");
    ACLRT_LAUNCH_KERNEL(prep_custom)(blockDim, stream, dkDev, cDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    auto *tr = reinterpret_cast<uint32_t *>(wsHost + OFF_TRACE);
    tr[SLOT_HOST_MID1] = MAGIC_HOST_MID1;
    CHECK_ACL(aclrtMemcpy(wsDev + OFF_TRACE, kTraceBytes, wsHost + OFF_TRACE, kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch2 ntt_custom");
    ACLRT_LAUNCH_KERNEL(ntt_custom)(blockDim, stream, outDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    tr[SLOT_HOST_MID2] = MAGIC_HOST_MID2;
    CHECK_ACL(aclrtMemcpy(wsDev + OFF_TRACE, kTraceBytes, wsHost + OFF_TRACE, kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch3 intt_custom");
    ACLRT_LAUNCH_KERNEL(intt_custom)(blockDim, stream, outDev, mDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned after Launch3");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(mHost, kMBytes, mDev, kMBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wssize, wsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    tr[SLOT_HOST_POST] = MAGIC_HOST_POST;
    PrintTrace(tr);

    if (!DumpOutputs(outHost, mHost, wsHost)) {
        return 14;
    }

    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    INFO_LOG("RB-T25 decrypt device host done");
    return 0;
}
