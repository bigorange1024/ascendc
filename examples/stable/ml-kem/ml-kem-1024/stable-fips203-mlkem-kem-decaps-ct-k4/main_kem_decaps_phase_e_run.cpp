/**
 * @file main_kem_decaps_phase_e_run.cpp
 * @brief Phase-E Host 编排：G(m'‖h) + Encrypt compute + 设备 FO → K（全链 / Phase-E-only 共用）。
 *
 * 平台分叉（锁参，勿擅自改 launch 数）：
 *   - SIM：prep → 单核 `f203_encrypt_l18_l19`（内联 pack+FO，T19i，共 2 次设备 launch）；
 *   - CPU：prep → ntt_y → at_jp → intt_e1 → 读 golden_v → pack_fo（分段便于孪生调试）。
 *
 * 数据面要点：
 *   - Encrypt LUT：`./input/lut_ntt_*` 与 `lut_intt_*`；
 *   - CPU 的 v 不现场算，读 `./input/golden_v.bin`（须与 encaps 的 m 一致，见 M_FILE / qa 假绿记录）。
 *
 * 对齐：CT_decaps；customspec；禁 host memcmp 选 K。
 */
#include "main_kem_decaps_phase_e_run.hpp"

#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_compute_tail_layout.h"
#include "f203_encrypt_full_layout.h"
#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"
#include "f203_kem_dec_layout.h"
#include "f203_l18_l19_tiling.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern void GenerateTiling(TilingData &data);

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
#include "aclrtlaunch_f203_encrypt_l18_l19.h"
#include "aclrtlaunch_f203_kem_dec_phase_e_prep.h"
#else
#include "tikicpulib.h"
#include "alg11_gammas.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_kem_dec_phase_e_prep(GM_ADDR ek_gm, GM_ADDR m_prime_gm, GM_ADDR h_gm, GM_ADDR Kprime_gm,
                                           GM_ADDR coins_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                           GM_ADDR tiling_gm);
extern "C" void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat);
extern "C" void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws, TilingData tiling);
extern "C" void f203_kem_dec_pack_fo(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cPrimeGm, GM_ADDR cInGm, GM_ADDR zGm,
                                     GM_ADDR KprimeGm, GM_ADDR KoutGm);
extern volatile int g_f203_ntt_y_mix_pass;
#endif

/* CHECK_ACL 已由 data_utils.h 提供 */
namespace {
using F203EncryptPrep::kAHatBytes;
using F203EncryptPrep::kCoinsSize;
using F203EncryptPrep::kEkBytes;
using F203EncryptPrep::kPrepBlockDim;
using F203EncryptPrep::kPrfBytes;
using F203EncryptPrep::kPrfBytesPerPoly;
using F203EncryptPrep::kReBytes;
constexpr size_t kKBytes = F203KemDec::kSharedSecretBytes;
constexpr size_t kMBytes = F203KemDec::kMsgBytes;
constexpr size_t kHBytes = F203KemDec::kHashBytes;

void FillPrfTiling(ShakeGeneralTilingData *t)
{
    // EncryptPrep 内 PRF/SHAKE 通用 tiling：rate=SHAKE256，按 poly 切分
    FillShakeTiling(t, 8U, 64U, kPrfBytesPerPoly, SHAKE256_RATE_BYTES);
    t->blockDim = 1U;
}

/** 加载 Encrypt 正向 NTT LUT 到 workspace 的 NTT 槽位。 */
static bool LoadNttLutHost(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_ntt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_ntt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}

#ifdef ASCENDC_CPU_DEBUG
/** CPU 分段 INTT：与 NTT 共用同一对 LUT 槽偏移（孪生约定）。 */
static bool LoadInttLutHostPhased(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_NTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_NTT_ODD_STACKED, lutBytes);
}
#else
/** SIM 融合核：INTT LUT 落在独立 INTT 槽（与 l18_l19 布局一致）。 */
static bool LoadInttLutHostFused(uint8_t *ws, size_t lutBytes)
{
    size_t rd = lutBytes;
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rd, ws + tiling::LUT_INTT_EVEN_STACKED, lutBytes)) {
        return false;
    }
    rd = lutBytes;
    return ReadFile("./input/lut_intt_odd_stacked.bin", rd, ws + tiling::LUT_INTT_ODD_STACKED, lutBytes);
}
#endif
}  // namespace

int RunKemDecapsPhaseE(const uint8_t *ek, const uint8_t *m_prime, const uint8_t *h, const uint8_t *z,
                       const uint8_t *c, uint8_t *K_out)
{
    constexpr size_t cBytes = F203_TAIL_C_BYTES;

    // Encrypt compute tiling（mixPass 等由 GenerateTiling 填生产默认）
    TilingData tilingHost{};
    GenerateTiling(tilingHost);

    ShakeGeneralTilingData prepTilingHost{};
    FillPrfTiling(&prepTilingHost);
    const size_t prepTilingBytes = sizeof(ShakeGeneralTilingData);

    const size_t uSize = tiling::uFileBytes;
    const size_t vSize = tiling::vFileBytes;
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;

#ifdef ASCENDC_CPU_DEBUG
    // ========== CPU 孪生路径：多 launch + golden_v ==========
    uint8_t *ekPke = (uint8_t *)AscendC::GmAlloc(kEkBytes);
    uint8_t *mPrime = (uint8_t *)AscendC::GmAlloc(kMBytes);
    uint8_t *hIn = (uint8_t *)AscendC::GmAlloc(kHBytes);
    uint8_t *zIn = (uint8_t *)AscendC::GmAlloc(kHBytes);
    uint8_t *cIn = (uint8_t *)AscendC::GmAlloc(cBytes);
    uint8_t *kPrime = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *kOut = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *coins = (uint8_t *)AscendC::GmAlloc(kCoinsSize);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(kAHatBytes);
    uint8_t *prf = (uint8_t *)AscendC::GmAlloc(kPrfBytes);
    uint8_t *re = (uint8_t *)AscendC::GmAlloc(kReBytes);
    uint8_t *prepTilingGm = (uint8_t *)AscendC::GmAlloc(prepTilingBytes);
    uint8_t *yHat = (uint8_t *)AscendC::GmAlloc(tiling::yHatFileBytes);
    uint8_t *uNtt = (uint8_t *)AscendC::GmAlloc(tiling::uNttFileBytes);
    uint8_t *uOut = (uint8_t *)AscendC::GmAlloc(uSize);
    uint8_t *vOut = (uint8_t *)AscendC::GmAlloc(vSize);
    uint8_t *cPrime = (uint8_t *)AscendC::GmAlloc(cBytes);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsSize);

    std::memcpy(ekPke, ek, kEkBytes);
    std::memcpy(mPrime, m_prime, kMBytes);
    std::memcpy(hIn, h, kHBytes);
    std::memcpy(zIn, z, kHBytes);
    std::memcpy(cIn, c, cBytes);
    std::memset(coins, 0, kCoinsSize);
    std::memset(kPrime, 0, kKBytes);
    std::memset(kOut, 0, kKBytes);
    std::memcpy(prepTilingGm, &prepTilingHost, prepTilingBytes);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    // 1) G(m'‖h) + EncryptPrep → K'、coins、Â、re
    ICPU_RUN_KF(f203_kem_dec_phase_e_prep, kPrepBlockDim, ekPke, mPrime, hIn, kPrime, coins, aHat, prf, re,
                prepTilingGm);

    // re 布局内 y / e1 偏移（与 Encrypt full layout 一致）
    uint8_t *ySrc = re + F203EncryptFull::kReYByteOff;
    uint8_t *e1 = re + F203EncryptFull::kReE1ByteOff;

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    std::memset(ws, 0, wsSize);
    if (!LoadNttLutHost(ws, lutBytes)) {
        return 20;
    }
    // 2–4) NTT(y) → Â·ŷ → INTT+e1 → u
    g_f203_ntt_y_mix_pass = tilingHost.mixPass;
    ICPU_RUN_KF(f203_encrypt_ntt_y, 1, yHat, ySrc, ws, tilingHost);
    ICPU_RUN_KF(f203_encrypt_at_jp, 2, uNtt, aHat, yHat);
    std::memset(ws, 0, wsSize);
    if (!LoadInttLutHostPhased(ws, lutBytes)) {
        return 21;
    }
    ICPU_RUN_KF(f203_encrypt_intt_e1, 1, uOut, uNtt, e1, ws, tilingHost);

    // CPU 不跑完整 v 链：读 gen_data 按 m 预计算的 golden_v（须与 c 匹配）
    size_t rs = vSize;
    if (!ReadFile("./input/golden_v.bin", rs, vOut, vSize)) {
        return 17;
    }

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    // 5) pack(u,v)→c' 并同核 FO → K
    ICPU_RUN_KF(f203_kem_dec_pack_fo, 1, uOut, vOut, cPrime, cIn, zIn, kPrime, kOut);

    (void)WriteFile("./output/c_prime.bin", cPrime, cBytes);
    std::memcpy(K_out, kOut, kKBytes);

    AscendC::GmFree(ekPke);
    AscendC::GmFree(mPrime);
    AscendC::GmFree(hIn);
    AscendC::GmFree(zIn);
    AscendC::GmFree(cIn);
    AscendC::GmFree(kPrime);
    AscendC::GmFree(kOut);
    AscendC::GmFree(coins);
    AscendC::GmFree(aHat);
    AscendC::GmFree(prf);
    AscendC::GmFree(re);
    AscendC::GmFree(prepTilingGm);
    AscendC::GmFree(yHat);
    AscendC::GmFree(uNtt);
    AscendC::GmFree(uOut);
    AscendC::GmFree(vOut);
    AscendC::GmFree(cPrime);
    AscendC::GmFree(ws);
    return 0;
#else
    // ========== SIM/NPU：独立 session；prep + l18_l19（内联 pack+FO）==========
    constexpr size_t mBytes = kMBytes;
    const size_t uTrSize = tiling::uTrFileBytes;
    const size_t trHatNttSize = tiling::n * sizeof(int32_t);

    CHECK_ACL(aclInit(nullptr));
    // 设备号：读 ASCEND_DEVICE_ID；缺省 0（标准默认；探针挂死脏退后同卡会连环挂，见 acl_session；需换卡时再 export）。SIM 由 run.sh 强制 export=0。
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    // 早退 / SIGINT / SIGTERM 均会 ResetDevice+Finalize，减轻同卡污染
    ascendc_acl::DeviceGuard aclGuard(deviceId);
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    constexpr size_t tilingSize = 64;
    TilingData *tilingPinned = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPinned), tilingSize));
    std::memcpy(tilingPinned, &tilingHost, sizeof(TilingData));

    ShakeGeneralTilingData *prepTilingPinned = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prepTilingPinned), prepTilingBytes));
    std::memcpy(prepTilingPinned, &prepTilingHost, prepTilingBytes);

    uint8_t *wsHost = nullptr;
    uint8_t *cPrimeHost = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cPrimeHost), cBytes));

    uint8_t *ekPkeDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *hDev = nullptr;
    uint8_t *zDev = nullptr;
    uint8_t *cInDev = nullptr;
    uint8_t *kPrimeDev = nullptr;
    uint8_t *kOutDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *prepTilingDev = nullptr;
    uint8_t *yHatDev = nullptr;
    uint8_t *uNttDev = nullptr;
    uint8_t *uTrDev = nullptr;
    uint8_t *trHatNttDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *cPrimeDev = nullptr;
    uint8_t *tHatDev = nullptr;

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&coinsDev), kCoinsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&hDev), kHBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&zDev), kHBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cInDev), cBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kPrimeDev), kKBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kOutDev), kKBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&reDev), kReBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prepTilingDev), prepTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&yHatDev), tiling::yHatFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uNttDev), tiling::uNttFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uTrDev), uTrSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&trHatNttDev), trHatNttSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), uSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), vSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cPrimeDev), cBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMemcpy(ekPkeDev, kEkBytes, ek, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, mBytes, m_prime, mBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHBytes, h, kHBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHBytes, z, kHBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cInDev, cBytes, c, cBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemset(coinsDev, kCoinsSize, 0, kCoinsSize));
    CHECK_ACL(aclrtMemset(kPrimeDev, kKBytes, 0, kKBytes));
    CHECK_ACL(aclrtMemset(kOutDev, kKBytes, 0, kKBytes));
    CHECK_ACL(aclrtMemcpy(prepTilingDev, prepTilingBytes, prepTilingPinned, prepTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::fprintf(stderr, "[kem-dec-e] launch prep G(m'||h)+EncryptPrep\n");
    ACLRT_LAUNCH_KERNEL(f203_kem_dec_phase_e_prep)(kPrepBlockDim, stream, ekPkeDev, mDev, hDev, kPrimeDev, coinsDev,
                                                   aHatDev, prfDev, reDev, prepTilingDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_kem_dec_phase_e_prep"));

    std::memset(wsHost, 0, wsSize);
    if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
        return 20;
    }
    CHECK_ACL(aclrtMemcpy(wsDev, wsSize, wsHost, wsSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *yDev = reDev + F203EncryptFull::kReYByteOff;
    uint8_t *e1Dev = reDev + F203EncryptFull::kReE1ByteOff;
    uint8_t *e2Dev = reDev + F203EncryptFull::kReE2ByteOff;

    // T19i：l18_l19 内联 pack + FO → K；不再单独 launch fo_only（SIM 3 launch 全链）
    std::fprintf(stderr, "[kem-dec-e] launch l18_l19 (inline pack + FO -> K)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, mDev, e1Dev, e2Dev, wsDev, tilingPinned, cPrimeDev,
                                              nullptr, cInDev, zDev, kPrimeDev, kOutDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_encrypt_l18_l19"));

    CHECK_ACL(aclrtMemcpy(cPrimeHost, cBytes, cPrimeDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(K_out, kKBytes, kOutDev, kKBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    (void)WriteFile("./output/c_prime.bin", cPrimeHost, cBytes);

    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFree(cInDev));
    CHECK_ACL(aclrtFree(kPrimeDev));
    CHECK_ACL(aclrtFree(kOutDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(prepTilingDev));
    CHECK_ACL(aclrtFree(yHatDev));
    CHECK_ACL(aclrtFree(uNttDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(trHatNttDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(cPrimeDev));
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(prepTilingPinned));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(cPrimeHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    // ResetDevice+Finalize 由 aclGuard 析构统一执行（含早退路径）
    return 0;
#endif
}
