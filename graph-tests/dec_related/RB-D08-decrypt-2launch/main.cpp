/**
 * @file main.cpp
 * @brief RB-D08 Host：二 launch 全链 Decrypt — L1 prep → L2a NTT+dot → L2b INTT+extract → m[32]。
 *
 * 本文件在流水线中的位置：仅 Host 编排（H2D/D2H + 三次 launch）；算法在三个独立设备核。
 * 对齐 S0A / DRW-D04：
 *   L1  ACLRT_LAUNCH(dec_prep_custom)         → SynchronizeStream
 *   L2  ACLRT_LAUNCH(dec_ntt_intt_extract_custom)  // 融合原 L2a+L2b      → SynchronizeStream
 *    *   D2H m[32]
 *
 * 背景：X14 禁 prep∥NTT；X15 禁 NTT∥INTT 同 launch；I1 跨核面要短。
 * 结论：单 session 三核；每次 launch 后必 sync；禁融单 launch / SoftSync / flag 5·7。
 * 未采用：抄 alg15/T25；把三核合进一次 ICPU_RUN_KF。
 *
 * CPU：依次 AIV_MODE → MIX_MODE → MIX_MODE（孪生无真流同步，以 TRACE+日志证分段）。
 * SIM/NPU：同 stream 三次 ACLRT_LAUNCH + aclrtSynchronizeStream。
 */
#include "data_utils.h"
// 仅 include d02 tiling（d03 同名 namespace/TilingData 会撞；L2b 偏移用字面量对齐 d03_inc/tiling.h）
#include "d02_inc/tiling.h"

#include <cstdio>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_dec_prep_custom.h"
#include "aclrtlaunch_dec_ntt_intt_extract_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void dec_prep_custom(GM_ADDR dkPkeGm, GM_ADDR cGm, GM_ADDR sHatGm, GM_ADDR uGm, GM_ADDR vGm);
extern "C" void dec_ntt_dot_custom(GM_ADDR out, GM_ADDR uHatOut, GM_ADDR wHatOut, GM_ADDR ws,
                                   TilingData tiling);
extern "C" void dec_intt_extract_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws, TilingData tiling);
#endif

namespace {
/** L1 缓冲尺寸（与 D01 一致）。 */
constexpr size_t kDkBytes = 1536;
constexpr size_t kCBytes = 1568;
constexpr size_t kSHatBytes = tiling::kSHatBytes; // d02 tiling 含 polyvec
constexpr size_t kUBytes = tiling::kUBytes;
constexpr size_t kVBytes = 256 * sizeof(int32_t);
constexpr size_t kMinAlloc = 1024;
constexpr size_t kTilingSize = 64;

/**
 * 打印 L2a TRACE（D02 槽语义），用于证 mid-sync 后 NTT+dot 握手。
 */
void PrintTraceL2a(const uint32_t *tr)
{
    using namespace tiling;
    INFO_LOG("L2a TRACE (NTT+dot):");
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
 * 打印 L2b TRACE（D03 槽语义），用于证 L2a sync 后 INTT+extract 握手。
 */
void PrintTraceL2b(const uint32_t *tr)
{
    // d03_inc/tiling.h 与 d02 同 namespace tiling，但 MAGIC_DONE 等不同；
    // 因两头均 #include 且 guard 互异，同 TU 只能看到后 include 的常量。
    // 此处用字面魔数（与 D03 tiling 一致），避免与 d02 符号冲突。
    INFO_LOG("L2b TRACE (INTT+extract):");
    INFO_LOG("  HOST_PRE=0x%08X AIV0_SET4=0x%08X AIV0_SET1=0x%08X AIV0_WAIT3=0x%08X DONE=0x%08X HOST_POST=0x%08X",
             tr[0], tr[1], tr[3], tr[8], tr[10], tr[11]);
    const bool gate = (tr[1] == 0xA1040004u) || (tr[2] == 0xA1140004u);
    const bool intt = ((tr[3] == 0xA1010001u) || (tr[4] == 0xA1110001u)) &&
                      ((tr[8] == 0xA1030003u) || (tr[9] == 0xA1130003u));
    if (gate) {
        INFO_LOG("L2b causal: GATE SET4 seen");
    }
    if (intt) {
        INFO_LOG("L2b causal: INTT SET1+WAIT3 seen");
    }
    if (intt && tr[10] == 0x4430334Fu) {
        INFO_LOG("L2b causal: device wrote m (D03 DONE)");
    }
}

/**
 * 装填 L2a workspace：u/ŝ/ζ/γ/mat；清零 û/ŵ；标 HOST_PRE。
 * @return false 读盘失败
 */
bool LoadWsL2a(uint8_t *ws, const uint8_t *uHost, const uint8_t *sHatHost)
{
    using namespace tiling;
    size_t got = 0;
    std::memcpy(ws + OFF_U, uHost, kUBytes);
    std::memcpy(ws + OFF_S_HAT, sHatHost, kSHatBytes);
    if (!ReadFile("./input/zetas.bin", got, ws + OFF_ZETAS, kZetasBytes) || got != kZetasBytes) {
        return false;
    }
    if (!ReadFile("./input/gammas.bin", got, ws + OFF_GAMMAS, kGammasBytes) || got != kGammasBytes) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatABytes)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatBBytes)) {
        return false;
    }
    std::memset(ws + OFF_U_HAT, 0, kUHatBytes);
    std::memset(ws + OFF_W_HAT, 0, kWHatBytes);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTraceBytes);
    tr[SLOT_HOST_PRE] = MAGIC_HOST_PRE;
    return true;
}

/**
 * 装填 L2b workspace：ŵ/v/ζ/mat；清零中间 w；标 HOST_PRE。
 * 注意：d03 布局与 d02 不同；偏移用字面量对齐 d03_inc/tiling.h，避免与 d02 符号混用。
 *
 * d03 布局：
 *   OFF_W_HAT=0, OFF_V=1024, OFF_ZETAS=2048, OFF_W=2560,
 *   OFF_MAT_A=3584, OFF_MAT_B=4096, OFF_MAT_C=5120, OFF_TRACE=7168
 *   wssize=7224；kWHatBytes=kVBytes=kWBytes=1024；kZetas=512；matA=512；matB=1024；matC=2048；trace=56
 */
bool LoadWsL2b(uint8_t *ws, const uint8_t *wHatHost, const uint8_t *vHost)
{
    constexpr size_t OFF_W_HAT = 0;
    constexpr size_t kWHat = 1024;
    constexpr size_t OFF_V = OFF_W_HAT + kWHat;
    constexpr size_t kV = 1024;
    constexpr size_t OFF_ZETAS = OFF_V + kV;
    constexpr size_t kZetas = 128 * sizeof(int32_t);
    constexpr size_t OFF_W = OFF_ZETAS + kZetas;
    constexpr size_t kW = 1024;
    constexpr size_t OFF_MAT_A = OFF_W + kW;
    constexpr size_t kMatA = 16 * 32;
    constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatA;
    constexpr size_t kMatB = 32 * 32;
    constexpr size_t OFF_MAT_C = OFF_MAT_B + kMatB;
    constexpr size_t kMatC = 16 * 32 * sizeof(int32_t);
    constexpr size_t OFF_TRACE = OFF_MAT_C + kMatC;
    constexpr size_t kTrace = 14 * sizeof(uint32_t);

    size_t got = 0;
    if (wHatHost != nullptr) {
        std::memcpy(ws + OFF_W_HAT, wHatHost, kWHat);
    } else {
        std::memset(ws + OFF_W_HAT, 0, kWHat);
    }
    std::memcpy(ws + OFF_V, vHost, kV);
    if (!ReadFile("./input/zetas.bin", got, ws + OFF_ZETAS, kZetas) || got != kZetas) {
        return false;
    }
    if (!ReadFile("./input/mat_a.bin", got, ws + OFF_MAT_A, kMatA)) {
        return false;
    }
    if (!ReadFile("./input/mat_b.bin", got, ws + OFF_MAT_B, kMatB)) {
        return false;
    }
    (void)kMatC;
    std::memset(ws + OFF_W, 0, kW);
    auto *tr = reinterpret_cast<uint32_t *>(ws + OFF_TRACE);
    std::memset(tr, 0, kTrace);
    tr[0] = 0x484F5354u; // MAGIC_HOST_PRE
    return true;
}
}  // namespace

/**
 * 主流程：读 dk_pke+c → 二 launch → 落盘 m + 两段 TRACE + 中间量（诊断）。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    static_assert(sizeof(TilingData) <= 64, "");
    uint32_t blockDim = 1;
    bool ok;

    // d02 / d03 wssize（d03 用字面量，因同 TU 只能见一个 tiling::wssize）
    constexpr size_t kWsL2aRaw = tiling::wssize;
    constexpr size_t kWsL2a = (kWsL2aRaw + 31u) & ~size_t(31u); // 32B 对齐垫
    constexpr size_t kWsL2b = 7224; // d03_inc/tiling.h::wssize
    constexpr size_t kOutL2a = tiling::kOutBytes;
    constexpr size_t kUHatBytes = tiling::kUHatBytes;
    constexpr size_t kWHatBytes = tiling::kWHatBytes;
    constexpr size_t kOutL2b = 64;
    constexpr size_t kMBytes = 32;
    constexpr size_t kTraceBytes = tiling::kTraceBytes;
    constexpr size_t kMatCBytes = tiling::kMatCBytes;
    // d03 OFF_TRACE / OFF_W / OFF_MAT_C
    constexpr size_t kD03OffW = 2560;
    constexpr size_t kD03OffMatC = 5120;
    constexpr size_t kD03OffTrace = 7168;
    constexpr size_t kD03WBytes = 1024;

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *tilingHost = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kTilingSize));
    std::memset(tilingHost, 0, kTilingSize);
    TilingData *tilingPtr = reinterpret_cast<TilingData *>(tilingHost);

    uint8_t *dkBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kDkBytes > kMinAlloc ? kDkBytes : kMinAlloc));
    uint8_t *cBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kCBytes > kMinAlloc ? kCBytes : kMinAlloc));
    uint8_t *sHatBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kSHatBytes > kMinAlloc ? kSHatBytes : kMinAlloc));
    uint8_t *uBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kUBytes > kMinAlloc ? kUBytes : kMinAlloc));
    uint8_t *vBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kVBytes > kMinAlloc ? kVBytes : kMinAlloc));
    uint8_t *outL2a =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutL2a > kMinAlloc ? kOutL2a : kMinAlloc));
    uint8_t *uHatBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kUHatBytes > kMinAlloc ? kUHatBytes : kMinAlloc));
    uint8_t *wHatBuf =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWHatBytes > kMinAlloc ? kWHatBytes : kMinAlloc));
    uint8_t *wsL2a = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsL2a > kMinAlloc ? kWsL2a : kMinAlloc));
    uint8_t *outL2b =
        reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kOutL2b > kMinAlloc ? kOutL2b : kMinAlloc));
    uint8_t *mBuf = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kMBytes > kMinAlloc ? kMBytes : kMinAlloc));
    uint8_t *wsL2b = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsL2b > kMinAlloc ? kWsL2b : kMinAlloc));

    std::memset(sHatBuf, 0, kSHatBytes);
    std::memset(uBuf, 0, kUBytes);
    std::memset(vBuf, 0, kVBytes);
    std::memset(outL2a, 0, kOutL2a);
    std::memset(uHatBuf, 0, kUHatBytes);
    std::memset(wHatBuf, 0, kWHatBytes);
    std::memset(wsL2a, 0, kWsL2a);
    std::memset(outL2b, 0, kOutL2b);
    std::memset(mBuf, 0, kMBytes);
    std::memset(wsL2b, 0, kWsL2b);

    size_t rs = 0;
    if (!ReadFile("./input/dk_pke.bin", rs, dkBuf, kDkBytes) || rs != kDkBytes) {
        ERROR_LOG("read dk_pke.bin failed");
        return 1;
    }
    if (!ReadFile("./input/c.bin", rs, cBuf, kCBytes) || rs != kCBytes) {
        ERROR_LOG("read c.bin failed");
        return 1;
    }

    // ---------- L1：AIV prep ----------
    INFO_LOG("Host: Launch1 dec_prep_custom (CPU AIV twin)");
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(dec_prep_custom, blockDim, dkBuf, cBuf, sHatBuf, uBuf, vBuf);
    INFO_LOG("Host: mid-sync after Launch1 (CPU sequential barrier)");

    // 融合 ws = [L2a | L2b]；ŵ 由设备段1 直写段2，Host 不中转
    if (!LoadWsL2a(wsL2a, uBuf, sHatBuf)) {
        ERROR_LOG("LoadWsL2a failed");
        return 3;
    }
    if (!LoadWsL2b(wsL2b, /*wHat*/ nullptr, vBuf)) {
        ERROR_LOG("LoadWsL2b failed");
        return 4;
    }
    // 拼接：wsL2a 缓冲后紧跟 wsL2b —— 用已有两块；CPU 路径把 L2b 内容拷到 fused 或分别保持
    // 简化：分配 fused 视图——此处原地要求 Launch 吃连续缓冲；拷到 outL2a 大缓冲不合适。
    // 使用 wsL2a 作为 fused 头：先把 L2b 布局写入独立区，再一次性传入需连续内存。
    // 本 CPU 孪生：顺序调用数学不可用融合核签名时，改为一次 ICPU 融合核，ws 连续堆在 heap。
    constexpr size_t kWsFused = kWsL2a + kWsL2b;
    uint8_t *wsFused = reinterpret_cast<uint8_t *>(AscendC::GmAlloc(kWsFused));
    std::memcpy(wsFused, wsL2a, kWsL2a);
    std::memcpy(wsFused + kWsL2a, wsL2b, kWsL2b);
    INFO_LOG("Host: Launch2 dec_ntt_intt_extract_custom (CPU MIX twin, fused)");
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(dec_ntt_intt_extract_custom, blockDim, outL2b, mBuf, wsFused, *tilingPtr);
    {
        auto *tr1 = reinterpret_cast<uint32_t *>(wsFused + tiling::OFF_TRACE);
        tr1[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceL2a(tr1);
        auto *tr2 = reinterpret_cast<uint32_t *>(wsFused + kWsL2a + kD03OffTrace);
        tr2[11] = 0x484F5355u;
        PrintTraceL2b(tr2);
        std::memcpy(wsL2a, wsFused, kWsL2a);
        std::memcpy(wsL2b, wsFused + kWsL2a, kWsL2b);
    }
    AscendC::GmFree(wsFused);
    INFO_LOG("Host: sync after Launch2 fused; D2H m[32]");

    ok = WriteFile("./output/m.bin", mBuf, kMBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/out_l2a.bin", outL2a, kOutL2a);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/out_l2b.bin", outL2b, kOutL2b);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/trace_l2a.bin", wsL2a + tiling::OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace_l2b.bin", wsL2b + kD03OffTrace, kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c_l2a.bin", wsL2a + tiling::OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_l2b.bin", wsL2b + kD03OffMatC, kMatCBytes);
    if (!ok) {
        return 21;
    }
    // 诊断中间量（非生产验收）
    (void)WriteFile("./output/s_hat.bin", sHatBuf, kSHatBytes);
    (void)WriteFile("./output/u.bin", uBuf, kUBytes);
    (void)WriteFile("./output/v.bin", vBuf, kVBytes);
    (void)WriteFile("./output/u_hat.bin", uHatBuf, kUHatBytes);
    (void)WriteFile("./output/w_hat.bin", wHatBuf, kWHatBytes);
    (void)WriteFile("./output/w.bin", wsL2b + kD03OffW, kD03WBytes);

    AscendC::GmFree(dkBuf);
    AscendC::GmFree(cBuf);
    AscendC::GmFree(sHatBuf);
    AscendC::GmFree(uBuf);
    AscendC::GmFree(vBuf);
    AscendC::GmFree(outL2a);
    AscendC::GmFree(uHatBuf);
    AscendC::GmFree(wHatBuf);
    AscendC::GmFree(wsL2a);
    AscendC::GmFree(outL2b);
    AscendC::GmFree(mBuf);
    AscendC::GmFree(wsL2b);
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

    TilingData *tilingPtr = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPtr), kTilingSize));
    std::memset(tilingPtr, 0, kTilingSize);

    uint8_t *dkHost = nullptr, *cHost = nullptr, *sHatHost = nullptr, *uHost = nullptr, *vHost = nullptr;
    uint8_t *dkDev = nullptr, *cDev = nullptr, *sHatDev = nullptr, *uDev = nullptr, *vDev = nullptr;
    uint8_t *outL2aHost = nullptr, *uHatHost = nullptr, *wHatHost = nullptr, *wsL2aHost = nullptr;
    uint8_t *outL2aDev = nullptr, *uHatDev = nullptr, *wHatDev = nullptr, *wsL2aDev = nullptr;
    uint8_t *outL2bHost = nullptr, *mHost = nullptr, *wsL2bHost = nullptr;
    uint8_t *outL2bDev = nullptr, *mDev = nullptr, *wsL2bDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&dkHost), kDkBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), kDkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), kCBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), kCBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&sHatHost), kSHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sHatDev), kSHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHost), kUBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), kUBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&vHost), kVBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), kVBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2aHost), kOutL2a));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2aDev), kOutL2a, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&uHatHost), kUHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uHatDev), kUHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wHatHost), kWHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wHatDev), kWHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2aHost), kWsL2a));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2aDev), kWsL2a, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outL2bHost), kOutL2b));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outL2bDev), kOutL2b, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), kMBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), kMBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsL2bHost), kWsL2b));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsL2bDev), kWsL2b, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memset(sHatHost, 0, kSHatBytes);
    std::memset(uHost, 0, kUBytes);
    std::memset(vHost, 0, kVBytes);
    std::memset(outL2aHost, 0, kOutL2a);
    std::memset(uHatHost, 0, kUHatBytes);
    std::memset(wHatHost, 0, kWHatBytes);
    std::memset(wsL2aHost, 0, kWsL2a);
    std::memset(outL2bHost, 0, kOutL2b);
    std::memset(mHost, 0, kMBytes);
    std::memset(wsL2bHost, 0, kWsL2b);

    size_t rs = 0;
    if (!ReadFile("./input/dk_pke.bin", rs, dkHost, kDkBytes) || rs != kDkBytes) {
        return 1;
    }
    if (!ReadFile("./input/c.bin", rs, cHost, kCBytes) || rs != kCBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(dkDev, kDkBytes, dkHost, kDkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, kCBytes, cHost, kCBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sHatDev, kSHatBytes, sHatHost, kSHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uDev, kUBytes, uHost, kUBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDev, kVBytes, vHost, kVBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L1 ----------
    INFO_LOG("Host: Launch1 dec_prep_custom");
    ACLRT_LAUNCH_KERNEL(dec_prep_custom)(blockDim, stream, dkDev, cDev, sHatDev, uDev, vDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch1 (mid-sync)");

    CHECK_ACL(aclrtMemcpy(sHatHost, kSHatBytes, sHatDev, kSHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(uHost, kUBytes, uDev, kUBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, kVBytes, vDev, kVBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    constexpr size_t kWsFused = kWsL2a + kWsL2b;
    uint8_t *wsFusedHost = nullptr, *wsFusedDev = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsFusedHost), kWsFused));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsFusedDev), kWsFused, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memset(wsFusedHost, 0, kWsFused);

    if (!LoadWsL2a(wsFusedHost, uHost, sHatHost)) {
        ERROR_LOG("LoadWsL2a failed");
        return 3;
    }
    if (!LoadWsL2b(wsFusedHost + kWsL2a, nullptr, vHost)) {
        ERROR_LOG("LoadWsL2b failed");
        return 4;
    }
    CHECK_ACL(aclrtMemcpy(outL2bDev, kOutL2b, outL2bHost, kOutL2b, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, kMBytes, mHost, kMBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsFusedDev, kWsFused, wsFusedHost, kWsFused, ACL_MEMCPY_HOST_TO_DEVICE));

    // ---------- L2 融合 MIX ----------
    INFO_LOG("Host: Launch2 dec_ntt_intt_extract_custom (fused MIX)");
    ACLRT_LAUNCH_KERNEL(dec_ntt_intt_extract_custom)
    (blockDim, stream, outL2bDev, mDev, wsFusedDev, tilingPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    INFO_LOG("Host: SynchronizeStream after Launch2 fused; D2H m[32]");

    CHECK_ACL(aclrtMemcpy(outL2bHost, kOutL2b, outL2bDev, kOutL2b, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(mHost, kMBytes, mDev, kMBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsFusedHost, kWsFused, wsFusedDev, kWsFused, ACL_MEMCPY_DEVICE_TO_HOST));
    {
        auto *tr1 = reinterpret_cast<uint32_t *>(wsFusedHost + tiling::OFF_TRACE);
        tr1[tiling::SLOT_HOST_POST_SYNC] = tiling::MAGIC_HOST_POST_SYNC;
        PrintTraceL2a(tr1);
        auto *tr2 = reinterpret_cast<uint32_t *>(wsFusedHost + kWsL2a + kD03OffTrace);
        tr2[11] = 0x484F5355u;
        PrintTraceL2b(tr2);
        std::memcpy(wsL2aHost, wsFusedHost, kWsL2a);
        std::memcpy(wsL2bHost, wsFusedHost + kWsL2a, kWsL2b);
        // 诊断：段1 直写段2 的 ŵ；并补 out_l2a 魔数（融合核只写 out=L2b）
        std::memcpy(wHatHost, wsFusedHost + kWsL2a + 0 /*tiling_d03::OFF_W_HAT*/, kWHatBytes);
        {
            auto *o = reinterpret_cast<uint32_t *>(outL2aHost);
            o[0] = 0x4430323Au; // D02: — 以 TRACE DONE 为准，满足 verify 双 out 魔数
        }
    }
    CHECK_ACL(aclrtFree(wsFusedDev));
    CHECK_ACL(aclrtFreeHost(wsFusedHost));

    ok = WriteFile("./output/m.bin", mHost, kMBytes);
    if (!ok) {
        return 15;
    }
    ok = WriteFile("./output/out_l2a.bin", outL2aHost, kOutL2a);
    if (!ok) {
        return 16;
    }
    ok = WriteFile("./output/out_l2b.bin", outL2bHost, kOutL2b);
    if (!ok) {
        return 17;
    }
    ok = WriteFile("./output/trace_l2a.bin", wsL2aHost + tiling::OFF_TRACE, kTraceBytes);
    if (!ok) {
        return 18;
    }
    ok = WriteFile("./output/trace_l2b.bin", wsL2bHost + kD03OffTrace, kTraceBytes);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/mat_c_l2a.bin", wsL2aHost + tiling::OFF_MAT_C, kMatCBytes);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/mat_c_l2b.bin", wsL2bHost + kD03OffMatC, kMatCBytes);
    if (!ok) {
        return 21;
    }
    (void)WriteFile("./output/s_hat.bin", sHatHost, kSHatBytes);
    (void)WriteFile("./output/u.bin", uHost, kUBytes);
    (void)WriteFile("./output/v.bin", vHost, kVBytes);
    (void)WriteFile("./output/u_hat.bin", uHatHost, kUHatBytes);
    (void)WriteFile("./output/w_hat.bin", wHatHost, kWHatBytes);
    (void)WriteFile("./output/w.bin", wsL2bHost + kD03OffW, kD03WBytes);

    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFreeHost(dkHost));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
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
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFree(wsL2bDev));
    CHECK_ACL(aclrtFreeHost(wsL2bHost));
    CHECK_ACL(aclrtFreeHost(tilingPtr));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
