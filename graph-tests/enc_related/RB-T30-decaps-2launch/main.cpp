/**
 * @file main.cpp
 * @brief RB-T30 Host：Alg.21 Decaps 2-launch（Decrypt 融合 + Encaps 融合）+ Host FO。
 *
 * 数据流：
 *   dk‖c → L1 t30_dec_fused → m'
 *        → L2 t30_enc_fused → c' / K'
 *        → Host：c'≡c ? K' : J(z‖c) → K[32]
 *
 * 硬锁：blockDim=1；L1 flag 1/3；L2 flag 1/3+4；永禁 5/7 / SoftSync。
 * 验收：Host 日志断言 launches=2；落盘 launch_count.txt。
 */
#include "data_utils.h"
#include "host_j_shake.hpp"
#include "tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_t30_dec_fused_custom.h"
#include "aclrtlaunch_t30_enc_fused_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void t30_dec_fused_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws,
                                     T30DecTilingData tiling);
extern "C" void t30_enc_fused_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ekIn, GM_ADDR ws,
                                     T30EncTilingData tiling);
#endif

constexpr size_t kDkKemBytes = 3168;
constexpr size_t kDkPkeBytes = t30_dec::kDkBytes;
constexpr size_t kEkBytes = t30_enc::kEkBytes;
constexpr size_t kHashBytes = 32;
constexpr size_t kCBytes = t30_dec::kCBytes;
constexpr size_t kKBytes = 32;
constexpr uint32_t kMagicOutT30 = 0x54333032u;
constexpr int32_t kExpectedLaunches = 2;

static bool HostShake256J(uint8_t *kOut, const uint8_t *z, const uint8_t *c)
{
    return host_j::Shake256J(kOut, z, c, kCBytes);
}

static bool LoadDecWorkspace(uint8_t *ws)
{
    using namespace t30_dec;
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

static bool LoadEncWorkspace(uint8_t *ws, const uint8_t *mPrime)
{
    using namespace t30_enc;
    size_t got = 0;
    std::memset(ws + OFF_COINS, 0, kCoinsBytes);
    std::memset(ws + OFF_K, 0, kSharedKeyBytes);
    std::memset(ws + OFF_H, 0, t30_enc::kHashBytes);
    std::memset(ws + OFF_Y_E1_E2, 0, kYe1e2Bytes);
    std::memset(ws + OFF_A_HAT, 0, kAHatBytes);
    std::memset(ws + OFF_Y_HAT, 0, kYHatBytes);
    std::memset(ws + OFF_T_HAT, 0, kTHatBytes);
    std::memcpy(ws + OFF_M, mPrime, kMsgBytes);
    std::memset(ws + OFF_MU, 0, kMuBytes);
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
    std::memset(ws + OFF_U_HAT, 0,
                kUHatBytes + kVHatBytes + kUBytes + kVBytes + t30_enc::kCBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

static void PrintDecTrace(const uint32_t *tr)
{
    using namespace t30_dec;
    INFO_LOG("DEC TRACE: PREP=0x%08X WAIT3_NTT=0x%08X NTT_DOT=0x%08X WAIT3_INTT=0x%08X EXTRACT=0x%08X",
             tr[SLOT_PREP_DONE], tr[SLOT_AIV0_POST_WAIT3_NTT], tr[SLOT_AIV0_NTT_DOT_DONE],
             tr[SLOT_AIV0_POST_WAIT3_INTT], tr[SLOT_AIV0_EXTRACT_DONE]);
}

static void PrintEncTrace(const uint32_t *tr)
{
    using namespace t30_enc;
    INFO_LOG("ENC TRACE: PREP=0x%08X G=0x%08X WAIT3_NTT=0x%08X GATE=0x%08X WAIT3_INTT=0x%08X PACK=0x%08X",
             tr[SLOT_PREP_DONE], tr[SLOT_AIV0_G_DONE], tr[SLOT_AIV0_POST_WAIT3_NTT],
             tr[SLOT_AIV0_PRE_SET4], tr[SLOT_AIV0_POST_WAIT3_INTT], tr[SLOT_AIV0_PACK_DONE]);
}

static bool SelectK(uint8_t *kOut, const uint8_t *kPrime, const uint8_t *cPrime,
                    const uint8_t *cIn, const uint8_t *z)
{
    if (std::memcmp(cPrime, cIn, kCBytes) == 0) {
        std::memcpy(kOut, kPrime, kKBytes);
        INFO_LOG("Host select: c'==c → K=K'");
        return true;
    }
    INFO_LOG("Host select: c'!=c → K=J(z||c)");
    return HostShake256J(kOut, z, cIn);
}

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(T30DecTilingData) <= 64, "");
    static_assert(sizeof(T30EncTilingData) <= 64, "");
    uint32_t blockDim = 1;
    int32_t launchCount = 0;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *decTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    uint8_t *encTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    std::memset(decTilingHost, 0, tilingSize);
    std::memset(encTilingHost, 0, tilingSize);
    auto *decTiling = reinterpret_cast<T30DecTilingData *>(decTilingHost);
    auto *encTiling = reinterpret_cast<T30EncTilingData *>(encTilingHost);

    uint8_t *dkKem = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkKemBytes > 1024 ? kDkKemBytes : 1024));
    uint8_t *cIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(64 > 1024 ? 64 : 1024));
    uint8_t *mPrime = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(32 > 1024 ? 32 : 1024));
    uint8_t *cPrime = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *kOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(32 > 1024 ? 32 : 1024));
    uint8_t *decWs =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(t30_dec::wssize > 1024 ? t30_dec::wssize : 1024));
    uint8_t *encWs =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(t30_enc::wssize > 1024 ? t30_enc::wssize : 1024));
    std::memset(dkKem, 0, kDkKemBytes);
    std::memset(cIn, 0, kCBytes);
    std::memset(out, 0, 64);
    std::memset(mPrime, 0, 32);
    std::memset(cPrime, 0, kCBytes);
    std::memset(kOut, 0, 32);
    std::memset(decWs, 0, t30_dec::wssize);
    std::memset(encWs, 0, t30_enc::wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk.bin", got, dkKem, kDkKemBytes);
    if (!ok || got != kDkKemBytes) {
        ERROR_LOG("read dk.bin failed got=%zu", got);
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cIn, kCBytes);
    if (!ok || got != kCBytes) {
        ERROR_LOG("read c.bin failed");
        return 3;
    }

    const uint8_t *dkPke = dkKem;
    const uint8_t *ek = dkKem + kDkPkeBytes;
    const uint8_t *z = dkKem + kDkPkeBytes + kEkBytes + kHashBytes;

    if (!LoadDecWorkspace(decWs)) {
        ERROR_LOG("LoadDecWorkspace failed");
        return 4;
    }

    INFO_LOG("Host: L1 t30_dec_fused_custom (MIX Decrypt)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(t30_dec_fused_custom, blockDim, out, mPrime, const_cast<uint8_t *>(dkPke), cIn, decWs,
                *decTiling);
    launchCount += 1;
    auto *decTr = reinterpret_cast<uint32_t *>(decWs + t30_dec::OFF_TRACE);
    decTr[t30_dec::SLOT_HOST_POST] = t30_dec::MAGIC_HOST_POST;
    PrintDecTrace(decTr);

    if (!LoadEncWorkspace(encWs, mPrime)) {
        ERROR_LOG("LoadEncWorkspace failed");
        return 5;
    }

    INFO_LOG("Host: L2 t30_enc_fused_custom (MIX Encaps/Reenc)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(t30_enc_fused_custom, blockDim, out, cPrime, const_cast<uint8_t *>(ek), encWs,
                *encTiling);
    launchCount += 1;
    auto *encTr = reinterpret_cast<uint32_t *>(encWs + t30_enc::OFF_TRACE);
    encTr[t30_enc::SLOT_HOST_POST_SYNC] = t30_enc::MAGIC_HOST_POST;
    PrintEncTrace(encTr);

    const uint8_t *kPrime = encWs + t30_enc::OFF_K;
    if (!SelectK(kOut, kPrime, cPrime, cIn, z)) {
        ERROR_LOG("SelectK / Host J failed");
        return 6;
    }
    *reinterpret_cast<uint32_t *>(out) = kMagicOutT30;

    INFO_LOG("RB-T30 Host launches=%d (expect %d)", launchCount, kExpectedLaunches);
    if (launchCount != kExpectedLaunches) {
        ERROR_LOG("launch count mismatch");
        return 7;
    }

    char launchTxt[8];
    std::snprintf(launchTxt, sizeof(launchTxt), "%d\n", launchCount);
    if (!WriteFile("./output/out.bin", out, 64) || !WriteFile("./output/K.bin", kOut, kKBytes) ||
        !WriteFile("./output/m_prime.bin", mPrime, 32) ||
        !WriteFile("./output/c_prime.bin", cPrime, kCBytes) ||
        !WriteFile("./output/K_prime.bin", kPrime, kKBytes) ||
        !WriteFile("./output/trace_dec.bin", decWs + t30_dec::OFF_TRACE, t30_dec::kTraceBytes) ||
        !WriteFile("./output/trace_enc.bin", encWs + t30_enc::OFF_TRACE, t30_enc::kTraceBytes) ||
        !WriteFile("./output/launch_count.txt", launchTxt, std::strlen(launchTxt))) {
        return 14;
    }

    AscendC::GmFree(dkKem);
    AscendC::GmFree(cIn);
    AscendC::GmFree(out);
    AscendC::GmFree(mPrime);
    AscendC::GmFree(cPrime);
    AscendC::GmFree(kOut);
    AscendC::GmFree(decWs);
    AscendC::GmFree(encWs);
    AscendC::GmFree(decTilingHost);
    AscendC::GmFree(encTilingHost);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    T30DecTilingData *decTiling = nullptr;
    T30EncTilingData *encTiling = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&decTiling), tilingSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&encTiling), tilingSize));
    std::memset(decTiling, 0, tilingSize);
    std::memset(encTiling, 0, tilingSize);

    uint8_t *dkHost = nullptr, *cHost = nullptr, *outHost = nullptr, *mHost = nullptr,
            *cPrimeHost = nullptr, *kHost = nullptr, *decWsHost = nullptr, *encWsHost = nullptr;
    uint8_t *dkDev = nullptr, *cDev = nullptr, *outDev = nullptr, *mDev = nullptr, *cPrimeDev = nullptr,
            *decWsDev = nullptr, *encWsDev = nullptr, *ekDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), kDkKemBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), kDkKemBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), 64));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), 64, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), 32));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), 32, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cPrimeHost), kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cPrimeDev), kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&kHost), kKBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&decWsHost), t30_dec::wssize));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&decWsDev), t30_dec::wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&encWsHost), t30_enc::wssize));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&encWsDev), t30_enc::wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(dkHost, 0, kDkKemBytes);
    std::memset(cHost, 0, kCBytes);
    std::memset(outHost, 0, 64);
    std::memset(mHost, 0, 32);
    std::memset(cPrimeHost, 0, kCBytes);
    std::memset(kHost, 0, kKBytes);
    std::memset(decWsHost, 0, t30_dec::wssize);
    std::memset(encWsHost, 0, t30_enc::wssize);

    size_t got = 0;
    ok = ReadFile("./input/dk.bin", got, dkHost, kDkKemBytes);
    if (!ok || got != kDkKemBytes) {
        return 2;
    }
    ok = ReadFile("./input/c.bin", got, cHost, kCBytes);
    if (!ok || got != kCBytes) {
        return 3;
    }
    if (!LoadDecWorkspace(decWsHost)) {
        return 4;
    }

    CHECK_ACL(aclrtMemcpy(dkDev, kDkKemBytes, dkHost, kDkKemBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, kCBytes, cHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(outDev, 64, outHost, 64, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, 32, mHost, 32, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(decWsDev, t30_dec::wssize, decWsHost, t30_dec::wssize,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L1 t30_dec_fused_custom");
    ACLRT_LAUNCH_KERNEL(t30_dec_fused_custom)
    (blockDim, stream, outDev, mDev, dkDev, cDev, decWsDev, decTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    launchCount += 1;

    CHECK_ACL(aclrtMemcpy(mHost, 32, mDev, 32, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(decWsHost, t30_dec::wssize, decWsDev, t30_dec::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    auto *decTr = reinterpret_cast<uint32_t *>(decWsHost + t30_dec::OFF_TRACE);
    decTr[t30_dec::SLOT_HOST_POST] = t30_dec::MAGIC_HOST_POST;
    PrintDecTrace(decTr);

    if (!LoadEncWorkspace(encWsHost, mHost)) {
        return 5;
    }
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, dkHost + kDkPkeBytes, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(encWsDev, t30_enc::wssize, encWsHost, t30_enc::wssize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cPrimeDev, kCBytes, cPrimeHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L2 t30_enc_fused_custom");
    ACLRT_LAUNCH_KERNEL(t30_enc_fused_custom)
    (blockDim, stream, outDev, cPrimeDev, ekDev, encWsDev, encTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    launchCount += 1;

    CHECK_ACL(aclrtMemcpy(outHost, 64, outDev, 64, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cPrimeHost, kCBytes, cPrimeDev, kCBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(encWsHost, t30_enc::wssize, encWsDev, t30_enc::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    auto *encTr = reinterpret_cast<uint32_t *>(encWsHost + t30_enc::OFF_TRACE);
    encTr[t30_enc::SLOT_HOST_POST_SYNC] = t30_enc::MAGIC_HOST_POST;
    PrintEncTrace(encTr);

    const uint8_t *z = dkHost + kDkPkeBytes + kEkBytes + kHashBytes;
    if (!SelectK(kHost, encWsHost + t30_enc::OFF_K, cPrimeHost, cHost, z)) {
        return 6;
    }
    *reinterpret_cast<uint32_t *>(outHost) = kMagicOutT30;

    INFO_LOG("RB-T30 Host launches=%d (expect %d)", launchCount, kExpectedLaunches);
    if (launchCount != kExpectedLaunches) {
        ERROR_LOG("launch count mismatch");
        return 7;
    }

    char launchTxt[8];
    std::snprintf(launchTxt, sizeof(launchTxt), "%d\n", launchCount);
    if (!WriteFile("./output/out.bin", outHost, 64) || !WriteFile("./output/K.bin", kHost, kKBytes) ||
        !WriteFile("./output/m_prime.bin", mHost, 32) ||
        !WriteFile("./output/c_prime.bin", cPrimeHost, kCBytes) ||
        !WriteFile("./output/K_prime.bin", encWsHost + t30_enc::OFF_K, kKBytes) ||
        !WriteFile("./output/trace_dec.bin", decWsHost + t30_dec::OFF_TRACE, t30_dec::kTraceBytes) ||
        !WriteFile("./output/trace_enc.bin", encWsHost + t30_enc::OFF_TRACE, t30_enc::kTraceBytes) ||
        !WriteFile("./output/launch_count.txt", launchTxt, std::strlen(launchTxt))) {
        return 14;
    }

    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(cPrimeDev));
    CHECK_ACL(aclrtFree(decWsDev));
    CHECK_ACL(aclrtFree(encWsDev));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(cPrimeHost));
    CHECK_ACL(aclrtFreeHost(kHost));
    CHECK_ACL(aclrtFreeHost(decWsHost));
    CHECK_ACL(aclrtFreeHost(encWsHost));
    CHECK_ACL(aclrtFreeHost(decTiling));
    CHECK_ACL(aclrtFreeHost(encTiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    INFO_LOG("RB-T30 decaps 2-launch host done");
    return 0;
}
