/**
 * @file main.cpp
 * @brief RB-T26 Host：Alg.21 Decaps 多 launch 编排。
 *
 * 数据流：
 *   dk‖c → Launch1–3 Decrypt（T25 契约）→ m'
 *        → Launch4–5 Encaps 重加密（T22/T23 契约）→ c' / K'
 *        → Host：c'≡c ? K' : J(z‖c) → K[32]
 *
 * 硬锁：blockDim=1；Decrypt flag 1/3；Reenc flag 1/3+4；永禁 5/7 / SoftSync。
 * 禁抄 alg21/decaps/encrypt/encaps/l18_l19/frozen。
 */
#include "decrypt/data_utils.h"
#include "decrypt/tiling.h"
#include "host_j_shake.hpp"
#include "reenc/tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_dec_intt_custom.h"
#include "aclrtlaunch_dec_ntt_custom.h"
#include "aclrtlaunch_dec_prep_custom.h"
#include "aclrtlaunch_enc_compute_custom.h"
#include "aclrtlaunch_enc_prep_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void dec_prep_custom(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws, DecTilingData tiling);
extern "C" void dec_ntt_custom(GM_ADDR out, GM_ADDR ws, DecTilingData tiling);
extern "C" void dec_intt_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws, DecTilingData tiling);
extern "C" void enc_prep_custom(GM_ADDR ekIn, GM_ADDR ws, EncTilingData tiling);
extern "C" void enc_compute_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ws, EncTilingData tiling);
#endif

/** ML-KEM-1024 dk_kem = dk_pke(1536) ‖ ek(1568) ‖ h(32) ‖ z(32)。 */
constexpr size_t kDkKemBytes = 3168;
constexpr size_t kDkPkeBytes = dec_tiling::kDkBytes;
constexpr size_t kEkBytes = enc_tiling::kEkBytes;
constexpr size_t kHashBytes = 32;
constexpr size_t kZBytes = 32;
constexpr size_t kCBytes = dec_tiling::kCBytes;
constexpr size_t kKBytes = 32;
constexpr uint32_t kMagicOutT26 = 0x543F001Au;

/** Host 侧 J：SHAKE256(z‖c, 32) via tiny_sha3。 */
static bool HostShake256J(uint8_t *kOut, const uint8_t *z, const uint8_t *c)
{
    return host_j::Shake256J(kOut, z, c, kCBytes);
}

/** 装填 Decrypt workspace：ζ/γ/mat；清零中间量与 TRACE。 */
static bool LoadDecWorkspace(uint8_t *ws)
{
    using namespace dec_tiling;
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

/**
 * 装填 Reenc workspace：m'→OFF_M + ζ/γ/mat；清零 coins/K/h/μ 等（禁预喂）。
 */
static bool LoadEncWorkspace(uint8_t *ws, const uint8_t *mPrime)
{
    using namespace enc_tiling;
    size_t got = 0;
    std::memset(ws + OFF_COINS, 0, kCoinsBytes);
    std::memset(ws + OFF_K, 0, kSharedKeyBytes);
    std::memset(ws + OFF_H, 0, enc_tiling::kHashBytes);
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
                kUHatBytes + kVHatBytes + kUBytes + kVBytes + enc_tiling::kCBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

static void PrintDecTrace(const uint32_t *tr)
{
    using namespace dec_tiling;
    INFO_LOG("DEC TRACE: PREP=0x%08X WAIT3_NTT=0x%08X NTT_DOT=0x%08X WAIT3_INTT=0x%08X EXTRACT=0x%08X",
             tr[SLOT_PREP_DONE], tr[SLOT_AIV0_POST_WAIT3_NTT], tr[SLOT_AIV0_NTT_DOT_DONE],
             tr[SLOT_AIV0_POST_WAIT3_INTT], tr[SLOT_AIV0_EXTRACT_DONE]);
}

static void PrintEncTrace(const uint32_t *tr)
{
    using namespace enc_tiling;
    INFO_LOG("ENC TRACE: PREP=0x%08X G=0x%08X WAIT3_NTT=0x%08X GATE=0x%08X WAIT3_INTT=0x%08X PACK=0x%08X",
             tr[SLOT_PREP_DONE], tr[SLOT_AIV0_G_DONE], tr[SLOT_AIV0_POST_WAIT3_NTT],
             tr[SLOT_AIV0_PRE_SET4], tr[SLOT_AIV0_POST_WAIT3_INTT], tr[SLOT_AIV0_PACK_DONE]);
}

/**
 * 选出共享密钥：c'与 c 逐字节相等则取 K'，否则 J(z‖c)。
 * 非常量时间（本刀正确性优先）。
 */
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
    static_assert(sizeof(DecTilingData) <= 64, "");
    static_assert(sizeof(EncTilingData) <= 64, "");
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *decTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    uint8_t *encTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(tilingSize));
    std::memset(decTilingHost, 0, tilingSize);
    std::memset(encTilingHost, 0, tilingSize);
    auto *decTiling = reinterpret_cast<DecTilingData *>(decTilingHost);
    auto *encTiling = reinterpret_cast<EncTilingData *>(encTilingHost);

    uint8_t *dkKem = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkKemBytes > 1024 ? kDkKemBytes : 1024));
    uint8_t *cIn = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *out = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(64 > 1024 ? 64 : 1024));
    uint8_t *mPrime = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(32 > 1024 ? 32 : 1024));
    uint8_t *cPrime = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > 1024 ? kCBytes : 1024));
    uint8_t *kOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(32 > 1024 ? 32 : 1024));
    uint8_t *decWs =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(dec_tiling::wssize > 1024 ? dec_tiling::wssize : 1024));
    uint8_t *encWs =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(enc_tiling::wssize > 1024 ? enc_tiling::wssize : 1024));
    std::memset(dkKem, 0, kDkKemBytes);
    std::memset(cIn, 0, kCBytes);
    std::memset(out, 0, 64);
    std::memset(mPrime, 0, 32);
    std::memset(cPrime, 0, kCBytes);
    std::memset(kOut, 0, 32);
    std::memset(decWs, 0, dec_tiling::wssize);
    std::memset(encWs, 0, enc_tiling::wssize);

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
    (void)(dkKem + kDkPkeBytes + kEkBytes); // h 位于 dk；Encaps prep 自算 H(ek)

    if (!LoadDecWorkspace(decWs)) {
        ERROR_LOG("LoadDecWorkspace failed");
        return 4;
    }

    INFO_LOG("Host: L1 dec_prep (AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(dec_prep_custom, blockDim, const_cast<uint8_t *>(dkPke), cIn, decWs, *decTiling);

    auto *decTr = reinterpret_cast<uint32_t *>(decWs + dec_tiling::OFF_TRACE);
    decTr[dec_tiling::SLOT_HOST_MID1] = dec_tiling::MAGIC_HOST_MID1;
    INFO_LOG("Host: mid1 → L2 dec_ntt (MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(dec_ntt_custom, blockDim, out, decWs, *decTiling);

    decTr[dec_tiling::SLOT_HOST_MID2] = dec_tiling::MAGIC_HOST_MID2;
    INFO_LOG("Host: mid2 → L3 dec_intt (MIX)");
    ICPU_RUN_KF(dec_intt_custom, blockDim, out, mPrime, decWs, *decTiling);
    decTr[dec_tiling::SLOT_HOST_POST] = dec_tiling::MAGIC_HOST_POST;
    PrintDecTrace(decTr);

    if (!LoadEncWorkspace(encWs, mPrime)) {
        ERROR_LOG("LoadEncWorkspace failed");
        return 5;
    }

    INFO_LOG("Host: L4 enc_prep G(m'||H(ek)) (AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_prep_custom, blockDim, const_cast<uint8_t *>(ek), encWs, *encTiling);

    auto *encTr = reinterpret_cast<uint32_t *>(encWs + enc_tiling::OFF_TRACE);
    encTr[enc_tiling::SLOT_HOST_MID_SYNC] = enc_tiling::MAGIC_HOST_MID;
    INFO_LOG("Host: mid3 → L5 enc_compute Encrypt (MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(enc_compute_custom, blockDim, out, cPrime, encWs, *encTiling);
    encTr[enc_tiling::SLOT_HOST_POST_SYNC] = enc_tiling::MAGIC_HOST_POST;
    PrintEncTrace(encTr);

    const uint8_t *kPrime = encWs + enc_tiling::OFF_K;
    if (!SelectK(kOut, kPrime, cPrime, cIn, z)) {
        ERROR_LOG("SelectK / Host J failed");
        return 6;
    }

    // 写 T26 魔数到 out
    *reinterpret_cast<uint32_t *>(out) = kMagicOutT26;

    if (!WriteFile("./output/out.bin", out, 64) || !WriteFile("./output/K.bin", kOut, kKBytes) ||
        !WriteFile("./output/m_prime.bin", mPrime, 32) ||
        !WriteFile("./output/c_prime.bin", cPrime, kCBytes) ||
        !WriteFile("./output/K_prime.bin", kPrime, kKBytes) ||
        !WriteFile("./output/trace_dec.bin", decWs + dec_tiling::OFF_TRACE, dec_tiling::kTraceBytes) ||
        !WriteFile("./output/trace_enc.bin", encWs + enc_tiling::OFF_TRACE, enc_tiling::kTraceBytes)) {
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

    DecTilingData *decTiling = nullptr;
    EncTilingData *encTiling = nullptr;
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
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&decWsHost), dec_tiling::wssize));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&decWsDev), dec_tiling::wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&encWsHost), enc_tiling::wssize));
    CHECK_ACL(
        aclrtMalloc(reinterpret_cast<void **>(&encWsDev), enc_tiling::wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(dkHost, 0, kDkKemBytes);
    std::memset(cHost, 0, kCBytes);
    std::memset(outHost, 0, 64);
    std::memset(mHost, 0, 32);
    std::memset(cPrimeHost, 0, kCBytes);
    std::memset(kHost, 0, kKBytes);
    std::memset(decWsHost, 0, dec_tiling::wssize);
    std::memset(encWsHost, 0, enc_tiling::wssize);

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
    CHECK_ACL(aclrtMemcpy(decWsDev, dec_tiling::wssize, decWsHost, dec_tiling::wssize,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L1 dec_prep");
    ACLRT_LAUNCH_KERNEL(dec_prep_custom)(blockDim, stream, dkDev, cDev, decWsDev, decTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(decWsHost, dec_tiling::wssize, decWsDev, dec_tiling::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    auto *decTr = reinterpret_cast<uint32_t *>(decWsHost + dec_tiling::OFF_TRACE);
    decTr[dec_tiling::SLOT_HOST_MID1] = dec_tiling::MAGIC_HOST_MID1;
    CHECK_ACL(aclrtMemcpy(decWsDev + dec_tiling::OFF_TRACE, dec_tiling::kTraceBytes,
                          decWsHost + dec_tiling::OFF_TRACE, dec_tiling::kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L2 dec_ntt");
    ACLRT_LAUNCH_KERNEL(dec_ntt_custom)(blockDim, stream, outDev, decWsDev, decTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(decWsHost, dec_tiling::wssize, decWsDev, dec_tiling::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    decTr[dec_tiling::SLOT_HOST_MID2] = dec_tiling::MAGIC_HOST_MID2;
    CHECK_ACL(aclrtMemcpy(decWsDev + dec_tiling::OFF_TRACE, dec_tiling::kTraceBytes,
                          decWsHost + dec_tiling::OFF_TRACE, dec_tiling::kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L3 dec_intt");
    ACLRT_LAUNCH_KERNEL(dec_intt_custom)(blockDim, stream, outDev, mDev, decWsDev, decTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(mHost, 32, mDev, 32, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(decWsHost, dec_tiling::wssize, decWsDev, dec_tiling::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    decTr[dec_tiling::SLOT_HOST_POST] = dec_tiling::MAGIC_HOST_POST;
    PrintDecTrace(decTr);

    if (!LoadEncWorkspace(encWsHost, mHost)) {
        return 5;
    }
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, dkHost + kDkPkeBytes, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(encWsDev, enc_tiling::wssize, encWsHost, enc_tiling::wssize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cPrimeDev, kCBytes, cPrimeHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L4 enc_prep");
    ACLRT_LAUNCH_KERNEL(enc_prep_custom)(blockDim, stream, ekDev, encWsDev, encTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(encWsHost, enc_tiling::wssize, encWsDev, enc_tiling::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    auto *encTr = reinterpret_cast<uint32_t *>(encWsHost + enc_tiling::OFF_TRACE);
    encTr[enc_tiling::SLOT_HOST_MID_SYNC] = enc_tiling::MAGIC_HOST_MID;
    CHECK_ACL(aclrtMemcpy(encWsDev + enc_tiling::OFF_TRACE, enc_tiling::kTraceBytes,
                          encWsHost + enc_tiling::OFF_TRACE, enc_tiling::kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: L5 enc_compute");
    ACLRT_LAUNCH_KERNEL(enc_compute_custom)(blockDim, stream, outDev, cPrimeDev, encWsDev, encTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream returned after L5");

    CHECK_ACL(aclrtMemcpy(outHost, 64, outDev, 64, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cPrimeHost, kCBytes, cPrimeDev, kCBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(encWsHost, enc_tiling::wssize, encWsDev, enc_tiling::wssize,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    encTr[enc_tiling::SLOT_HOST_POST_SYNC] = enc_tiling::MAGIC_HOST_POST;
    PrintEncTrace(encTr);

    const uint8_t *z = dkHost + kDkPkeBytes + kEkBytes + kHashBytes;
    if (!SelectK(kHost, encWsHost + enc_tiling::OFF_K, cPrimeHost, cHost, z)) {
        return 6;
    }
    *reinterpret_cast<uint32_t *>(outHost) = kMagicOutT26;

    if (!WriteFile("./output/out.bin", outHost, 64) || !WriteFile("./output/K.bin", kHost, kKBytes) ||
        !WriteFile("./output/m_prime.bin", mHost, 32) ||
        !WriteFile("./output/c_prime.bin", cPrimeHost, kCBytes) ||
        !WriteFile("./output/K_prime.bin", encWsHost + enc_tiling::OFF_K, kKBytes) ||
        !WriteFile("./output/trace_dec.bin", decWsHost + dec_tiling::OFF_TRACE,
                   dec_tiling::kTraceBytes) ||
        !WriteFile("./output/trace_enc.bin", encWsHost + enc_tiling::OFF_TRACE,
                   enc_tiling::kTraceBytes)) {
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
    INFO_LOG("RB-T26 decaps device host done");
    return 0;
}
