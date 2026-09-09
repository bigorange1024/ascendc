/**
 * @file main.cpp
 * @brief RB-K06 Host：四 launch KEM KeyGen — L1 prep → L2a NTT → L2b dot+encode → L3 kem_tail。
 *
 * 本文件在流水线中的位置：仅 Host 编排（H2D/D2H + 四次 launch）；算法在四个独立设备核。
 * 对齐 KGR-K02 / KB §B2：
 *   L1  ACLRT_LAUNCH(kg_prep_custom)         → SynchronizeStream
 *   L2a ACLRT_LAUNCH(kg_ntt_custom)          → SynchronizeStream
 *   L2b ACLRT_LAUNCH(kg_dot_encode_custom)   → SynchronizeStream
 *   L3  ACLRT_LAUNCH(kg_kem_tail_custom)     → SynchronizeStream
 *   D2H ek[1568] / dk_kem[3168]
 *
 * 背景：禁融单核；禁抄 examples/pass-fix/frozen KeyGen；可链接 K01–K05 契约。
 * 结论：单 session 四核；每次 launch 后必 Host mid-sync；ρ/ζ/γ/mat 由 Host 预喂 L2 ws；
 *       L3 AIV-only，禁与 L2b 深 CrossCore 融合。
 * 未采用：单 launch 长 MIX；SoftSync；flag 5/7。
 *
 * CPU：依次 AIV → MIX → MIX → AIV（孪生无真流同步，以 TRACE+日志证分段）。
 * SIM/NPU：同 stream 四次 ACLRT_LAUNCH + aclrtSynchronizeStream。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"
// 仅 include k02 tiling（k03 同名会撞；L2b 偏移用字面量对齐 k03_inc/tiling.h）
#include "k02_inc/tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_kg_prep_custom.h"
#include "aclrtlaunch_kg_ntt_custom.h"
#include "aclrtlaunch_kg_dot_encode_custom.h"
#include "aclrtlaunch_kg_kem_tail_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void kg_prep_custom(GM_ADDR seedDGm, GM_ADDR aHatGm, GM_ADDR sHatGm, GM_ADDR eGm,
                               GM_ADDR prfWsGm, GM_ADDR srcWsGm, GM_ADDR xWsGm, GM_ADDR lenWsGm,
                               GM_ADDR tilingGm);
extern "C" void kg_ntt_custom(GM_ADDR out, GM_ADDR sNttOut, GM_ADDR eNttOut, GM_ADDR ws,
                              TilingData tiling);
extern "C" void kg_dot_encode_custom(GM_ADDR out, GM_ADDR ekOut, GM_ADDR dkOut, GM_ADDR ws,
                                     TilingData tiling);
extern "C" void kg_kem_tail_custom(GM_ADDR ekGm, GM_ADDR dkPkeGm, GM_ADDR seedDGm, GM_ADDR hGm,
                                   GM_ADDR zGm, GM_ADDR dkKemGm);
#endif

namespace {
/** L1 缓冲尺寸（与 K01 一致）；seed 用 32B 垫块供 L3 同缓冲读取。 */
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kAHatPolys = kK * kK;
constexpr size_t kSrcRows = 2 * kK;
/** seed_d.bin：前 4B = uint32 LE SEED_D；后垫至 32B（对齐 K05 L3 DataCopy） */
constexpr size_t kSeedPad = 32;
constexpr size_t kAHatBytes = kAHatPolys * kPolyN * sizeof(int32_t);
constexpr size_t kSHatBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kEBytes = kK * kPolyN * sizeof(int32_t);
constexpr size_t kPrfBytes = kSrcRows * 128U;
constexpr size_t kSrcBytes = kSrcRows * kPolyN * sizeof(int32_t);
constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr size_t kMinAlloc = 1024;
constexpr uint32_t kPrfBatch = 8U;
constexpr uint32_t kPrfMaxMsgLen = 64U;
constexpr uint32_t kPrfOutLen = 128U;
constexpr size_t kXBytes = static_cast<size_t>(kPrfBatch) * kPrfMaxMsgLen;
constexpr size_t kLenBytes = static_cast<size_t>(kPrfBatch) * sizeof(uint32_t);
constexpr size_t kMixTilingSize = 64;
constexpr size_t kHashBytes = 32;
constexpr size_t kDkKemBytes = 3168;

/**
 * 打印 L2a TRACE（K02 槽语义），证 mid-sync 后 NTT 握手。
 */
void PrintTraceL2a(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("L2a TRACE (NTT):");
    INFO_LOG("  HOST_PRE=0x%08X AIV0_SET4=0x%08X AIV0_SET1=0x%08X AIV0_WAIT3=0x%08X DONE=0x%08X HOST_POST=0x%08X",
             tr[SLOT_HOST_PRE], tr[SLOT_AIV0_PRE_SET4], tr[SLOT_AIV0_PRE_SET1],
             tr[SLOT_AIV0_POST_WAIT3], tr[SLOT_AIV0_DONE], tr[SLOT_HOST_POST_SYNC]);
    const bool gate =
        (tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4) || (tr[SLOT_AIV1_PRE_SET4] == MAGIC_AIV1_PRE_SET4);
    const bool ntt =
        ((tr[SLOT_AIV0_PRE_SET1] == MAGIC_AIV0_PRE_SET1) || (tr[SLOT_AIV1_PRE_SET1] == MAGIC_AIV1_PRE_SET1)) &&
        ((tr[SLOT_AIV0_POST_WAIT3] == MAGIC_AIV0_POST_WAIT3) ||
         (tr[SLOT_AIV1_POST_WAIT3] == MAGIC_AIV1_POST_WAIT3));
    if (gate) {
        INFO_LOG("L2a causal: GATE SET4 seen");
    }
    if (ntt) {
        INFO_LOG("L2a causal: NTT SET1+WAIT3 seen");
    }
}

/**
 * 打印 L2b TRACE（K03 槽语义）；字面魔数避免与 k02 符号冲突。
 */
void PrintTraceL2b(const uint32_t *tr)
{
    INFO_LOG("L2b TRACE (dot+encode):");
    INFO_LOG("  HOST_PRE=0x%08X AIV0_SET4=0x%08X AIV0_SET1=0x%08X AIV0_WAIT3=0x%08X DONE=0x%08X HOST_POST=0x%08X",
             tr[0], tr[1], tr[3], tr[8], tr[10], tr[11]);
    const bool gate = (tr[1] == 0xA1040004u) || (tr[2] == 0xA1140004u);
    const bool dot = ((tr[3] == 0xA1010001u) || (tr[4] == 0xA1110001u)) &&
                     ((tr[8] == 0xA1030003u) || (tr[9] == 0xA1130003u));
    if (gate) {
        INFO_LOG("L2b causal: GATE SET4 seen");
    }
    if (dot) {
        INFO_LOG("L2b causal: DOT SET1+WAIT3 seen");
    }
    if (dot && tr[10] == 0x4B30334Fu) {
        INFO_LOG("L2b causal: device wrote ek/dk (K03 DONE)");
    }
}

/**
 * 装填 L2a workspace：ŝ/ê（来自 L1）+ ζ/mat；清零 NTT 区；标 HOST_PRE。
 * @return false 读盘失败
 */
bool LoadWsL2a(uint8_t *ws, const uint8_t *sHost, const uint8_t *eHost)
{
    using namespace tiling;
    size_t got = 0;
    std::memcpy(ws + OFF_S, sHost, kSBytes);
    std::memcpy(ws + OFF_E, eHost, kEBytes);
    if (!ReadFile("./input/zetas.bin", got, ws + OFF_ZETAS, kZetasBytes) || got != kZetasBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    std::memset(ws + OFF_S_NTT, 0, kSNttBytes);
    std::memset(ws + OFF_E_NTT, 0, kENttBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 装填 L2b workspace：Â（L1）+ ŝ̂/ê̂（L2a）+ γ/ρ/mat；清零 ek/dk/t̂。
 * 偏移字面量对齐 k03_inc/tiling.h（见 LAYOUT）。
 */
bool LoadWsL2b(uint8_t *ws, const uint8_t *aHatHost, const uint8_t *sNttHost, const uint8_t *eNttHost)
{
    constexpr size_t OFF_A_HAT = 0;
    constexpr size_t kAHat = 16384;
    constexpr size_t OFF_S_NTT = OFF_A_HAT + kAHat;
    constexpr size_t kSNtt = 4096;
    constexpr size_t OFF_E_NTT = OFF_S_NTT + kSNtt;
    constexpr size_t kENtt = 4096;
    constexpr size_t OFF_GAMMAS = OFF_E_NTT + kENtt;
    constexpr size_t kGammas = 512;
    constexpr size_t OFF_RHO = OFF_GAMMAS + kGammas;
    constexpr size_t kRho = 32;
    constexpr size_t OFF_T_HAT = OFF_RHO + kRho;
    constexpr size_t kTHat = 4096;
    constexpr size_t OFF_EK = OFF_T_HAT + kTHat;
    constexpr size_t kEk = 1568;
    constexpr size_t OFF_DK = OFF_EK + kEk;
    constexpr size_t kDk = 1536;
    constexpr size_t OFF_MAT_A = OFF_DK + kDk;
    constexpr size_t kMatA = 16 * 32;
    constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatA;
    constexpr size_t kMatB = 32 * 32;
    constexpr size_t OFF_MAT_C = OFF_MAT_B + kMatB;
    constexpr size_t kMatC = 16 * 32 * sizeof(int32_t);
    constexpr size_t OFF_TRACE = OFF_MAT_C + kMatC;
    constexpr size_t kTrace = 14 * sizeof(uint32_t);

    size_t got = 0;
    std::memcpy(ws + OFF_A_HAT, aHatHost, kAHat);
    std::memcpy(ws + OFF_S_NTT, sNttHost, kSNtt);
    std::memcpy(ws + OFF_E_NTT, eNttHost, kENtt);
    if (!ReadFile("./input/gammas.bin", got, ws + OFF_GAMMAS, kGammas) || got != kGammas) {
        return false;
    }
    if (!ReadFile("./input/rho.bin", got, ws + OFF_RHO, kRho) || got != kRho) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatA)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatB)) {
        return false;
    }
    (void)kMatC;
    std::memset(ws + OFF_T_HAT, 0, kTHat);
    std::memset(ws + OFF_EK, 0, kEk);
    std::memset(ws + OFF_DK, 0, kDk);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTrace);
    tr[0] = 0x484F5354u; // MAGIC_HOST_PRE
    return true;
}
}  // namespace

/**
 * 主流程：读 seed_d → 四 launch → 落盘 ek/dk_kem + TRACE + 诊断中间量。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    static_assert(sizeof(TilingData) <= 64, "");
    uint32_t blockDim = kBlockDim;
    bool ok;

    constexpr size_t kWsL2a = tiling::wssize; // 20536
    constexpr size_t kWsL2b = 35960;          // k03_inc/tiling.h::wssize
    constexpr size_t kOutL2a = tiling::kOutBytes;
    constexpr size_t kSNttBytes = tiling::kSNttBytes;
    constexpr size_t kENttBytes = tiling::kENttBytes;
    constexpr size_t kOutL2b = 64;
    constexpr size_t kEkBytes = 1568;
    constexpr size_t kDkPkeBytes = 1536;
    constexpr size_t kTraceBytes = tiling::kTraceBytes;
    constexpr size_t kMatCBytes = tiling::kMatCBytes;
    constexpr size_t kK03OffTHat = 25120;
    constexpr size_t kK03OffMatC = 33856;
    constexpr size_t kK03OffTrace = 35904;
    constexpr size_t kTHatBytes = 4096;

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kBlockDim;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *mixTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kMixTilingSize));
    std::memset(mixTilingHost, 0, kMixTilingSize);
    TilingData *mixTiling = reinterpret_cast<TilingData *>(mixTilingHost);

    uint8_t *seedBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSeedPad > kMinAlloc ? kSeedPad : kMinAlloc));
    uint8_t *aHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *sHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSHatBytes));
    uint8_t *eBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kEBytes));
    uint8_t *prfBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *xBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kXBytes > kMinAlloc ? kXBytes : kMinAlloc));
    uint8_t *lenBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kLenBytes > kMinAlloc ? kLenBytes : kMinAlloc));
    uint8_t *shakeTilingBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kShakeTilingBytes > kMinAlloc ? kShakeTilingBytes : kMinAlloc));

    uint8_t *outL2a = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutL2a > kMinAlloc ? kOutL2a : kMinAlloc));
    uint8_t *sNttBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSNttBytes > kMinAlloc ? kSNttBytes : kMinAlloc));
    uint8_t *eNttBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kENttBytes > kMinAlloc ? kENttBytes : kMinAlloc));
    uint8_t *wsL2a = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsL2a > kMinAlloc ? kWsL2a : kMinAlloc));

    uint8_t *outL2b = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutL2b > kMinAlloc ? kOutL2b : kMinAlloc));
    uint8_t *ekBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kEkBytes > kMinAlloc ? kEkBytes : kMinAlloc));
    uint8_t *dkPkeBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkPkeBytes > kMinAlloc ? kDkPkeBytes : kMinAlloc));
    uint8_t *wsL2b = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsL2b > kMinAlloc ? kWsL2b : kMinAlloc));

    uint8_t *hBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc));
    uint8_t *zBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc));
    uint8_t *dkKemBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkKemBytes > kMinAlloc ? kDkKemBytes : kMinAlloc));

    std::memset(seedBuf, 0, kSeedPad);
    std::memset(aHatBuf, 0, kAHatBytes);
    std::memset(sHatBuf, 0, kSHatBytes);
    std::memset(eBuf, 0, kEBytes);
    std::memset(outL2a, 0, kOutL2a);
    std::memset(sNttBuf, 0, kSNttBytes);
    std::memset(eNttBuf, 0, kENttBytes);
    std::memset(wsL2a, 0, kWsL2a);
    std::memset(outL2b, 0, kOutL2b);
    std::memset(ekBuf, 0, kEkBytes);
    std::memset(dkPkeBuf, 0, kDkPkeBytes);
    std::memset(wsL2b, 0, kWsL2b);
    std::memset(hBuf, 0, kHashBytes);
    std::memset(zBuf, 0, kHashBytes);
    std::memset(dkKemBuf, 0, kDkKemBytes);

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedBuf, kSeedPad) || rs != kSeedPad) {
        ERROR_LOG("read seed_d.bin failed (want %zuB pad)", kSeedPad);
        return 1;
    }
    std::memcpy(shakeTilingBuf, &shakeTiling, kShakeTilingBytes);

    // ---------- L1：AIV prep ----------
    INFO_LOG("Host: Launch1 kg_prep_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(kg_prep_custom, blockDim, seedBuf, aHatBuf, sHatBuf, eBuf, prfBuf, srcBuf, xBuf, lenBuf,
                shakeTilingBuf);
    INFO_LOG("Host: mid-sync after Launch1 (CPU sequential barrier)");

    if (!LoadWsL2a(wsL2a, sHatBuf, eBuf)) {
        ERROR_LOG("LoadWsL2a failed");
        return 3;
    }

    // ---------- L2a：MIX NTT ----------
    INFO_LOG("Host: Launch2a kg_ntt_custom (CPU MIX twin)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(kg_ntt_custom, blockDim, outL2a, sNttBuf, eNttBuf, wsL2a, *mixTiling);
    {
        auto *tr = reinterpret_cast<uint32_t *>(wsL2a + tiling::OFF_TRACE);
        tr[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceL2a(tr);
    }
    INFO_LOG("Host: sync after Launch2a (CPU sequential barrier)");

    if (!LoadWsL2b(wsL2b, aHatBuf, sNttBuf, eNttBuf)) {
        ERROR_LOG("LoadWsL2b failed");
        return 4;
    }

    // ---------- L2b：MIX dot+encode ----------
    INFO_LOG("Host: Launch2b kg_dot_encode_custom (CPU MIX twin)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(kg_dot_encode_custom, blockDim, outL2b, ekBuf, dkPkeBuf, wsL2b, *mixTiling);
    {
        auto *tr = reinterpret_cast<uint32_t *>(wsL2b + kK03OffTrace);
        tr[11] = 0x484F5355u; // MAGIC_HOST_POST_SYNC
        PrintTraceL2b(tr);
    }
    INFO_LOG("Host: sync after Launch2b (CPU sequential barrier)");

    // ---------- L3：AIV kem_tail（禁与 L2b CrossCore 融合）----------
    // Host 不预喂 H/z/dk_kem：输出已清零，由设备 UB+DataCopy 写出。
    INFO_LOG("Host: Launch3 kg_kem_tail_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(kg_kem_tail_custom, blockDim, ekBuf, dkPkeBuf, seedBuf, hBuf, zBuf, dkKemBuf);
    INFO_LOG("Host: sync after Launch3; D2H ek/dk_kem");

    ok = WriteFile("./output/ek.bin", ekBuf, kEkBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/dk_kem.bin", dkKemBuf, kDkKemBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/h.bin", hBuf, kHashBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/z.bin", zBuf, kHashBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/dk_pke.bin", dkPkeBuf, kDkPkeBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/out_l2a.bin", outL2a, kOutL2a);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/out_l2b.bin", outL2b, kOutL2b);
    if (!ok) {
        return 21;
    }
    ok = WriteFile("./output/trace_l2a.bin", wsL2a + tiling::OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 22;
    }
    ok = WriteFile("./output/trace_l2b.bin", wsL2b + kK03OffTrace, kTraceBytes);
    if (!ok) {
        return 23;
    }
    ok = WriteFile("./output/mat_c_l2a.bin", wsL2a + tiling::OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 24;
    }
    ok = WriteFile("./output/mat_c_l2b.bin", wsL2b + kK03OffMatC, kMatCBytes);
    if (!ok) {
        return 25;
    }
    (void)WriteFile("./output/a_hat.bin", aHatBuf, kAHatBytes);
    (void)WriteFile("./output/s_hat.bin", sHatBuf, kSHatBytes);
    (void)WriteFile("./output/e.bin", eBuf, kEBytes);
    (void)WriteFile("./output/s_ntt.bin", sNttBuf, kSNttBytes);
    (void)WriteFile("./output/e_ntt.bin", eNttBuf, kENttBytes);
    (void)WriteFile("./output/t_hat.bin", wsL2b + kK03OffTHat, kTHatBytes);

    AscendC::GmFree(seedBuf);
    AscendC::GmFree(aHatBuf);
    AscendC::GmFree(sHatBuf);
    AscendC::GmFree(eBuf);
    AscendC::GmFree(prfBuf);
    AscendC::GmFree(srcBuf);
    AscendC::GmFree(xBuf);
    AscendC::GmFree(lenBuf);
    AscendC::GmFree(shakeTilingBuf);
    AscendC::GmFree(outL2a);
    AscendC::GmFree(sNttBuf);
    AscendC::GmFree(eNttBuf);
    AscendC::GmFree(wsL2a);
    AscendC::GmFree(outL2b);
    AscendC::GmFree(ekBuf);
    AscendC::GmFree(dkPkeBuf);
    AscendC::GmFree(wsL2b);
    AscendC::GmFree(hBuf);
    AscendC::GmFree(zBuf);
    AscendC::GmFree(dkKemBuf);
    AscendC::GmFree(mixTilingHost);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *mixTiling = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mixTiling), kMixTilingSize));
    std::memset(mixTiling, 0, kMixTilingSize);

    uint8_t *seedHost = nullptr, *aHatHost = nullptr, *sHatHost = nullptr, *eHost = nullptr;
    uint8_t *prfHost = nullptr, *srcHost = nullptr, *xHost = nullptr, *lenHost = nullptr;
    uint8_t *shakeTilingHost = nullptr;
    uint8_t *seedDev = nullptr, *aHatDev = nullptr, *sHatDev = nullptr, *eDev = nullptr;
    uint8_t *prfDev = nullptr, *srcDev = nullptr, *xDev = nullptr, *lenDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;
    uint8_t *outL2aHost = nullptr, *sNttHost = nullptr, *eNttHost = nullptr, *wsL2aHost = nullptr;
    uint8_t *outL2aDev = nullptr, *sNttDev = nullptr, *eNttDev = nullptr, *wsL2aDev = nullptr;
    uint8_t *outL2bHost = nullptr, *ekHost = nullptr, *dkPkeHost = nullptr, *wsL2bHost = nullptr;
    uint8_t *outL2bDev = nullptr, *ekDev = nullptr, *dkPkeDev = nullptr, *wsL2bDev = nullptr;
    uint8_t *hHost = nullptr, *zHost = nullptr, *dkKemHost = nullptr;
    uint8_t *hDev = nullptr, *zDev = nullptr, *dkKemDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedPad));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedPad, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&sHatHost), kSHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sHatDev), kSHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&eHost), kEBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&eDev), kEBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prfHost), kPrfBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&xHost), kXBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&lenHost), kLenBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&shakeTilingHost), kShakeTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&shakeTilingDev), kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2aHost), kOutL2a));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2aDev), kOutL2a, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&sNttHost), kSNttBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sNttDev), kSNttBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&eNttHost), kENttBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&eNttDev), kENttBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2aHost), kWsL2a));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2aDev), kWsL2a, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2bHost), kOutL2b));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2bDev), kOutL2b, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkPkeHost), kDkPkeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkPkeDev), kDkPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2bHost), kWsL2b));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2bDev), kWsL2b, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&hHost), kHashBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&hDev), kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&zHost), kHashBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&zDev), kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkKemHost), kDkKemBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkKemDev), kDkKemBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(seedHost, 0, kSeedPad);
    std::memset(aHatHost, 0, kAHatBytes);
    std::memset(sHatHost, 0, kSHatBytes);
    std::memset(eHost, 0, kEBytes);
    std::memset(outL2aHost, 0, kOutL2a);
    std::memset(sNttHost, 0, kSNttBytes);
    std::memset(eNttHost, 0, kENttBytes);
    std::memset(wsL2aHost, 0, kWsL2a);
    std::memset(outL2bHost, 0, kOutL2b);
    std::memset(ekHost, 0, kEkBytes);
    std::memset(dkPkeHost, 0, kDkPkeBytes);
    std::memset(wsL2bHost, 0, kWsL2b);
    std::memset(hHost, 0, kHashBytes);
    std::memset(zHost, 0, kHashBytes);
    std::memset(dkKemHost, 0, kDkKemBytes);

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedHost, kSeedPad) || rs != kSeedPad) {
        return 1;
    }
    std::memcpy(shakeTilingHost, &shakeTiling, kShakeTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedPad, seedHost, kSeedPad, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, shakeTilingHost, kShakeTilingBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(aHatDev, kAHatBytes, aHatHost, kAHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sHatDev, kSHatBytes, sHatHost, kSHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(eDev, kEBytes, eHost, kEBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L1 ----------
    INFO_LOG("Host: Launch1 kg_prep_custom");
    ACLRT_LAUNCH_KERNEL(kg_prep_custom)
    (blockDim, stream, seedDev, aHatDev, sHatDev, eDev, prfDev, srcDev, xDev, lenDev, shakeTilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch1 (mid-sync)");

    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(sHatHost, kSHatBytes, sHatDev, kSHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(eHost, kEBytes, eDev, kEBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!LoadWsL2a(wsL2aHost, sHatHost, eHost)) {
        ERROR_LOG("LoadWsL2a failed");
        return 3;
    }
    CHECK_ACL(aclrtMemcpy(outL2aDev, kOutL2a, outL2aHost, kOutL2a, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sNttDev, kSNttBytes, sNttHost, kSNttBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(eNttDev, kENttBytes, eNttHost, kENttBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsL2aDev, kWsL2a, wsL2aHost, kWsL2a, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L2a ----------
    INFO_LOG("Host: Launch2a kg_ntt_custom");
    ACLRT_LAUNCH_KERNEL(kg_ntt_custom)
    (blockDim, stream, outL2aDev, sNttDev, eNttDev, wsL2aDev, mixTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch2a");

    CHECK_ACL(aclrtMemcpy(outL2aHost, kOutL2a, outL2aDev, kOutL2a, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(sNttHost, kSNttBytes, sNttDev, kSNttBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(eNttHost, kENttBytes, eNttDev, kENttBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsL2aHost, kWsL2a, wsL2aDev, kWsL2a, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr = reinterpret_cast<uint32_t *>(wsL2aHost + tiling::OFF_TRACE);
        tr[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceL2a(tr);
    }

    if (!LoadWsL2b(wsL2bHost, aHatHost, sNttHost, eNttHost)) {
        ERROR_LOG("LoadWsL2b failed");
        return 4;
    }
    CHECK_ACL(aclrtMemcpy(outL2bDev, kOutL2b, outL2bHost, kOutL2b, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkPkeDev, kDkPkeBytes, dkPkeHost, kDkPkeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsL2bDev, kWsL2b, wsL2bHost, kWsL2b, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L2b ----------
    INFO_LOG("Host: Launch2b kg_dot_encode_custom");
    ACLRT_LAUNCH_KERNEL(kg_dot_encode_custom)
    (blockDim, stream, outL2bDev, ekDev, dkPkeDev, wsL2bDev, mixTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch2b");

    CHECK_ACL(aclrtMemcpy(outL2bHost, kOutL2b, outL2bDev, kOutL2b, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(ekHost, kEkBytes, ekDev, kEkBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkPkeHost, kDkPkeBytes, dkPkeDev, kDkPkeBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsL2bHost, kWsL2b, wsL2bDev, kWsL2b, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr = reinterpret_cast<uint32_t *>(wsL2bHost + kK03OffTrace);
        tr[11] = 0x484F5355u;
        PrintTraceL2b(tr);
    }

    // ---------- L3：重新 H2D ek/dk_pke/seed（清零 h/z/dk_kem）----------
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkPkeDev, kDkPkeBytes, dkPkeHost, kDkPkeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedPad, seedHost, kSeedPad, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHashBytes, hHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHashBytes, zHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkKemDev, kDkKemBytes, dkKemHost, kDkKemBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    INFO_LOG("Host: Launch3 kg_kem_tail_custom");
    ACLRT_LAUNCH_KERNEL(kg_kem_tail_custom)
    (blockDim, stream, ekDev, dkPkeDev, seedDev, hDev, zDev, dkKemDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch3");

    CHECK_ACL(aclrtMemcpy(hHost, kHashBytes, hDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(zHost, kHashBytes, zDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkKemHost, kDkKemBytes, dkKemDev, kDkKemBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    ok = WriteFile("./output/ek.bin", ekHost, kEkBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/dk_kem.bin", dkKemHost, kDkKemBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/h.bin", hHost, kHashBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/z.bin", zHost, kHashBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/dk_pke.bin", dkPkeHost, kDkPkeBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/out_l2a.bin", outL2aHost, kOutL2a);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/out_l2b.bin", outL2bHost, kOutL2b);
    if (!ok) {
        return 21;
    }
    ok = WriteFile("./output/trace_l2a.bin", wsL2aHost + tiling::OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 22;
    }
    ok = WriteFile("./output/trace_l2b.bin", wsL2bHost + kK03OffTrace, kTraceBytes);
    if (!ok) {
        return 23;
    }
    ok = WriteFile("./output/mat_c_l2a.bin", wsL2aHost + tiling::OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 24;
    }
    ok = WriteFile("./output/mat_c_l2b.bin", wsL2bHost + kK03OffMatC, kMatCBytes);
    if (!ok) {
        return 25;
    }
    (void)WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes);
    (void)WriteFile("./output/s_hat.bin", sHatHost, kSHatBytes);
    (void)WriteFile("./output/e.bin", eHost, kEBytes);
    (void)WriteFile("./output/s_ntt.bin", sNttHost, kSNttBytes);
    (void)WriteFile("./output/e_ntt.bin", eNttHost, kENttBytes);
    (void)WriteFile("./output/t_hat.bin", wsL2bHost + kK03OffTHat, kTHatBytes);

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFreeHost(sHatHost));
    CHECK_ACL(aclrtFree(eDev));
    CHECK_ACL(aclrtFreeHost(eHost));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFreeHost(prfHost));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFreeHost(xHost));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFreeHost(lenHost));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFreeHost(shakeTilingHost));
    CHECK_ACL(aclrtFree(outL2aDev));
    CHECK_ACL(aclrtFreeHost(outL2aHost));
    CHECK_ACL(aclrtFree(sNttDev));
    CHECK_ACL(aclrtFreeHost(sNttHost));
    CHECK_ACL(aclrtFree(eNttDev));
    CHECK_ACL(aclrtFreeHost(eNttHost));
    CHECK_ACL(aclrtFree(wsL2aDev));
    CHECK_ACL(aclrtFreeHost(wsL2aHost));
    CHECK_ACL(aclrtFree(outL2bDev));
    CHECK_ACL(aclrtFreeHost(outL2bHost));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(dkPkeDev));
    CHECK_ACL(aclrtFreeHost(dkPkeHost));
    CHECK_ACL(aclrtFree(wsL2bDev));
    CHECK_ACL(aclrtFreeHost(wsL2bHost));
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFreeHost(zHost));
    CHECK_ACL(aclrtFree(dkKemDev));
    CHECK_ACL(aclrtFreeHost(dkKemHost));
    CHECK_ACL(aclrtFreeHost(mixTiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
