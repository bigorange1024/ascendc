/**
 * @file main.cpp
 * @brief RB-D07 Host：设备 Encaps → (c,K_enc) → 设备 Decaps → K_dec；单 session 多 launch + mid-sync。
 *
 * 流水线（Alg.20 Encaps + Alg.21 Decaps 设备路径；禁两段 aclFinalize）：
 *   Encaps:  enc_g(m‖h)→(K,r) → mid-sync → enc_prep → mid-sync → enc_mix→c
 *   Decaps:  dec_prep → mid-sync → dec_ntt_dot → mid-sync → dec_intt_extract→m'
 *            → enc_g(m'‖h)→(K',r') → mid-sync → enc_prep → mid-sync → enc_mix→c'
 *            → mid-sync → decaps_fo → K_dec
 *
 * 编排来源：本战役已绿 D04-decrypt-full / D04-G / D05-reenc / D06-fo 契约重拼；
 * Encaps 侧 basename=enc_*（S0B §6）；G/Encrypt 与 Decaps G/ReEnc **共用**同核。
 * 硬锁：BLOCK_DIM=1；flag∈{1,3,4}；X12 DataCopy；Host 不预喂 K/c/coins。
 * 未采用：抄 T25–T27 / alg15|21 / examples / frozen / l18；两段 Finalize。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_enc_g_custom.h"
#include "aclrtlaunch_enc_prep_custom.h"
#include "aclrtlaunch_enc_mix_custom.h"
#include "aclrtlaunch_dec_prep_custom.h"
#include "aclrtlaunch_dec_ntt_dot_custom.h"
#include "aclrtlaunch_dec_intt_extract_custom.h"
#include "aclrtlaunch_decaps_fo_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void enc_g_custom(GM_ADDR mGm, GM_ADDR hGm, GM_ADDR kGm, GM_ADDR rGm);
extern "C" void enc_prep_custom(GM_ADDR coinsIn, GM_ADDR ekIn, GM_ADDR ws, TilingData tiling);
extern "C" void enc_mix_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ws, TilingData tiling);
extern "C" void dec_prep_custom(GM_ADDR dkPkeGm, GM_ADDR cGm, GM_ADDR sHatGm, GM_ADDR uGm, GM_ADDR vGm);
extern "C" void dec_ntt_dot_custom(GM_ADDR out, GM_ADDR uHatOut, GM_ADDR wHatOut, GM_ADDR ws,
                                   TilingData tiling);
extern "C" void dec_intt_extract_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws, TilingData tiling);
extern "C" void decaps_fo_custom(GM_ADDR cGm, GM_ADDR cPrimeGm, GM_ADDR kPrimeGm, GM_ADDR zGm,
                                 GM_ADDR kOutGm);
#endif

namespace {

constexpr size_t kHalf = 32;
constexpr size_t kCt = 1568;
constexpr size_t kDkPke = 1536;
constexpr size_t kTilingSize = 64;

/**
 * Decrypt L2a（d02_inc/tiling.h）偏移字面量。
 * 背景：main 只 #include Encaps tiling.h（同名 namespace tiling 会撞）；
 * 结论：L2a 布局用字面量，禁再 include d02 tiling。
 */
constexpr size_t kDecL2aOffU = 0;
constexpr size_t kDecL2aU = 4096;
constexpr size_t kDecL2aOffSHat = 4096;
constexpr size_t kDecL2aSHat = 4096;
constexpr size_t kDecL2aOffZetas = 8192;
constexpr size_t kDecL2aZetas = 512;
constexpr size_t kDecL2aOffGammas = 8704;
constexpr size_t kDecL2aGammas = 512;
constexpr size_t kDecL2aOffUHat = 9216;
constexpr size_t kDecL2aUHat = 4096;
constexpr size_t kDecL2aOffWHat = 13312;
constexpr size_t kDecL2aWHat = 1024;
constexpr size_t kDecL2aOffMatA = 14336;
constexpr size_t kDecL2aMatA = 512;
constexpr size_t kDecL2aOffMatB = 14848;
constexpr size_t kDecL2aMatB = 1024;
constexpr size_t kDecL2aOffMatC = 15872;
constexpr size_t kDecL2aMatC = 2048;
constexpr size_t kDecL2aOffTrace = 17920;
constexpr size_t kDecL2aTrace = 56;
constexpr size_t kDecL2aWs = 17976;
constexpr size_t kDecL2aOut = 64;

/** Decrypt L2b（d03_inc/tiling.h）字面量。 */
constexpr size_t kDecL2bOffWHat = 0;
constexpr size_t kDecL2bWHat = 1024;
constexpr size_t kDecL2bOffV = 1024;
constexpr size_t kDecL2bV = 1024;
constexpr size_t kDecL2bOffZetas = 2048;
constexpr size_t kDecL2bZetas = 512;
constexpr size_t kDecL2bOffW = 2560;
constexpr size_t kDecL2bW = 1024;
constexpr size_t kDecL2bOffMatA = 3584;
constexpr size_t kDecL2bMatA = 512;
constexpr size_t kDecL2bOffMatB = 4096;
constexpr size_t kDecL2bMatB = 1024;
constexpr size_t kDecL2bOffMatC = 5120;
constexpr size_t kDecL2bMatC = 2048;
constexpr size_t kDecL2bOffTrace = 7168;
constexpr size_t kDecL2bTrace = 56;
constexpr size_t kDecL2bWs = 7224;
constexpr size_t kDecL2bOut = 64;

constexpr size_t kDecVBytes = 256 * sizeof(int32_t); // Decrypt prep 写出的 v[256] int32

/**
 * 装填 Encaps/ReEnc 共用 workspace：原始消息→OFF_M；ζ/γ/mat；清零设备写出区。
 * @param mMsg  Encaps 用 m[32] 或 Decaps ReEnc 用 m'[32]
 */
bool LoadEncWorkspace(uint8_t *ws, const uint8_t *mMsg)
{
    using namespace tiling;
    size_t got = 0;
    std::memset(ws + OFF_Y_E1_E2, 0, kYe1e2Bytes);
    std::memset(ws + OFF_A_HAT, 0, kAHatBytes);
    std::memset(ws + OFF_Y_HAT, 0, kYHatBytes);
    std::memset(ws + OFF_T_HAT, 0, kTHatBytes);
    std::memcpy(ws + OFF_M, mMsg, kMsgBytes);
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
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes + kVHatBytes + kUBytes + kVBytes + kCBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 装填 Decrypt L2a workspace（字面量偏移）：u/ŝ/ζ/γ/mat；清零 û/ŵ。
 */
bool LoadDecL2a(uint8_t *ws, const uint8_t *uHost, const uint8_t *sHatHost)
{
    size_t got = 0;
    std::memcpy(ws + kDecL2aOffU, uHost, kDecL2aU);
    std::memcpy(ws + kDecL2aOffSHat, sHatHost, kDecL2aSHat);
    if (!ReadFile("./input/zetas.bin", got, ws + kDecL2aOffZetas, kDecL2aZetas) ||
        got != kDecL2aZetas) {
        return false;
    }
    if (!ReadFile("./input/gammas.bin", got, ws + kDecL2aOffGammas, kDecL2aGammas) ||
        got != kDecL2aGammas) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + kDecL2aOffMatA, kDecL2aMatA)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + kDecL2aOffMatB, kDecL2aMatB)) {
        return false;
    }
    std::memset(ws + kDecL2aOffUHat, 0, kDecL2aUHat);
    std::memset(ws + kDecL2aOffWHat, 0, kDecL2aWHat);
    auto *tr = reinterpret_cast<uint32_t *>(ws + kDecL2aOffTrace);
    std::memset(tr, 0, kDecL2aTrace);
    tr[0] = 0x484F5354u; // MAGIC_HOST_PRE
    return true;
}

/**
 * 装填 Decrypt L2b workspace：ŵ/v/ζ/mat；清零中间 w。
 */
bool LoadDecL2b(uint8_t *ws, const uint8_t *wHatHost, const uint8_t *vHost)
{
    size_t got = 0;
    std::memcpy(ws + kDecL2bOffWHat, wHatHost, kDecL2bWHat);
    std::memcpy(ws + kDecL2bOffV, vHost, kDecL2bV);
    if (!ReadFile("./input/zetas.bin", got, ws + kDecL2bOffZetas, kDecL2bZetas) ||
        got != kDecL2bZetas) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + kDecL2bOffMatA, kDecL2bMatA)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + kDecL2bOffMatB, kDecL2bMatB)) {
        return false;
    }
    (void)kDecL2bMatC;
    std::memset(ws + kDecL2bOffW, 0, kDecL2bW);
    auto *tr = reinterpret_cast<uint32_t *>(ws + kDecL2bOffTrace);
    std::memset(tr, 0, kDecL2bTrace);
    tr[0] = 0x484F5354u;
    return true;
}

#ifdef ASCENDC_CPU_DEBUG
constexpr size_t kMinAlloc = 2048;
/** CPU 孪生 GmAlloc 下限；NPU 路径不用，故仅 CPU 编译。 */
size_t AllocSz(size_t n)
{
    return n > kMinAlloc ? n : kMinAlloc;
}
#endif

}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    using namespace tiling;
    static_assert(sizeof(TilingData) <= 64, "");
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kTilingSize));
    std::memset(tilingHost, 0, kTilingSize);
    TilingData *tilingPtr = reinterpret_cast<TilingData *>(tilingHost);

    // ---- 公共输入 / Encaps 输出 ----
    uint8_t *mBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *hBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *ekBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kEkBytes)));
    uint8_t *dkBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDkPke)));
    uint8_t *zBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *kEncBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *rEncBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *cEncBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kCt)));
    uint8_t *encOut = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kOutBytes)));
    uint8_t *encWs = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(wssize)));

    // ---- Decrypt 中间 ----
    uint8_t *sHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aSHat)));
    uint8_t *uBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aU)));
    uint8_t *vBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecVBytes)));
    uint8_t *outL2a = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aOut)));
    uint8_t *uHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aUHat)));
    uint8_t *wHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aWHat)));
    uint8_t *wsL2a = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2aWs)));
    uint8_t *outL2b = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2bOut)));
    uint8_t *mPrimeBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *wsL2b = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kDecL2bWs)));

    // ---- Decaps G / ReEnc / FO ----
    uint8_t *kPrimeBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *rPrimeBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));
    uint8_t *cPrimeBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kCt)));
    uint8_t *kDecBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(AllocSz(kHalf)));

    std::memset(kEncBuf, 0, kHalf);
    std::memset(rEncBuf, 0, kHalf);
    std::memset(cEncBuf, 0, kCt);
    std::memset(encOut, 0, kOutBytes);
    std::memset(encWs, 0, wssize);
    std::memset(sHatBuf, 0, kDecL2aSHat);
    std::memset(uBuf, 0, kDecL2aU);
    std::memset(vBuf, 0, kDecVBytes);
    std::memset(outL2a, 0, kDecL2aOut);
    std::memset(uHatBuf, 0, kDecL2aUHat);
    std::memset(wHatBuf, 0, kDecL2aWHat);
    std::memset(wsL2a, 0, kDecL2aWs);
    std::memset(outL2b, 0, kDecL2bOut);
    std::memset(mPrimeBuf, 0, kHalf);
    std::memset(wsL2b, 0, kDecL2bWs);
    std::memset(kPrimeBuf, 0, kHalf);
    std::memset(rPrimeBuf, 0, kHalf);
    std::memset(cPrimeBuf, 0, kCt);
    std::memset(kDecBuf, 0, kHalf);

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/h.bin", rs, hBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/ek.bin", rs, ekBuf, kEkBytes) || rs != kEkBytes) {
        return 1;
    }
    if (!ReadFile("./input/dk_pke.bin", rs, dkBuf, kDkPke) || rs != kDkPke) {
        return 1;
    }
    if (!ReadFile("./input/z.bin", rs, zBuf, kHalf) || rs != kHalf) {
        return 1;
    }

    // ========== Encaps ==========
    INFO_LOG("Host: Encaps L1 enc_g_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_g_custom, blockDim, mBuf, hBuf, kEncBuf, rEncBuf);
    INFO_LOG("Host: mid-sync after Encaps G");

    if (!LoadEncWorkspace(encWs, mBuf)) {
        return 3;
    }
    INFO_LOG("Host: Encaps L2 enc_prep_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_prep_custom, blockDim, rEncBuf, ekBuf, encWs, *tilingPtr);
    {
        auto *tr = reinterpret_cast<uint32_t *>(encWs + OFF_TRACE);
        tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    }
    INFO_LOG("Host: mid-sync after Encaps prep; Encaps L3 enc_mix_custom (CPU MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(enc_mix_custom, blockDim, encOut, cEncBuf, encWs, *tilingPtr);
    INFO_LOG("Host: mid-sync after Encaps mix; c/K ready");

    // ========== Decrypt ==========
    INFO_LOG("Host: Decaps Decrypt L1 dec_prep_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(dec_prep_custom, blockDim, dkBuf, cEncBuf, sHatBuf, uBuf, vBuf);
    INFO_LOG("Host: mid-sync after Decrypt prep");

    if (!LoadDecL2a(wsL2a, uBuf, sHatBuf)) {
        return 4;
    }
    INFO_LOG("Host: Decaps Decrypt L2a dec_ntt_dot_custom (CPU MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(dec_ntt_dot_custom, blockDim, outL2a, uHatBuf, wHatBuf, wsL2a, *tilingPtr);
    INFO_LOG("Host: mid-sync after Decrypt L2a");

    if (!LoadDecL2b(wsL2b, wHatBuf, vBuf)) {
        return 5;
    }
    INFO_LOG("Host: Decaps Decrypt L2b dec_intt_extract_custom (CPU MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(dec_intt_extract_custom, blockDim, outL2b, mPrimeBuf, wsL2b, *tilingPtr);
    INFO_LOG("Host: mid-sync after Decrypt L2b; m' ready");

    // ========== Decaps G ==========
    INFO_LOG("Host: Decaps G enc_g_custom (CPU AIV; 共用核)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_g_custom, blockDim, mPrimeBuf, hBuf, kPrimeBuf, rPrimeBuf);
    INFO_LOG("Host: mid-sync after Decaps G");

    // ========== Re-Encrypt（共用 enc_prep/mix）==========
    if (!LoadEncWorkspace(encWs, mPrimeBuf)) {
        return 6;
    }
    std::memset(cPrimeBuf, 0, kCt);
    std::memset(encOut, 0, kOutBytes);
    INFO_LOG("Host: ReEnc prep enc_prep_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(enc_prep_custom, blockDim, rPrimeBuf, ekBuf, encWs, *tilingPtr);
    {
        auto *tr = reinterpret_cast<uint32_t *>(encWs + OFF_TRACE);
        tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    }
    INFO_LOG("Host: mid-sync after ReEnc prep; ReEnc mix (CPU MIX)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(enc_mix_custom, blockDim, encOut, cPrimeBuf, encWs, *tilingPtr);
    INFO_LOG("Host: mid-sync after ReEnc mix");

    // ========== FO ==========
    INFO_LOG("Host: FO decaps_fo_custom (CPU AIV)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(decaps_fo_custom, blockDim, cEncBuf, cPrimeBuf, kPrimeBuf, zBuf, kDecBuf);
    INFO_LOG("Host: mid-sync after FO; write outputs");

    ok = WriteFile("./output/k_enc.bin", kEncBuf, kHalf);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/c_enc.bin", cEncBuf, kCt);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/k_dec.bin", kDecBuf, kHalf);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/m_prime.bin", mPrimeBuf, kHalf);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/c_prime.bin", cPrimeBuf, kCt);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/k_prime.bin", kPrimeBuf, kHalf);
    if (!ok) {
        return 19;
    }

    AscendC::GmFree(mBuf);
    AscendC::GmFree(hBuf);
    AscendC::GmFree(ekBuf);
    AscendC::GmFree(dkBuf);
    AscendC::GmFree(zBuf);
    AscendC::GmFree(kEncBuf);
    AscendC::GmFree(rEncBuf);
    AscendC::GmFree(cEncBuf);
    AscendC::GmFree(encOut);
    AscendC::GmFree(encWs);
    AscendC::GmFree(sHatBuf);
    AscendC::GmFree(uBuf);
    AscendC::GmFree(vBuf);
    AscendC::GmFree(outL2a);
    AscendC::GmFree(uHatBuf);
    AscendC::GmFree(wHatBuf);
    AscendC::GmFree(wsL2a);
    AscendC::GmFree(outL2b);
    AscendC::GmFree(mPrimeBuf);
    AscendC::GmFree(wsL2b);
    AscendC::GmFree(kPrimeBuf);
    AscendC::GmFree(rPrimeBuf);
    AscendC::GmFree(cPrimeBuf);
    AscendC::GmFree(kDecBuf);
    AscendC::GmFree(tilingHost);
#else
    // ========== NPU / SIM：单 session 多 launch + 每次 SynchronizeStream ==========
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tilingPtr = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPtr), kTilingSize));
    std::memset(tilingPtr, 0, kTilingSize);

    uint8_t *mHost = nullptr, *hHost = nullptr, *ekHost = nullptr, *dkHost = nullptr, *zHost = nullptr;
    uint8_t *mDev = nullptr, *hDev = nullptr, *ekDev = nullptr, *dkDev = nullptr, *zDev = nullptr;
    uint8_t *kEncHost = nullptr, *rEncHost = nullptr, *cEncHost = nullptr, *encOutHost = nullptr,
            *encWsHost = nullptr;
    uint8_t *kEncDev = nullptr, *rEncDev = nullptr, *cEncDev = nullptr, *encOutDev = nullptr,
            *encWsDev = nullptr;
    uint8_t *sHatHost = nullptr, *uHost = nullptr, *vHost = nullptr;
    uint8_t *sHatDev = nullptr, *uDev = nullptr, *vDev = nullptr;
    uint8_t *outL2aHost = nullptr, *uHatHost = nullptr, *wHatHost = nullptr, *wsL2aHost = nullptr;
    uint8_t *outL2aDev = nullptr, *uHatDev = nullptr, *wHatDev = nullptr, *wsL2aDev = nullptr;
    uint8_t *outL2bHost = nullptr, *mPrimeHost = nullptr, *wsL2bHost = nullptr;
    uint8_t *outL2bDev = nullptr, *mPrimeDev = nullptr, *wsL2bDev = nullptr;
    uint8_t *kPrimeHost = nullptr, *rPrimeHost = nullptr, *cPrimeHost = nullptr, *kDecHost = nullptr;
    uint8_t *kPrimeDev = nullptr, *rPrimeDev = nullptr, *cPrimeDev = nullptr, *kDecDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&hHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&hDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), kDkPke));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), kDkPke, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&zHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&zDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&kEncHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kEncDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&rEncHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rEncDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cEncHost), kCt));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cEncDev), kCt, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&encOutHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&encOutDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&encWsHost), wssize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&encWsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&sHatHost), kDecL2aSHat));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sHatDev), kDecL2aSHat, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHost), kDecL2aU));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), kDecL2aU, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&vHost), kDecVBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), kDecVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2aHost), kDecL2aOut));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2aDev), kDecL2aOut, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHatHost), kDecL2aUHat));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uHatDev), kDecL2aUHat, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wHatHost), kDecL2aWHat));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wHatDev), kDecL2aWHat, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2aHost), kDecL2aWs));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2aDev), kDecL2aWs, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2bHost), kDecL2bOut));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2bDev), kDecL2bOut, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mPrimeHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mPrimeDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2bHost), kDecL2bWs));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2bDev), kDecL2bWs, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&kPrimeHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kPrimeDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&rPrimeHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rPrimeDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cPrimeHost), kCt));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cPrimeDev), kCt, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&kDecHost), kHalf));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kDecDev), kHalf, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(kEncHost, 0, kHalf);
    std::memset(rEncHost, 0, kHalf);
    std::memset(cEncHost, 0, kCt);
    std::memset(encOutHost, 0, kOutBytes);
    std::memset(encWsHost, 0, wssize);
    std::memset(sHatHost, 0, kDecL2aSHat);
    std::memset(uHost, 0, kDecL2aU);
    std::memset(vHost, 0, kDecVBytes);
    std::memset(outL2aHost, 0, kDecL2aOut);
    std::memset(uHatHost, 0, kDecL2aUHat);
    std::memset(wHatHost, 0, kDecL2aWHat);
    std::memset(wsL2aHost, 0, kDecL2aWs);
    std::memset(outL2bHost, 0, kDecL2bOut);
    std::memset(mPrimeHost, 0, kHalf);
    std::memset(wsL2bHost, 0, kDecL2bWs);
    std::memset(kPrimeHost, 0, kHalf);
    std::memset(rPrimeHost, 0, kHalf);
    std::memset(cPrimeHost, 0, kCt);
    std::memset(kDecHost, 0, kHalf);

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mHost, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/h.bin", rs, hHost, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile("./input/ek.bin", rs, ekHost, kEkBytes) || rs != kEkBytes) {
        return 1;
    }
    if (!ReadFile("./input/dk_pke.bin", rs, dkHost, kDkPke) || rs != kDkPke) {
        return 1;
    }
    if (!ReadFile("./input/z.bin", rs, zHost, kHalf) || rs != kHalf) {
        return 1;
    }

    CHECK_ACL(aclrtMemcpy(mDev, kHalf, mHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHalf, hHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, kDkPke, dkHost, kDkPke, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHalf, zHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(kEncDev, kHalf, kEncHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rEncDev, kHalf, rEncHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cEncDev, kCt, cEncHost, kCt, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(encOutDev, kOutBytes, encOutHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // Encaps G
    INFO_LOG("Host: Encaps L1 enc_g_custom");
    ACLRT_LAUNCH_KERNEL(enc_g_custom)(blockDim, stream, mDev, hDev, kEncDev, rEncDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Encaps G");

    if (!LoadEncWorkspace(encWsHost, mHost)) {
        return 3;
    }
    CHECK_ACL(aclrtMemcpy(encWsDev, wssize, encWsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Encaps L2 enc_prep_custom");
    ACLRT_LAUNCH_KERNEL(enc_prep_custom)(blockDim, stream, rEncDev, ekDev, encWsDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(encWsHost, wssize, encWsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr = reinterpret_cast<uint32_t *>(encWsHost + OFF_TRACE);
        tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    }
    CHECK_ACL(aclrtMemcpy(encWsDev + OFF_TRACE, kTraceBytes, encWsHost + OFF_TRACE, kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Encaps L3 enc_mix_custom");
    ACLRT_LAUNCH_KERNEL(enc_mix_custom)(blockDim, stream, encOutDev, cEncDev, encWsDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Encaps mix");

    CHECK_ACL(aclrtMemcpy(cEncHost, kCt, cEncDev, kCt, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(kEncHost, kHalf, kEncDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));

    // Decrypt prep — c 已在 device
    CHECK_ACL(aclrtMemcpy(sHatDev, kDecL2aSHat, sHatHost, kDecL2aSHat, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uDev, kDecL2aU, uHost, kDecL2aU, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDev, kDecVBytes, vHost, kDecVBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Decrypt L1 dec_prep_custom");
    ACLRT_LAUNCH_KERNEL(dec_prep_custom)(blockDim, stream, dkDev, cEncDev, sHatDev, uDev, vDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(sHatHost, kDecL2aSHat, sHatDev, kDecL2aSHat, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHost, kDecL2aU, uDev, kDecL2aU, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, kDecVBytes, vDev, kDecVBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!LoadDecL2a(wsL2aHost, uHost, sHatHost)) {
        return 4;
    }
    CHECK_ACL(aclrtMemcpy(outL2aDev, kDecL2aOut, outL2aHost, kDecL2aOut, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uHatDev, kDecL2aUHat, uHatHost, kDecL2aUHat, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wHatDev, kDecL2aWHat, wHatHost, kDecL2aWHat, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsL2aDev, kDecL2aWs, wsL2aHost, kDecL2aWs, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Decrypt L2a dec_ntt_dot_custom");
    ACLRT_LAUNCH_KERNEL(dec_ntt_dot_custom)
    (blockDim, stream, outL2aDev, uHatDev, wHatDev, wsL2aDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(wHatHost, kDecL2aWHat, wHatDev, kDecL2aWHat, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!LoadDecL2b(wsL2bHost, wHatHost, vHost)) {
        return 5;
    }
    CHECK_ACL(aclrtMemcpy(outL2bDev, kDecL2bOut, outL2bHost, kDecL2bOut, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mPrimeDev, kHalf, mPrimeHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsL2bDev, kDecL2bWs, wsL2bHost, kDecL2bWs, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Decrypt L2b dec_intt_extract_custom");
    ACLRT_LAUNCH_KERNEL(dec_intt_extract_custom)
    (blockDim, stream, outL2bDev, mPrimeDev, wsL2bDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(mPrimeHost, kHalf, mPrimeDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));

    // Decaps G
    CHECK_ACL(aclrtMemcpy(kPrimeDev, kHalf, kPrimeHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rPrimeDev, kHalf, rPrimeHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Decaps G enc_g_custom");
    ACLRT_LAUNCH_KERNEL(enc_g_custom)(blockDim, stream, mPrimeDev, hDev, kPrimeDev, rPrimeDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // ReEnc
    if (!LoadEncWorkspace(encWsHost, mPrimeHost)) {
        return 6;
    }
    std::memset(cPrimeHost, 0, kCt);
    std::memset(encOutHost, 0, kOutBytes);
    CHECK_ACL(aclrtMemcpy(encWsDev, wssize, encWsHost, wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cPrimeDev, kCt, cPrimeHost, kCt, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(encOutDev, kOutBytes, encOutHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: ReEnc prep enc_prep_custom");
    ACLRT_LAUNCH_KERNEL(enc_prep_custom)(blockDim, stream, rPrimeDev, ekDev, encWsDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(encWsHost, wssize, encWsDev, wssize, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr = reinterpret_cast<uint32_t *>(encWsHost + OFF_TRACE);
        tr[SLOT_HOST_MID_SYNC] = MAGIC_HOST_MID;
    }
    CHECK_ACL(aclrtMemcpy(encWsDev + OFF_TRACE, kTraceBytes, encWsHost + OFF_TRACE, kTraceBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: ReEnc mix enc_mix_custom");
    ACLRT_LAUNCH_KERNEL(enc_mix_custom)(blockDim, stream, encOutDev, cPrimeDev, encWsDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // FO
    CHECK_ACL(aclrtMemcpy(kDecDev, kHalf, kDecHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: FO decaps_fo_custom");
    ACLRT_LAUNCH_KERNEL(decaps_fo_custom)
    (blockDim, stream, cEncDev, cPrimeDev, kPrimeDev, zDev, kDecDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after FO");

    CHECK_ACL(aclrtMemcpy(kDecHost, kHalf, kDecDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cPrimeHost, kCt, cPrimeDev, kCt, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(kPrimeHost, kHalf, kPrimeDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));

    ok = WriteFile("./output/k_enc.bin", kEncHost, kHalf);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/c_enc.bin", cEncHost, kCt);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/k_dec.bin", kDecHost, kHalf);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/m_prime.bin", mPrimeHost, kHalf);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/c_prime.bin", cPrimeHost, kCt);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/k_prime.bin", kPrimeHost, kHalf);
    if (!ok) {
        return 19;
    }

    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFreeHost(zHost));
    CHECK_ACL(aclrtFree(kEncDev));
    CHECK_ACL(aclrtFreeHost(kEncHost));
    CHECK_ACL(aclrtFree(rEncDev));
    CHECK_ACL(aclrtFreeHost(rEncHost));
    CHECK_ACL(aclrtFree(cEncDev));
    CHECK_ACL(aclrtFreeHost(cEncHost));
    CHECK_ACL(aclrtFree(encOutDev));
    CHECK_ACL(aclrtFreeHost(encOutHost));
    CHECK_ACL(aclrtFree(encWsDev));
    CHECK_ACL(aclrtFreeHost(encWsHost));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFree(outL2aDev));
    CHECK_ACL(aclrtFreeHost(outL2aHost));
    CHECK_ACL(aclrtFree(uHatDev));
    CHECK_ACL(aclrtFreeHost(uHatHost));
    CHECK_ACL(aclrtFree(wHatDev));
    CHECK_ACL(aclrtFreeHost(wHatHost));
    CHECK_ACL(aclrtFree(wsL2aDev));
    CHECK_ACL(aclrtFreeHost(wsL2aHost));
    CHECK_ACL(aclrtFree(outL2bDev));
    CHECK_ACL(aclrtFreeHost(outL2bHost));
    CHECK_ACL(aclrtFree(mPrimeDev));
    CHECK_ACL(aclrtFreeHost(mPrimeHost));
    CHECK_ACL(aclrtFree(wsL2bDev));
    CHECK_ACL(aclrtFreeHost(wsL2bHost));
    CHECK_ACL(aclrtFree(kPrimeDev));
    CHECK_ACL(aclrtFreeHost(kPrimeHost));
    CHECK_ACL(aclrtFree(rPrimeDev));
    CHECK_ACL(aclrtFreeHost(rPrimeHost));
    CHECK_ACL(aclrtFree(cPrimeDev));
    CHECK_ACL(aclrtFreeHost(cPrimeHost));
    CHECK_ACL(aclrtFree(kDecDev));
    CHECK_ACL(aclrtFreeHost(kDecHost));
    CHECK_ACL(aclrtFreeHost(tilingPtr));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
