/**
 * @file main.cpp
 * @brief stable-fips203-mlkem-pke-encrypt-k4 — 完整 Encrypt host 编排（FIPS 203 Alg.14 行 1–22）。
 *
 * 流水线位置：ML-KEM-1024（k=4）K-PKE.Encrypt 的 **host 入口**；负责读 `input/`、
 * 分配 GM/workspace、按 CPU/SIM 路径 launch 设备核，并写出 **唯一产物** `output/c.bin`。
 *
 * 对齐 customspec：`stable-fips203-mlkem-pke-encrypt-k4-实现方案-customspec`（自 incubating exp 晋级）。
 *
 * 生产 I/O（强制，与 golden 对拍）：
 *   输入 `ek_pke` + `m` + `coins`（+ 静态 NTT/INTT LUT）→ 设备产 **仅** c = c₁‖c₂（1568B）。
 *   Â / re(y,e₁,e₂) / u / v 等仅设备 GM handoff，**默认不落盘**；
 *   例外：生产默认 Host 折 μ 时对 e₂ 切片做一次 D2H/H2D（非 Host 算 c）。
 *   golden：`scripts/gen_data.py` 生成 `golden/c.bin`；`run.sh` 对拍 `output/c.bin`。
 *
 * Launch：
 *   SIM  **2×** prep → l18_l19（内联 tail pack）——生产主路径
 *   CPU  **5×** prep + ntt_y/at_jp/intt_e1 + pack（v=`golden_v` 注入，非 Alg.14 产物）
 *
 * μ 折叠（SIM/NPU，2026-09-04 / D-next-pke-encrypt-hostmu）：
 *   默认 `F203_HOST_FOLD_MU=1`（未设亦开）：prep 后 Host 完成 e₂+=μ，向 l18 传 mGm=nullptr，
 *   跳过设备 PrefixEmbed（对齐 KEM Encaps；排查 l18 空 TRACE 挂死）。
 *   调试：`F203_HOST_FOLD_MU=0` → 设备 PrefixEmbed。
 *
 * handoff 布局：`f203_encrypt_full_layout.h`；compute tiling：`f203_encrypt_tiling.cpp`。
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
#include <cstdlib>
#include <cstring>
#include <cstdio>

// compute 运行时 tiling（模板风格，见 f203_encrypt_tiling.cpp）
extern void GenerateTiling(TilingData &data);

namespace {
/**
 * 生产默认开 Host 折 μ：未设 env 或显式 `=1` → true；仅 `F203_HOST_FOLD_MU=0` → false。
 * 背景：PKE Encrypt 实机挂在 l18；与 KEM Encaps TASK-006 同开关语义。
 */
bool HostFoldMuEnabled()
{
    const char *v = std::getenv("F203_HOST_FOLD_MU");
    if (v == nullptr || v[0] == '\0') {
        return true;
    }
    return v[0] == '1' && v[1] == '\0';
}

/**
 * Host 侧完成与设备 `PrefixEmbedMuIntoE2Gm` I/O 等价的 `e₂ += μ (mod q)`。
 * @param e2 长度 N=256 的 e₂ 系数缓冲（原地更新）
 * @param m  32B 消息
 * 前置：prep 已写出合法 e₂；调用方在 l18 launch 前 D2H/H2D。
 */
void HostFoldMuIntoE2InPlace(int32_t *e2, const uint8_t *m)
{
    constexpr int32_t kN = F203_TAIL_N;
    constexpr int32_t kQ = F203_TAIL_Q;
    constexpr int32_t kHalfQ = F203_TAIL_HALF_Q;
    for (int32_t c = 0; c < kN; ++c) {
        const int32_t byteIdx = c / 8;
        const int32_t bitIdx = c % 8;
        const int32_t bit = (static_cast<int32_t>(m[byteIdx]) >> bitIdx) & 1;
        const int32_t mu = (bit != 0) ? kHalfQ : 0;
        int32_t v = e2[c] + mu;
        v %= kQ;
        if (v < 0) {
            v += kQ;
        }
        e2[c] = v;
    }
}
}  // namespace

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
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
 * Host 主函数：按编译宏走 CPU 五 launch 或 SIM 两 launch，产出 `output/c.bin`（1568B）。
 * 输入：`input/{ek_pke,coins}`（SIM 另需 `m`）；LUT 与（仅 CPU）`golden_v`。
 * 返回：0 成功；非 0 为读文件/写文件失败码（与历史探针约定一致）。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t cBytes = F203_TAIL_C_BYTES;  // 密文长度：c₁‖c₂ = 1568B（ML-KEM-1024）

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

    // 默认 Host 折 μ：D2H e₂ → e₂+=μ → H2D；l18 传 mGm=nullptr 跳过 PrefixEmbed
    const bool hostFoldMu = HostFoldMuEnabled();
    constexpr size_t kE2Bytes = static_cast<size_t>(F203_TAIL_N) * sizeof(int32_t);
    int32_t *e2HostFold = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&e2HostFold), kE2Bytes));
    uint8_t *mForL18 = mDev;
    if (hostFoldMu) {
        CHECK_ACL(aclrtMemcpy(e2HostFold, kE2Bytes, e2Dev, kE2Bytes, ACL_MEMCPY_DEVICE_TO_HOST));
        HostFoldMuIntoE2InPlace(e2HostFold, mHost);
        CHECK_ACL(aclrtMemcpy(e2Dev, kE2Bytes, e2HostFold, kE2Bytes, ACL_MEMCPY_HOST_TO_DEVICE));
        mForL18 = nullptr;
        std::fprintf(stderr, "[full] F203_HOST_FOLD_MU=1：Host 已折 e2+=mu；l18 mGm=null\n");
    } else {
        std::fprintf(stderr, "[full] F203_HOST_FOLD_MU=0：设备 PrefixEmbed μ（调试）\n");
    }

    // Launch 2：l18_l19（compute + 内联 tail pack → c；μ 默认已在 Host）
    std::fprintf(stderr, "[full] launch 2 f203_encrypt_l18_l19 (compute + inline tail pack -> c)\n");
    ACLRT_LAUNCH_KERNEL(f203_encrypt_l18_l19)(1, stream, uDev, vDev, yDev, yHatDev, uNttDev, uTrDev, aHatDev, ekPkeDev,
                                              tHatDev, trHatNttDev, mForL18, e1Dev, e2Dev, wsDev, tilingPinned, cDev,
                                              nullptr);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(cHost, cBytes, cDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    // Alg.14 输出仅密文 c（u/v 为设备内部中间量，不 D2H、不落盘）
    if (!WriteFile("./output/c.bin", cHost, cBytes)) {
        return 4;
    }

    if (e2HostFold != nullptr) {
        CHECK_ACL(aclrtFreeHost(e2HostFold));
        e2HostFold = nullptr;
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
    // ResetDevice+Finalize 由 aclGuard 析构统一执行（含早退路径）
#endif
    return 0;
}
