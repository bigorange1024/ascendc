/**
 * @file main.cpp
 * @brief RB-K08 Host：三 launch KEM KeyGen（K07 融合 MIX + kem_tail） —— L1 prep → L2 融合 MIX(NTT+dot/encode) → ek/dk。
 *
 * 本文件在流水线中的位置：仅 Host 编排（H2D/D2H + 两次 launch + 两次 Host sync）；
 * 算法分布在两个设备核：L1（AIV-only，链自 RB-K01）、L2（MIX，本目录 `kg_ntt_dot_encode_custom`，
 * 融合了 RB-K04 的 L2a NTT 与 L2b 点积+编码）。
 *
 * 对齐 LR-KG-F1（PLAN）：
 *   L1  ACLRT_LAUNCH(kg_prep_custom)             → SynchronizeStream（mid-sync）
 *   L2  ACLRT_LAUNCH(kg_ntt_dot_encode_custom)     → SynchronizeStream（sync）
 *   D2H ek[1568] / dk_kem[3168]（经 L3 kem_tail）
 *
 * 背景：由 RB-K04（三 launch）派生；L2a+L2b 已融为单 MIX（见 kg_ntt_dot_encode_custom.cpp）；
 * 仍禁融 prep∥MIX（L1 必须先于 L2 独立完成并 mid-sync）；禁抄 examples/pass-fix/frozen KeyGen。
 * 结论：单 session 两核；每次 launch 后必 Host sync；ρ/ζ/γ/mat 由 Host 预喂 L2 融合 ws；
 * ŝ̂/ê̂（L2 内部段1→段2 的桥接数据）**不再经 Host** —— 由设备侧段1 NTT 直接写到段2 期望的槛位。
 * 未采用：单 launch 长 MIX（融 prep）；SoftSync；flag 5/7。
 *
 * CPU：AIV_MODE（L1）→ MIX_MODE（L2 融合）；孪生无真流同步，以 TRACE+日志证分段。
 * SIM/NPU：同 stream 两次 ACLRT_LAUNCH + aclrtSynchronizeStream。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"
// k02_inc：L2 融合 ws 前半（NTT 段）布局 + TilingData 占位（Host/Device 统一使用此结构占位）。
#include "k02_inc/tiling.h"
// k03_inc：L2 融合 ws 后半（点积+编码段）布局；已重命名 namespace tiling_k03 避免撞名。
#include "k03_inc/tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_kg_prep_custom.h"
#include "aclrtlaunch_kg_ntt_dot_encode_custom.h"
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
extern "C" void kg_ntt_dot_encode_custom(GM_ADDR out, GM_ADDR ekOut, GM_ADDR dkOut, GM_ADDR ws,
                                         TilingData tiling);
extern "C" void kg_kem_tail_custom(GM_ADDR ekGm, GM_ADDR dkPkeGm, GM_ADDR seedDGm, GM_ADDR hGm,
                                   GM_ADDR zGm, GM_ADDR dkKemGm);
#endif

namespace {
/** L1 缓冲尺寸（与 K01 一致）。 */
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kK = 4;
constexpr size_t kPolyN = 256;
constexpr size_t kAHatPolys = kK * kK;
constexpr size_t kSrcRows = 2 * kK;
constexpr size_t kSeedBytes = 32; // seed_d.bin 垫块（前4B=SEED_D LE）；与 K06/kem_tail 一致
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

/** L2 融合 ws 布局：[ 段1(k02) tiling::wssize | 段2(k03) tiling_k03::wssize ]。 */
constexpr size_t kWsSeg1 = tiling::wssize;      // 20536
constexpr size_t kWsSeg2 = tiling_k03::wssize;  // 35960
constexpr size_t kWsFused = kWsSeg1 + kWsSeg2;
constexpr size_t kOutBytes = 64;
constexpr size_t kEkBytes = tiling_k03::kEkBytes; // 1568
constexpr size_t kDkBytes = tiling_k03::kDkBytes; // 1536
constexpr size_t kHashBytes = 32;
constexpr size_t kDkKemBytes = 3168; // dk_pke(1536)+ek(1568)+H(32)+z(32)
constexpr size_t kSeedPadBytes = 32; // kem_tail 读 seed 垫块

/**
 * 打印段1 TRACE（K02 槽语义），证 NTT 握手与设备写 ŝ̂/ê̂。
 */
void PrintTraceSeg1(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("Seg1 TRACE (NTT):");
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
        INFO_LOG("Seg1 causal: GATE SET4 seen");
    }
    if (ntt) {
        INFO_LOG("Seg1 causal: NTT SET1+WAIT3 seen");
    }
}

/**
 * 打印段2 TRACE（K03 槛语义）。
 */
void PrintTraceSeg2(const uint32_t *tr)
{
    using namespace tiling_k03;
    INFO_LOG("Seg2 TRACE (dot+encode):");
    INFO_LOG("  HOST_PRE=0x%08X AIV0_SET4=0x%08X AIV0_SET1=0x%08X AIV0_WAIT3=0x%08X DONE=0x%08X HOST_POST=0x%08X",
             tr[SLOT_HOST_PRE], tr[SLOT_AIV0_PRE_SET4], tr[SLOT_AIV0_PRE_SET1],
             tr[SLOT_AIV0_POST_WAIT3], tr[SLOT_AIV0_DONE], tr[SLOT_HOST_POST_SYNC]);
    const bool gate =
        (tr[SLOT_AIV0_PRE_SET4] == MAGIC_AIV0_PRE_SET4) || (tr[SLOT_AIV1_PRE_SET4] == MAGIC_AIV1_PRE_SET4);
    const bool dot =
        ((tr[SLOT_AIV0_PRE_SET1] == MAGIC_AIV0_PRE_SET1) || (tr[SLOT_AIV1_PRE_SET1] == MAGIC_AIV1_PRE_SET1)) &&
        ((tr[SLOT_AIV0_POST_WAIT3] == MAGIC_AIV0_POST_WAIT3) ||
         (tr[SLOT_AIV1_POST_WAIT3] == MAGIC_AIV1_POST_WAIT3));
    if (gate) {
        INFO_LOG("Seg2 causal: GATE SET4 seen");
    }
    if (dot) {
        INFO_LOG("Seg2 causal: DOT SET1+WAIT3 seen");
    }
    if (dot && tr[SLOT_AIV0_DONE] == MAGIC_AIV0_DONE) {
        INFO_LOG("Seg2 causal: device wrote ek/dk (K03 DONE)");
    }
}

/**
 * 装填融合 ws 的段1 子块：ŝ/ê（来自 L1）+ ζ/mat；清零 NTT 输出区；标 HOST_PRE。
 * @return false 读盘失败
 */
bool LoadWsSeg1(uint8_t *ws1, const uint8_t *sHost, const uint8_t *eHost)
{
    using namespace tiling;
    size_t got = 0;
    std::memcpy(ws1 + OFF_S, sHost, kSBytes);
    std::memcpy(ws1 + OFF_E, eHost, kEBytes);
    if (!ReadFile("./input/zetas.bin", got, ws1 + OFF_ZETAS, kZetasBytes) || got != kZetasBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws1 + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws1 + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    std::memset(ws1 + OFF_S_NTT, 0, kSNttBytes);
    std::memset(ws1 + OFF_E_NTT, 0, kENttBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws1 + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 装填融合 ws 的段2 子块：Â（来自 L1）+ γ/ρ/mat；清零 t̂/ek/dk **与 S_NTT/E_NTT**
 * （ŝ̂/ê̂ 不再由 Host 预喂 —— 段1 device NTT 会把结果直写到本子块的 S_NTT/E_NTT，
 * 这正是本刀“Host 仅 2 launch”的关键：省掉了 RB-K04 里 Host 在 L2a/L2b 之间的中转拷贝）。
 */
bool LoadWsSeg2(uint8_t *ws2, const uint8_t *aHatHost)
{
    using namespace tiling_k03;
    size_t got = 0;
    std::memcpy(ws2 + OFF_A_HAT, aHatHost, kAHatBytes);
    std::memset(ws2 + OFF_S_NTT, 0, kSNttBytes);
    std::memset(ws2 + OFF_E_NTT, 0, kENttBytes);
    if (!ReadFile("./input/gammas.bin", got, ws2 + OFF_GAMMAS, kGammasBytes) || got != kGammasBytes) {
        return false;
    }
    if (!ReadFile("./input/rho.bin", got, ws2 + OFF_RHO, kRhoBytes) || got != kRhoBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws2 + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws2 + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    std::memset(ws2 + OFF_T_HAT, 0, kTHatBytes);
    std::memset(ws2 + OFF_EK, 0, kEkBytes);
    std::memset(ws2 + OFF_DK, 0, kDkBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws2 + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}
}  // namespace

/**
 * 主流程：读 seed_d → 二 launch → 落盘 ek/dk + 两段 TRACE + 诊断中间量。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    static_assert(sizeof(TilingData) <= 64, "");
    uint32_t blockDim = kBlockDim;
    bool ok;

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kBlockDim;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *mixTilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kMixTilingSize));
    std::memset(mixTilingHost, 0, kMixTilingSize);
    TilingData *mixTiling = reinterpret_cast<TilingData *>(mixTilingHost);

    uint8_t *seedBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes > kMinAlloc ? kSeedBytes : kMinAlloc));
    uint8_t *aHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *sHatBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSHatBytes));
    uint8_t *eBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kEBytes));
    uint8_t *prfBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *xBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kXBytes > kMinAlloc ? kXBytes : kMinAlloc));
    uint8_t *lenBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kLenBytes > kMinAlloc ? kLenBytes : kMinAlloc));
    uint8_t *shakeTilingBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kShakeTilingBytes > kMinAlloc ? kShakeTilingBytes : kMinAlloc));

    uint8_t *outBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutBytes > kMinAlloc ? kOutBytes : kMinAlloc));
    uint8_t *ekBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kEkBytes > kMinAlloc ? kEkBytes : kMinAlloc));
    uint8_t *dkBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkBytes > kMinAlloc ? kDkBytes : kMinAlloc));
    uint8_t *wsFused = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsFused));

    std::memset(aHatBuf, 0, kAHatBytes);
    std::memset(sHatBuf, 0, kSHatBytes);
    std::memset(eBuf, 0, kEBytes);
    std::memset(outBuf, 0, kOutBytes);
    std::memset(ekBuf, 0, kEkBytes);
    std::memset(dkBuf, 0, kDkBytes);
    std::memset(wsFused, 0, kWsFused);

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedBuf, kSeedBytes) || rs != kSeedBytes) {
        ERROR_LOG("read seed_d.bin failed");
        return 1;
    }
    std::memcpy(shakeTilingBuf, &shakeTiling, kShakeTilingBytes);

    // ---------- L1：AIV prep ----------
    INFO_LOG("Host: Launch1 kg_prep_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(kg_prep_custom, blockDim, seedBuf, aHatBuf, sHatBuf, eBuf, prfBuf, srcBuf, xBuf, lenBuf,
                shakeTilingBuf);
    INFO_LOG("Host: mid-sync after Launch1 (CPU sequential barrier)");

    if (!LoadWsSeg1(wsFused, sHatBuf, eBuf)) {
        ERROR_LOG("LoadWsSeg1 failed");
        return 3;
    }
    if (!LoadWsSeg2(wsFused + kWsSeg1, aHatBuf)) {
        ERROR_LOG("LoadWsSeg2 failed");
        return 4;
    }

    // ---------- L2：融合 MIX（段1 NTT + 段2 点积/编码，单 launch）----------
    INFO_LOG("Host: Launch2 kg_ntt_dot_encode_custom (CPU MIX twin, fused)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(kg_ntt_dot_encode_custom, blockDim, outBuf, ekBuf, dkBuf, wsFused, *mixTiling);
    {
        auto *tr1 = reinterpret_cast<uint32_t *>(wsFused + tiling::OFF_TRACE);
        tr1[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceSeg1(tr1);
        auto *tr2 = reinterpret_cast<uint32_t *>(wsFused + kWsSeg1 + tiling_k03::OFF_TRACE);
        tr2[tiling_k03::SLOT_HOST_POST_SYNC] = tiling_k03::MAGIC_HOST_POST_SYNC;
        PrintTraceSeg2(tr2);
    }
    INFO_LOG("Host: sync after Launch2; D2H ek/dk");

    ok = WriteFile("./output/ek_pke.bin", ekBuf, kEkBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/dk_pke.bin", dkBuf, kDkBytes);
    if (!ok) {
        return 16;
    }
    // L3 kem_tail CPU twin
    uint8_t *hBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc));
    uint8_t *zBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc));
    uint8_t *dkKemBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkKemBytes));
    uint8_t *seedPadBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSeedPadBytes > kMinAlloc ? kSeedPadBytes : kMinAlloc));
    std::memset(hBuf, 0, kHashBytes);
    std::memset(zBuf, 0, kHashBytes);
    std::memset(dkKemBuf, 0, kDkKemBytes);
    std::memset(seedPadBuf, 0, kSeedPadBytes);
    std::memcpy(seedPadBuf, seedBuf, kSeedBytes < kSeedPadBytes ? kSeedBytes : kSeedPadBytes);
    INFO_LOG("Host: Launch3 kg_kem_tail_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(kg_kem_tail_custom, blockDim, ekBuf, dkBuf, seedPadBuf, hBuf, zBuf, dkKemBuf);
    ok = WriteFile("./output/ek.bin", ekBuf, kEkBytes);
    if (!ok) { return 22; }
    ok = WriteFile("./output/dk_kem.bin", dkKemBuf, kDkKemBytes);
    if (!ok) { return 23; }
    AscendC::GmFree(hBuf);
    AscendC::GmFree(zBuf);
    AscendC::GmFree(dkKemBuf);
    AscendC::GmFree(seedPadBuf);
    ok = WriteFile("./output/out.bin", outBuf, kOutBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/trace_seg1.bin", wsFused + tiling::OFF_TRACE, tiling::kTraceBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace_seg2.bin", wsFused + kWsSeg1 + tiling_k03::OFF_TRACE,
                   tiling_k03::kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c_seg1.bin", wsFused + tiling::OFF_MAT_C, tiling::kMatCBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_seg2.bin", wsFused + kWsSeg1 + tiling_k03::OFF_MAT_C,
                   tiling_k03::kMatCBytes);
    if (!ok) {
        return 21;
    }
    // 诊断中间量（非生产验收）
    (void)WriteFile("./output/a_hat.bin", aHatBuf, kAHatBytes);
    (void)WriteFile("./output/s_hat.bin", sHatBuf, kSHatBytes);
    (void)WriteFile("./output/e.bin", eBuf, kEBytes);
    (void)WriteFile("./output/s_ntt.bin", wsFused + kWsSeg1 + tiling_k03::OFF_S_NTT, tiling_k03::kSNttBytes);
    (void)WriteFile("./output/e_ntt.bin", wsFused + kWsSeg1 + tiling_k03::OFF_E_NTT, tiling_k03::kENttBytes);
    (void)WriteFile("./output/t_hat.bin", wsFused + kWsSeg1 + tiling_k03::OFF_T_HAT, tiling_k03::kTHatBytes);

    AscendC::GmFree(seedBuf);
    AscendC::GmFree(aHatBuf);
    AscendC::GmFree(sHatBuf);
    AscendC::GmFree(eBuf);
    AscendC::GmFree(prfBuf);
    AscendC::GmFree(srcBuf);
    AscendC::GmFree(xBuf);
    AscendC::GmFree(lenBuf);
    AscendC::GmFree(shakeTilingBuf);
    AscendC::GmFree(outBuf);
    AscendC::GmFree(ekBuf);
    AscendC::GmFree(dkBuf);
    AscendC::GmFree(wsFused);
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
    uint8_t *outHost = nullptr, *ekHost = nullptr, *dkHost = nullptr, *wsFusedHost = nullptr;
    uint8_t *outDev = nullptr, *ekDev = nullptr, *dkDev = nullptr, *wsFusedDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
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
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kOutBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), kDkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), kDkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsFusedHost), kWsFused));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsFusedDev), kWsFused, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(aHatHost, 0, kAHatBytes);
    std::memset(sHatHost, 0, kSHatBytes);
    std::memset(eHost, 0, kEBytes);
    std::memset(outHost, 0, kOutBytes);
    std::memset(ekHost, 0, kEkBytes);
    std::memset(dkHost, 0, kDkBytes);
    std::memset(wsFusedHost, 0, kWsFused);

    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, seedHost, kSeedBytes) || rs != kSeedBytes) {
        return 1;
    }
    std::memcpy(shakeTilingHost, &shakeTiling, kShakeTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
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

    if (!LoadWsSeg1(wsFusedHost, sHatHost, eHost)) {
        ERROR_LOG("LoadWsSeg1 failed");
        return 3;
    }
    if (!LoadWsSeg2(wsFusedHost + kWsSeg1, aHatHost)) {
        ERROR_LOG("LoadWsSeg2 failed");
        return 4;
    }
    CHECK_ACL(aclrtMemcpy(outDev, kOutBytes, outHost, kOutBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, kDkBytes, dkHost, kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsFusedDev, kWsFused, wsFusedHost, kWsFused, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L2：融合 MIX（段1 NTT + 段2 点积/编码，单 launch）----------
    INFO_LOG("Host: Launch2 kg_ntt_dot_encode_custom (fused MIX)");
    ACLRT_LAUNCH_KERNEL(kg_ntt_dot_encode_custom)
    (blockDim, stream, outDev, ekDev, dkDev, wsFusedDev, mixTiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch2 (fused)");

    CHECK_ACL(aclrtMemcpy(outHost, kOutBytes, outDev, kOutBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(ekHost, kEkBytes, ekDev, kEkBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkHost, kDkBytes, dkDev, kDkBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsFusedHost, kWsFused, wsFusedDev, kWsFused, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr1 = reinterpret_cast<uint32_t *>(wsFusedHost + tiling::OFF_TRACE);
        tr1[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceSeg1(tr1);
        auto *tr2 = reinterpret_cast<uint32_t *>(wsFusedHost + kWsSeg1 + tiling_k03::OFF_TRACE);
        tr2[tiling_k03::SLOT_HOST_POST_SYNC] = tiling_k03::MAGIC_HOST_POST_SYNC;
        PrintTraceSeg2(tr2);
    }

    ok = WriteFile("./output/ek_pke.bin", ekHost, kEkBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/dk_pke.bin", dkHost, kDkBytes);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/out.bin", outHost, kOutBytes);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/trace_seg1.bin", wsFusedHost + tiling::OFF_TRACE, tiling::kTraceBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace_seg2.bin", wsFusedHost + kWsSeg1 + tiling_k03::OFF_TRACE,
                   tiling_k03::kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c_seg1.bin", wsFusedHost + tiling::OFF_MAT_C, tiling::kMatCBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_seg2.bin", wsFusedHost + kWsSeg1 + tiling_k03::OFF_MAT_C,
                   tiling_k03::kMatCBytes);
    if (!ok) {
        return 21;
    }

    // ---------- L3：kem_tail（AIV）— ek/dk_pke → dk_kem ----------
    uint8_t *hHost = nullptr, *zHost = nullptr, *dkKemHost = nullptr;
    uint8_t *hDev = nullptr, *zDev = nullptr, *dkKemDev = nullptr;
    uint8_t *seedPadHost = nullptr, *seedPadDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&hHost), kHashBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&hDev), kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&zHost), kHashBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&zDev), kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkKemHost), kDkKemBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkKemDev), kDkKemBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedPadHost), kSeedPadBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedPadDev), kSeedPadBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(hHost, 0, kHashBytes);
    std::memset(zHost, 0, kHashBytes);
    std::memset(dkKemHost, 0, kDkKemBytes);
    std::memset(seedPadHost, 0, kSeedPadBytes);
    std::memcpy(seedPadHost, seedHost, kSeedBytes < kSeedPadBytes ? kSeedBytes : kSeedPadBytes);
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, kDkBytes, dkHost, kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(seedPadDev, kSeedPadBytes, seedPadHost, kSeedPadBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHashBytes, hHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHashBytes, zHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkKemDev, kDkKemBytes, dkKemHost, kDkKemBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    INFO_LOG("Host: Launch3 kg_kem_tail_custom");
    ACLRT_LAUNCH_KERNEL(kg_kem_tail_custom)
    (blockDim, stream, ekDev, dkDev, seedPadDev, hDev, zDev, dkKemDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch3 (kem_tail)");
    CHECK_ACL(aclrtMemcpy(hHost, kHashBytes, hDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(zHost, kHashBytes, zDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkKemHost, kDkKemBytes, dkKemDev, kDkKemBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/ek.bin", ekHost, kEkBytes);
    if (!ok) { return 22; }
    ok = WriteFile("./output/dk_kem.bin", dkKemHost, kDkKemBytes);
    if (!ok) { return 23; }
    ok = WriteFile("./output/h.bin", hHost, kHashBytes);
    if (!ok) { return 24; }
    ok = WriteFile("./output/z.bin", zHost, kHashBytes);
    if (!ok) { return 25; }
    CHECK_ACL(aclrtFree(hDev)); CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFree(zDev)); CHECK_ACL(aclrtFreeHost(zHost));
    CHECK_ACL(aclrtFree(dkKemDev)); CHECK_ACL(aclrtFreeHost(dkKemHost));
    CHECK_ACL(aclrtFree(seedPadDev)); CHECK_ACL(aclrtFreeHost(seedPadHost));

    (void)WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes);
    (void)WriteFile("./output/s_hat.bin", sHatHost, kSHatBytes);
    (void)WriteFile("./output/e.bin", eHost, kEBytes);
    (void)WriteFile("./output/s_ntt.bin", wsFusedHost + kWsSeg1 + tiling_k03::OFF_S_NTT,
                   tiling_k03::kSNttBytes);
    (void)WriteFile("./output/e_ntt.bin", wsFusedHost + kWsSeg1 + tiling_k03::OFF_E_NTT,
                   tiling_k03::kENttBytes);
    (void)WriteFile("./output/t_hat.bin", wsFusedHost + kWsSeg1 + tiling_k03::OFF_T_HAT,
                   tiling_k03::kTHatBytes);

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
    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFree(wsFusedDev));
    CHECK_ACL(aclrtFreeHost(wsFusedHost));
    CHECK_ACL(aclrtFreeHost(mixTiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
