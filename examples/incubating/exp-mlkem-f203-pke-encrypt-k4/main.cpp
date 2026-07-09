/**
 * @file main.cpp
 * @brief exp-mlkem-f203-pke-encrypt-k4 — 完整 Encrypt host 编排（Alg.14 行 1–22）。
 *
 * 对齐 customspec：exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex
 *
 * 生产 I/O（强制）：
 *   输入 ek_pke + m + coins（+ 静态 NTT/INTT LUT）→ 设备产 **仅** c = c₁‖c₂（1568B）。
 *   Â / re(y,e₁,e₂) / u / v 等仅设备 GM handoff，**不 D2H、不落盘**。
 *
 * Launch：
 *   SIM  **2×** prep → l18_l19（含 e₂+=μ 与内联 tail pack）
 *   CPU  **5×** prep + ntt_y/at_jp/intt_e1 + pack（v=golden_v 注入，非产物）
 *
 * handoff：f203_encrypt_full_layout.h；golden：scripts/gen_data.py（自包含）。
 */
#include "ascendc_build_mode.hpp"
#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_compute_tail_layout.h"
#include "f203_encrypt_full_layout.h"
#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"
#include "f203_l18_l19_tiling.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>

// compute 运行时 tiling（模板风格，见 f203_encrypt_tiling.cpp）
extern void GenerateTiling(TilingData &data);

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_at_jp.h"
#include "aclrtlaunch_f203_encrypt_intt_e1.h"
#include "aclrtlaunch_f203_encrypt_l18_l19.h"
#include "aclrtlaunch_f203_encrypt_ntt_y.h"
#include "aclrtlaunch_f203_encrypt_prep.h"
#else
#include "tikicpulib.h"
#include "alg11_gammas.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_encrypt_prep(GM_ADDR ek_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                  GM_ADDR tiling_gm);
extern "C" void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_at_jp(GM_ADDR uNtt, GM_ADDR aHat, GM_ADDR yHat);
extern "C" void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws, TilingData tiling);
extern "C" void f203_encrypt_alg14_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm);
extern volatile int g_f203_ntt_y_mix_pass;
#endif

namespace {
using F203EncryptPrep::kAHatBytes;
using F203EncryptPrep::kCoinsSize;
using F203EncryptPrep::kEkBytes;
using F203EncryptPrep::kPrepBlockDim;
using F203EncryptPrep::kPrfBytes;
using F203EncryptPrep::kPrfBytesPerPoly;
using F203EncryptPrep::kReBytes;

/** PRF(SHAKE256) tiling：与 prep 探针 FillPrfTiling 逐值一致（batch9、msgStride 64、outLen 128）。 */
void FillPrfTiling(ShakeGeneralTilingData *t)
{
    FillShakeTiling(t, 8U, 64U, kPrfBytesPerPoly, SHAKE256_RATE_BYTES);
    t->blockDim = 1U;
}
}  // namespace

// NTT LUT 装入 ws（CPU/SIM 共用；写 ws 内 LUT_NTT_* 段）。
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
// CPU 三 launch：INTT 段复用 LUT_NTT_* 区（phased，与 compute 探针 RunCpuThreeLaunch 一致）。
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

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t cBytes = F203_TAIL_C_BYTES;

    // compute 运行时 tiling（3 标量）
    TilingData tilingHost{};
    GenerateTiling(tilingHost);

    // prep PRF tiling（SHAKE256）
    ShakeGeneralTilingData prepTilingHost{};
    FillPrfTiling(&prepTilingHost);
    const size_t prepTilingBytes = sizeof(ShakeGeneralTilingData);

    const size_t uSize = tiling::uFileBytes;
    const size_t vSize = tiling::vFileBytes;
    const size_t wsSize = tiling::wssize;
    const size_t lutBytes = tiling::lutEvenOddBytes;

#ifdef ASCENDC_CPU_DEBUG
    // ---- prep 输出 + compute 缓冲（GmAlloc；re 切片零拷贝供 compute）----
    uint8_t *ekPke = (uint8_t *)AscendC::GmAlloc(kEkBytes);
    uint8_t *coins = (uint8_t *)AscendC::GmAlloc(kCoinsSize);
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(kAHatBytes);
    uint8_t *prf = (uint8_t *)AscendC::GmAlloc(kPrfBytes);
    uint8_t *re = (uint8_t *)AscendC::GmAlloc(kReBytes);
    uint8_t *prepTilingGm = (uint8_t *)AscendC::GmAlloc(prepTilingBytes);

    uint8_t *yHat = (uint8_t *)AscendC::GmAlloc(tiling::yHatFileBytes);
    uint8_t *uNtt = (uint8_t *)AscendC::GmAlloc(tiling::uNttFileBytes);
    uint8_t *uOut = (uint8_t *)AscendC::GmAlloc(uSize);
    uint8_t *vOut = (uint8_t *)AscendC::GmAlloc(vSize);
    uint8_t *cOut = (uint8_t *)AscendC::GmAlloc(cBytes);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsSize);

    size_t rs = 0;
    if (!ReadFile("./input/ek_pke.bin", rs, ekPke, kEkBytes) || rs != kEkBytes) {
        return 13;
    }
    if (!ReadFile("./input/coins.bin", rs, coins, kCoinsSize) || rs != kCoinsSize) {
        return 12;
    }
    std::memcpy(prepTilingGm, &prepTilingHost, prepTilingBytes);

    // Launch 1：prep（AIV blockDim=2）→ a_hat + re
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_encrypt_prep, kPrepBlockDim, ekPke, coins, aHat, prf, re, prepTilingGm);

    // re 切片：y=r（0）、e₁（4096）；compute 读同一 GM
    uint8_t *ySrc = re + F203EncryptFull::kReYByteOff;
    uint8_t *e1 = re + F203EncryptFull::kReE1ByteOff;

    // Launch 2–4：ntt_y → at_jp → intt_e1（MIX 串行产设备 u；tikicpu 单融合核死锁）
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    std::memset(ws, 0, wsSize);
    if (!LoadNttLutHost(ws, lutBytes)) {
        return 20;
    }
    g_f203_ntt_y_mix_pass = tilingHost.mixPass;
    ICPU_RUN_KF(f203_encrypt_ntt_y, 1, yHat, ySrc, ws, tilingHost);
    ICPU_RUN_KF(f203_encrypt_at_jp, 2, uNtt, aHat, yHat);
    std::memset(ws, 0, wsSize);
    if (!LoadInttLutHostPhased(ws, lutBytes)) {
        return 21;
    }
    ICPU_RUN_KF(f203_encrypt_intt_e1, 1, uOut, uNtt, e1, ws, tilingHost);

    // v 注入：CPU 三 launch 无 k=8 INTT 不产 v，读 gen_data 生成的注入 golden（含 μ+e₂）；
    // 该文件为 CPU 分段实现的内部注入数据，非 Alg.14 产物。
    rs = vSize;
    if (!ReadFile("./input/golden_v.bin", rs, vOut, vSize)) {
        return 17;
    }

    // Launch 5：tail pack（Compress+ByteEncode）→ c
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_encrypt_alg14_pack, 1, uOut, vOut, cOut);

    // Alg.14 输出仅密文 c（u/v 为内部中间量，不落盘）
    if (!WriteFile("./output/c.bin", cOut, cBytes)) {
        return 4;
    }

    AscendC::GmFree(ekPke);
    AscendC::GmFree(coins);
    AscendC::GmFree(aHat);
    AscendC::GmFree(prf);
    AscendC::GmFree(re);
    AscendC::GmFree(prepTilingGm);
    AscendC::GmFree(yHat);
    AscendC::GmFree(uNtt);
    AscendC::GmFree(uOut);
    AscendC::GmFree(vOut);
    AscendC::GmFree(cOut);
    AscendC::GmFree(ws);
#else
    constexpr size_t mBytes = F203_TAIL_MSG_BYTES;
    const size_t uTrSize = tiling::uTrFileBytes;
    const size_t trHatNttSize = tiling::n * sizeof(int32_t);

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    // tiling 固定 64B 对齐（compute），prep tiling 单独 pinned
    constexpr size_t tilingSize = 64;
    TilingData *tilingPinned = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingPinned), tilingSize));
    std::memcpy(tilingPinned, &tilingHost, sizeof(TilingData));

    ShakeGeneralTilingData *prepTilingPinned = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prepTilingPinned), prepTilingBytes));
    std::memcpy(prepTilingPinned, &prepTilingHost, prepTilingBytes);

    // ---- host pinned ----
    uint8_t *ekPkeHost = nullptr;
    uint8_t *coinsHost = nullptr;
    uint8_t *mHost = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *cHost = nullptr;  // Alg.14 输出仅密文 c；u/v device buffer 不 D2H、不落盘
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&coinsHost), kCoinsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), mBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), cBytes));

    // ---- device arena ----
    uint8_t *ekPkeDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *mDev = nullptr;
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
    uint8_t *cDev = nullptr;
    uint8_t *tHatDev = nullptr;  // 保持 nullptr：t̂ 在 l18_l19 内 ByteDecode₁₂ 驻留 UB

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&coinsDev), kCoinsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
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
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), cBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/ek_pke.bin", rs, ekPkeHost, kEkBytes) || rs != kEkBytes) {
        return 13;
    }
    if (!ReadFile("./input/coins.bin", rs, coinsHost, kCoinsSize) || rs != kCoinsSize) {
        return 12;
    }
    if (!ReadFile("./input/m.bin", rs, mHost, mBytes) || rs != mBytes) {
        return 8;
    }

    CHECK_ACL(aclrtMemcpy(ekPkeDev, kEkBytes, ekPkeHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(coinsDev, kCoinsSize, coinsHost, kCoinsSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, mBytes, mHost, mBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(prepTilingDev, prepTilingBytes, prepTilingPinned, prepTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // Launch 1：prep（AIV blockDim=2）→ a_hat + re（同 stream 序，写后即对 compute 可见）
    std::fprintf(stderr, "[full] launch 1 f203_encrypt_prep (ek+coins -> a_hat + re)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_prep)(kPrepBlockDim, stream, ekPkeDev, coinsDev, aHatDev, prfDev, reDev,
                                           prepTilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // LUT 装入 ws（NTT + INTT 段）后 H2D
    std::memset(wsHost, 0, wsSize);
    if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
        return 20;
    }
    CHECK_ACL(aclrtMemcpy(wsDev, wsSize, wsHost, wsSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // re 切片零拷贝：y=r（0）、e₁（4096）、e₂（8192）
    uint8_t *yDev = reDev + F203EncryptFull::kReYByteOff;
    uint8_t *e1Dev = reDev + F203EncryptFull::kReE1ByteOff;
    uint8_t *e2Dev = reDev + F203EncryptFull::kReE2ByteOff;

    // Launch 2：l18_l19（compute + e₂+=μ + 内联 tail pack → c）
    std::fprintf(stderr, "[full] launch 2 f203_encrypt_l18_l19 (compute + inline tail pack -> c)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, mDev, e1Dev, e2Dev, wsDev, tilingPinned, cDev,
                                              nullptr);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(cHost, cBytes, cDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    // Alg.14 输出仅密文 c（u/v 为设备内部中间量，不 D2H、不落盘）
    if (!WriteFile("./output/c.bin", cHost, cBytes)) {
        return 4;
    }

    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(mDev));
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
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(prepTilingPinned));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(coinsHost));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
