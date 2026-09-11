/**
 * @file main.cpp
 * @brief RB-D09 Host：Alg.15 Decrypt 单 launch（prep+NTT+INTT 融合）。
 *
 * 数据流：
 *   1. 装载 dk_pke[1536]、c[1568]、zetas/gammas/mat_a/mat_b → workspace
 *   2. 单次 ACLRT_LAUNCH_KERNEL(d09_decrypt_fused_custom)
 *   3. Sync；D2H m[32]、TRACE、out magic
 *
 * 硬锁：blockDim=1；flag∈{1,3}；永禁 SoftSync / flag 5·7。
 * 禁抄 alg15/decrypt/encrypt/encaps/frozen。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_d09_decrypt_fused_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void d09_decrypt_fused_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR dkIn, GM_ADDR cIn,
                                         GM_ADDR ws, D09TilingData tiling);
#endif

namespace {

constexpr size_t kMinAlloc = 1024;
constexpr size_t kTilingSize = 64;

/**
 * 装填统一 workspace：ζ/γ/mat；清零 ŝ/u/v/û/ŵ/w/m；标 HOST_PRE。
 * @return false 读盘失败
 */
bool LoadWorkspace(uint8_t *ws)
{
    using namespace d09;
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
    std::memset(ws + OFF_MAT_C_NTT, 0, kMatCBytes);
    std::memset(ws + OFF_MAT_C_INTT, 0, kMatCBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/** 打印 Decrypt TRACE 关键槽（prep / NTT / INTT）。 */
void PrintTrace(const uint32_t *tr)
{
    using namespace d09;
    INFO_LOG("D09 TRACE: PREP=0x%08X WAIT3_NTT=0x%08X NTT_DOT=0x%08X WAIT3_INTT=0x%08X EXTRACT=0x%08X",
             tr[SLOT_PREP_DONE], tr[SLOT_AIV0_POST_WAIT3_NTT], tr[SLOT_AIV0_NTT_DOT_DONE],
             tr[SLOT_AIV0_POST_WAIT3_INTT], tr[SLOT_AIV0_EXTRACT_DONE]);
}

} // namespace

/**
 * 主流程：读 dk_pke+c → 1 launch → 落盘 m + TRACE + out magic。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    static_assert(sizeof(D09TilingData) <= 64, "");
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kTilingSize));
    std::memset(tilingHost, 0, kTilingSize);
    auto *tiling = reinterpret_cast<D09TilingData *>(tilingHost);

    const size_t dkBytes = d09::kDkBytes > kMinAlloc ? d09::kDkBytes : kMinAlloc;
    const size_t cBytes = d09::kCBytes > kMinAlloc ? d09::kCBytes : kMinAlloc;
    const size_t outBytes = d09::kOutBytes > kMinAlloc ? d09::kOutBytes : kMinAlloc;
    const size_t mBytes = d09::kMBytes > kMinAlloc ? d09::kMBytes : kMinAlloc;
    const size_t wsBytes = d09::wssize > kMinAlloc ? d09::wssize : kMinAlloc;

    uint8_t *dk = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(dkBytes));
    uint8_t *cIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(cBytes));
    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(outBytes));
    uint8_t *mOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(mBytes));
    uint8_t *ws = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(wsBytes));
    std::memset(dk, 0, d09::kDkBytes);
    std::memset(cIn, 0, d09::kCBytes);
    std::memset(out, 0, d09::kOutBytes);
    std::memset(mOut, 0, d09::kMBytes);
    std::memset(ws, 0, d09::wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk_pke.bin", got, dk, d09::kDkBytes);
    if (!ok || got != d09::kDkBytes) {
        ERROR_LOG("read dk_pke.bin failed got=%zu", got);
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cIn, d09::kCBytes);
    if (!ok || got != d09::kCBytes) {
        ERROR_LOG("read c.bin failed");
        return 3;
    }
    if (!LoadWorkspace(ws)) {
        ERROR_LOG("LoadWorkspace failed");
        return 4;
    }

    INFO_LOG("Host: L1 d09_decrypt_fused (prep+NTT+INTT) MIX");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(d09_decrypt_fused_custom, blockDim, out, mOut, dk, cIn, ws, *tiling);

    auto *tr = reinterpret_cast<uint32_t *>(ws + d09::OFF_TRACE);
    tr[d09::SLOT_HOST_POST] = d09::MAGIC_HOST_POST;
    PrintTrace(tr);

    if (!WriteFile("./output/out.bin", out, d09::kOutBytes) ||
        !WriteFile("./output/m.bin", mOut, d09::kMBytes) ||
        !WriteFile("./output/trace.bin", ws + d09::OFF_TRACE, d09::kTraceBytes) ||
        !WriteFile("./output/mat_c_ntt.bin", ws + d09::OFF_MAT_C_NTT, d09::kMatCBytes) ||
        !WriteFile("./output/mat_c_intt.bin", ws + d09::OFF_MAT_C_INTT, d09::kMatCBytes)) {
        return 14;
    }

    AscendC::GmFree(dk);
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

    D09TilingData *tiling = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tiling), kTilingSize));
    std::memset(tiling, 0, kTilingSize);

    uint8_t *dkHost = nullptr, *cHost = nullptr, *outHost = nullptr, *mHost = nullptr,
            *wsHost = nullptr;
    uint8_t *dkDev = nullptr, *cDev = nullptr, *outDev = nullptr, *mDev = nullptr, *wsDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), d09::kDkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), d09::kDkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), d09::kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), d09::kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), d09::kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), d09::kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), d09::kMBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), d09::kMBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), d09::wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), d09::wssize, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(dkHost, 0, d09::kDkBytes);
    std::memset(cHost, 0, d09::kCBytes);
    std::memset(outHost, 0, d09::kOutBytes);
    std::memset(mHost, 0, d09::kMBytes);
    std::memset(wsHost, 0, d09::wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk_pke.bin", got, dkHost, d09::kDkBytes);
    if (!ok || got != d09::kDkBytes) {
        ERROR_LOG("read dk_pke.bin failed got=%zu", got);
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cHost, d09::kCBytes);
    if (!ok || got != d09::kCBytes) {
        ERROR_LOG("read c.bin failed");
        return 3;
    }
    if (!LoadWorkspace(wsHost)) {
        ERROR_LOG("LoadWorkspace failed");
        return 4;
    }

    CHECK_ACL(aclrtMemcpy(dkDev, d09::kDkBytes, dkHost, d09::kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, d09::kCBytes, cHost, d09::kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(outDev, d09::kOutBytes, outHost, d09::kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, d09::kMBytes, mHost, d09::kMBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, d09::wssize, wsHost, d09::wssize, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: single launch d09_decrypt_fused_custom");
    ACLRT_LAUNCH_KERNEL(d09_decrypt_fused_custom)
    (blockDim, stream, outDev, mDev, dkDev, cDev, wsDev, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, d09::kOutBytes, outDev, d09::kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(mHost, d09::kMBytes, mDev, d09::kMBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, d09::wssize, wsDev, d09::wssize, ACL_MEMCPY_DEVICE_TO_HOST));

    auto *tr = reinterpret_cast<uint32_t *>(wsHost + d09::OFF_TRACE);
    tr[d09::SLOT_HOST_POST] = d09::MAGIC_HOST_POST;
    PrintTrace(tr);

    if (!WriteFile("./output/out.bin", outHost, d09::kOutBytes) ||
        !WriteFile("./output/m.bin", mHost, d09::kMBytes) ||
        !WriteFile("./output/trace.bin", wsHost + d09::OFF_TRACE, d09::kTraceBytes) ||
        !WriteFile("./output/mat_c_ntt.bin", wsHost + d09::OFF_MAT_C_NTT, d09::kMatCBytes) ||
        !WriteFile("./output/mat_c_intt.bin", wsHost + d09::OFF_MAT_C_INTT, d09::kMatCBytes)) {
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

    INFO_LOG("RB-D09 decrypt 1-launch host done");
    return 0;
}
