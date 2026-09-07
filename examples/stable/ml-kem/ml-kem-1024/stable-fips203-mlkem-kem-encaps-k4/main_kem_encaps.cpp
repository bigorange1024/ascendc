/**
 * @file main_kem_encaps.cpp
 * @brief Alg.20/17 KEM Encaps host：vendored Encrypt SIM2/CPU5 + prep 前段设备 H/G。
 *
 * 流水线：本文件为交付入口；设备侧 `f203_kem_enc_prep`（头+prep）→ compute/pack → D2H。
 * 生产 I/O：input/ek_kem.bin + m.bin + LUT → output/c.bin + K.bin。
 * FIPS：$m$ 为 GM 输入；$r$ 由设备 G 写入 workspace，Host 禁止预填 $r$。
 * customspec：stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*
 * registry：docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md
 *
 * Launch 拓扑（锁定）：
 *   - SIM：2 次（prep → f203_encrypt_l18_l19 内联 pack）；仅 L2 后一次 SynchronizeStream
 *   - CPU：5 次（prep → ntt_y → at_jp → intt_e1 → pack）；v 用 golden_v 注入（辅助正确性）
 */
#include "ascendc_build_mode.hpp"
#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_compute_tail_layout.h"
#include "f203_encrypt_full_layout.h"
#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"
#include "f203_kem_enc_layout.h"
#include "f203_l18_l19_tiling.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// compute 运行时 tiling（模板风格，见 f203_encrypt_tiling.cpp）
extern void GenerateTiling(TilingData &data);

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
#include "aclrtlaunch_f203_encrypt_at_jp.h"
#include "aclrtlaunch_f203_encrypt_intt_e1.h"
#include "aclrtlaunch_f203_encrypt_l18_l19.h"
#include "aclrtlaunch_f203_encrypt_ntt_y.h"
#include "aclrtlaunch_f203_kem_enc_prep.h"
#else
#include "tikicpulib.h"
#include "alg11_gammas.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_kem_enc_prep(GM_ADDR ek_gm, GM_ADDR m_gm, GM_ADDR K_gm, GM_ADDR r_gm, GM_ADDR a_hat_gm,
                                  GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm);
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
constexpr size_t kKBytes = F203KemEnc::kSharedSecretBytes;
constexpr size_t kMBytes = F203KemEnc::kMsgBytes;

/** PRF(SHAKE256) tiling：与 prep 探针 FillPrfTiling 逐值一致（batch9、msgStride 64、outLen 128）。 */
void FillPrfTiling(ShakeGeneralTilingData *t)
{
    FillShakeTiling(t, 8U, 64U, kPrfBytesPerPoly, SHAKE256_RATE_BYTES);
    t->blockDim = 1U;
}
}  // namespace

/**
 * 将 NTT 正变换 LUT（even/odd planar-stacked）装入 workspace 的 LUT_NTT_* 段。
 * @param ws        host/device 可见的 workspace 基址
 * @param lutBytes  单份 even 或 odd LUT 字节数（与 tiling::lutEvenOddBytes 一致）
 * @return 两份文件均读成功为 true；失败时调用方应中止（返回码 20）
 * 前置：`input/lut_ntt_{even,odd}_stacked.bin` 已由 gen_data 生成。
 */
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
/**
 * CPU 分段路径：INTT LUT 覆盖写入同一 LUT_NTT_* 区（phased 复用，与探针 RunCpuThreeLaunch 一致）。
 * 原因：CPU 三 launch 串行，NTT 完成后才跑 INTT，可安全覆盖；SIM 融合核则用独立 LUT_INTT_* 段。
 */
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
/**
 * SIM 融合路径：INTT LUT 写入独立 LUT_INTT_* 段，与 NTT LUT 并存于同一 ws（单 launch 内两用）。
 */
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

/**
 * Host main：读生产输入、按 CPU/SIM 分叉 launch，写出 c.bin + K.bin。
 *
 * @return 0 成功；非 0 为文件/尺寸/ACL 失败码（与 Encrypt host 惯例对齐）
 * 输入：`./input/ek_kem.bin`（或 `ek_pke.bin` 别名）、`m.bin`，LUT；
 *       CPU 另需 `golden_v.bin`（tikicpu 无融合 INTT 时的 v 注入，非交付契约）。
 * 输出：`./output/c.bin`、`K.bin`。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t cBytes = F203_TAIL_C_BYTES;

    // compute 运行时 tiling：tileLength / mixPass 等 3 标量，供 NTT/INTT MIX 核使用
    TilingData tilingHost{};
    GenerateTiling(tilingHost);

    // prep 侧 PRF(SHAKE256) tiling：batch/msgStride/outLen 与 FillPrfTiling 锁定值一致
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
    uint8_t *mIn = (uint8_t *)AscendC::GmAlloc(kMBytes);
    uint8_t *kOut = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *rGm = (uint8_t *)AscendC::GmAlloc(kCoinsSize);
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
    // 优先 ek_kem.bin；兼容 ek_pke.bin 别名
    if ((!ReadFile("./input/ek_kem.bin", rs, ekPke, kEkBytes) || rs != kEkBytes) &&
        (!ReadFile("./input/ek_pke.bin", rs, ekPke, kEkBytes) || rs != kEkBytes)) {
        return 13;
    }
    if (!ReadFile("./input/m.bin", rs, mIn, kMBytes) || rs != kMBytes) {
        return 8;
    }
    std::memset(rGm, 0, kCoinsSize);  // r workspace；设备写出
    std::memset(kOut, 0, kKBytes);
    std::memcpy(prepTilingGm, &prepTilingHost, prepTilingBytes);

    // Launch 1：KEM 头 + prep → K、r、Â、(y‖e1‖e2)
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_kem_enc_prep, kPrepBlockDim, ekPke, mIn, kOut, rGm, aHat, prf, re, prepTilingGm);

    // re 切片：y（0）、e₁（4096）；compute 读同一 GM（y 由 PRF(r) 采样）
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

    // Alg.14 密文 + Alg.17 共享秘密
    if (!WriteFile("./output/c.bin", cOut, cBytes)) {
        return 4;
    }
    if (!WriteFile("./output/K.bin", kOut, kKBytes)) {
        return 5;
    }

    AscendC::GmFree(ekPke);
    AscendC::GmFree(mIn);
    AscendC::GmFree(kOut);
    AscendC::GmFree(rGm);
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
    uint8_t *mHost = nullptr;
    uint8_t *kHost = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *cHost = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&mHost), mBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&kHost), kKBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&cHost), cBytes));

    // ---- device arena ----
    uint8_t *ekPkeDev = nullptr;
    uint8_t *rDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *kDev = nullptr;
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
    uint8_t *tHatDev = nullptr;
    uint8_t *traceDev = nullptr;
    int32_t *traceHost = nullptr;
    constexpr int kFusedTraceStages = 16;
    const bool l18Trace = ascendc_acl::EnvFlagOn("F203_L18_TRACE");

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rDev), kCoinsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&kDev), kKBytes, ACL_MEM_MALLOC_HUGE_FIRST));
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
    if (l18Trace) {
        CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&traceDev),
                              static_cast<size_t>(kFusedTraceStages) * sizeof(int32_t), ACL_MEM_MALLOC_HUGE_FIRST));
        CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&traceHost),
                                  static_cast<size_t>(kFusedTraceStages) * sizeof(int32_t)));
        CHECK_ACL(aclrtMemset(traceDev, static_cast<size_t>(kFusedTraceStages) * sizeof(int32_t), 0,
                              static_cast<size_t>(kFusedTraceStages) * sizeof(int32_t)));
        std::fprintf(stderr, "[kem-enc] F203_L18_TRACE=1：l18_l19 将轮询 fused-trace 槽位\n");
    }

    size_t rs = 0;
    if ((!ReadFile("./input/ek_kem.bin", rs, ekPkeHost, kEkBytes) || rs != kEkBytes) &&
        (!ReadFile("./input/ek_pke.bin", rs, ekPkeHost, kEkBytes) || rs != kEkBytes)) {
        return 13;
    }
    if (!ReadFile("./input/m.bin", rs, mHost, mBytes) || rs != mBytes) {
        return 8;
    }

    CHECK_ACL(aclrtMemcpy(ekPkeDev, kEkBytes, ekPkeHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, mBytes, mHost, mBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemset(rDev, kCoinsSize, 0, kCoinsSize));
    CHECK_ACL(aclrtMemset(kDev, kKBytes, 0, kKBytes));
    CHECK_ACL(aclrtMemcpy(prepTilingDev, prepTilingBytes, prepTilingPinned, prepTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // Launch 1：KEM 头 + prep
    std::fprintf(stderr, "[kem-enc] launch 1 f203_kem_enc_prep (ek+m -> K,r,A-hat,y||e)\n");
    ACLRT_LAUNCH_KERNEL(f203_kem_enc_prep)(kPrepBlockDim, stream, ekPkeDev, mDev, kDev, rDev, aHatDev, prfDev,
                                           reDev, prepTilingDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_kem_enc_prep"));

    // LUT 装入 ws（NTT + INTT 段）后 H2D
    std::memset(wsHost, 0, wsSize);
    if (!LoadNttLutHost(wsHost, lutBytes) || !LoadInttLutHostFused(wsHost, lutBytes)) {
        return 20;
    }
    CHECK_ACL(aclrtMemcpy(wsDev, wsSize, wsHost, wsSize, ACL_MEMCPY_HOST_TO_DEVICE));

    // re 切片零拷贝：y（0）、e₁（4096）、e₂（8192）
    uint8_t *yDev = reDev + F203EncryptFull::kReYByteOff;
    uint8_t *e1Dev = reDev + F203EncryptFull::kReE1ByteOff;
    uint8_t *e2Dev = reDev + F203EncryptFull::kReE2ByteOff;

    // Launch 2：l18_l19（compute + e₂+=μ + 内联 tail pack → c）
    // 调试（非默认）：F203_L18_TRACE=1 时传 traceGm，卡住时 stderr 打印已到达的 FusedTraceStage 下标。
    std::fprintf(stderr, "[kem-enc] launch 2 f203_encrypt_l18_l19 (compute + inline pack -> c)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, mDev, e1Dev, e2Dev, wsDev, tilingPinned, cDev,
                                              traceDev);
    CHECK_ACL(ascendc_acl::TimedSynchronizeStreamMaybeTrace(stream, traceDev, traceHost, kFusedTraceStages,
                                                              "f203_encrypt_l18_l19"));

    CHECK_ACL(aclrtMemcpy(cHost, cBytes, cDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(kHost, kKBytes, kDev, kKBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/c.bin", cHost, cBytes)) {
        return 4;
    }
    if (!WriteFile("./output/K.bin", kHost, kKBytes)) {
        return 5;
    }

    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(rDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(kDev));
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
    if (traceDev != nullptr) {
        CHECK_ACL(aclrtFree(traceDev));
    }
    if (traceHost != nullptr) {
        CHECK_ACL(aclrtFreeHost(traceHost));
    }
    CHECK_ACL(aclrtFreeHost(tilingPinned));
    CHECK_ACL(aclrtFreeHost(prepTilingPinned));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(kHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    // ResetDevice+Finalize 由 aclGuard 析构统一执行（含早退路径）
#endif
    return 0;
}
