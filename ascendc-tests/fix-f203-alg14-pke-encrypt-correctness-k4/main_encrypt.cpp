/**
 * @file main_encrypt.cpp
 * @brief Alg.14 Encrypt 探针 Host 入口（G1–G4 全链至 c.bin）。
 *
 * 生产 I/O：
 *   input/  — ek_pke.bin (1568B) + m.bin (32B) + coins.bin (32B) + NTT/INTT LUT
 *   output/ — c.bin (1568B)；G1–G3 中间张量可选落盘
 *
 * ENCRYPT_GATE=0：仅 marker 壳；>=4 写真实 c.bin（INTT+噪声+pack）。
 */
#include "data_utils.h"
#include "f203_encrypt_g5_run.hpp"
#include "f203_encrypt_layout.h"
#include "f203_ntt_r_tiling.h"
#include "f203_encrypt_intt_tiling.h"
#include "f203_encrypt_pack_config.hpp"
#include "innerproduct_tiling.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_marker_custom.h"
#include "aclrtlaunch_f203_encrypt_prep_a_hat.h"
#include "aclrtlaunch_f203_encrypt_prep_re.h"
#include "aclrtlaunch_f203_encrypt_ntt_r.h"
#include "aclrtlaunch_f203_encrypt_g3_linear.h"
#include "aclrtlaunch_f203_encrypt_at_r.h"
#include "aclrtlaunch_f203_encrypt_t_dot_r.h"
#include "aclrtlaunch_f203_encrypt_intt.h"
#include "aclrtlaunch_f203_encrypt_g4_noise.h"
#include "aclrtlaunch_f203_encrypt_pack.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_encrypt_marker_custom(GM_ADDR marker_out, int32_t marker_val);
extern "C" __global__ __aicore__ void f203_encrypt_prep_a_hat(GM_ADDR rho_gm, GM_ADDR a_hat_gm);
extern "C" __global__ __aicore__ void f203_encrypt_prep_re(GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                         GM_ADDR tiling);
extern "C" __global__ __aicore__ void f203_encrypt_ntt_r(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_g3_linear(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uHat,
                                                             GM_ADDR trHat);
extern "C" __global__ __aicore__ void f203_encrypt_at_r(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR uHat);
extern "C" __global__ __aicore__ void f203_encrypt_t_dot_r(GM_ADDR tHat, GM_ADDR rHat, GM_ADDR trHat);
extern "C" __global__ __aicore__ void f203_encrypt_intt(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_g4_noise(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm,
                                                          GM_ADDR mGm, GM_ADDR vGm);
extern "C" __global__ __aicore__ void f203_encrypt_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm);
extern volatile int g_f203_ntt_r_mix_pass;
extern volatile int g_f203_intt_mix_pass;
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kAhatBlockDim = 2U;
constexpr uint32_t kReBlockDim = 1U;
constexpr uint32_t kNttBlockDim = 1U;
constexpr uint32_t kAtRBlockDim = 1U;
constexpr uint32_t kTDotRBlockDim = 1U;
constexpr uint32_t kNttMixPass = 3U;
constexpr uint32_t kInttBlockDim = 1U;
constexpr uint32_t kInttMixPass = 3U;
constexpr uint32_t kG4NoiseBlockDim = 1U;
constexpr uint32_t kPackBlockDim = 1U;
constexpr uint32_t kPrfBatch = F203_ENCRYPT_PRF_BATCH;
constexpr uint32_t kPrfMaxMsgLen = 64U;
constexpr uint32_t kPrfOutLen = F203_ENCRYPT_PRF_BYTES_PER_POLY;
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);

int launch_marker(uint32_t blockDim)
{
    constexpr size_t markerBytes = sizeof(int32_t);
#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *marker = (uint8_t *)AscendC::GmAlloc(markerBytes);
    ICPU_RUN_KF(f203_encrypt_marker_custom, blockDim, marker, static_cast<int32_t>(0));
    AscendC::GmFree(marker);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));
    uint8_t *dev = nullptr;
    CHECK_ACL(aclrtMalloc((void **)&dev, markerBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    ACLRT_LAUNCH_KERNEL(f203_encrypt_marker_custom)(blockDim, stream, dev, static_cast<int32_t>(0));
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtFree(dev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

/** G1 Launch-1：ρ → a_hat[16,256]。 */
template <typename LaunchFn>
int run_prep_a_hat(LaunchFn &&launch, const uint8_t *rho, uint8_t *a_hat_out)
{
#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *rhoGm = (uint8_t *)AscendC::GmAlloc(F203_EK_RHO_BYTES);
    uint8_t *aHatGm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    std::memcpy(rhoGm, rho, F203_EK_RHO_BYTES);
    launch(kAhatBlockDim, rhoGm, aHatGm);
    std::memcpy(a_hat_out, aHatGm, F203_AHAT_BYTES);
    AscendC::GmFree(rhoGm);
    AscendC::GmFree(aHatGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *rhoHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *rhoDev = nullptr;
    uint8_t *aHatDev = nullptr;
    CHECK_ACL(aclrtMallocHost((void **)&rhoHost, F203_EK_RHO_BYTES));
    CHECK_ACL(aclrtMallocHost((void **)&aHatHost, F203_AHAT_BYTES));
    CHECK_ACL(aclrtMalloc((void **)&rhoDev, F203_EK_RHO_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(rhoHost, rho, F203_EK_RHO_BYTES);
    CHECK_ACL(aclrtMemcpy(rhoDev, F203_EK_RHO_BYTES, rhoHost, F203_EK_RHO_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kAhatBlockDim, stream, rhoDev, aHatDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(aHatHost, F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(a_hat_out, aHatHost, F203_AHAT_BYTES);
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

/** G1 Launch-2：coins → r/e1/e2 扁平 re GM。 */
template <typename LaunchFn>
int run_prep_re(LaunchFn &&launch, const uint8_t *coins, uint8_t *re_out)
{
    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kReBlockDim;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *coinsGm = (uint8_t *)AscendC::GmAlloc(F203_ENC_COINS_BYTES);
    uint8_t *prfGm = (uint8_t *)AscendC::GmAlloc(F203_ENCRYPT_PRF_TOTAL_BYTES);
    uint8_t *reGm = (uint8_t *)AscendC::GmAlloc(F203_RE_TOTAL_BYTES);
    uint8_t *tilingGm = (uint8_t *)AscendC::GmAlloc(kTilingBytes);
    std::memcpy(coinsGm, coins, F203_ENC_COINS_BYTES);
    std::memcpy(tilingGm, &tilingHost, kTilingBytes);
    launch(kReBlockDim, coinsGm, prfGm, reGm, tilingGm);
    std::memcpy(re_out, reGm, F203_RE_TOTAL_BYTES);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(tilingGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *coinsHost = nullptr;
    uint8_t *reHost = nullptr;
    uint8_t *tilingHostBuf = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *tilingDev = nullptr;
    CHECK_ACL(aclrtMallocHost((void **)&coinsHost, F203_ENC_COINS_BYTES));
    CHECK_ACL(aclrtMallocHost((void **)&reHost, F203_RE_TOTAL_BYTES));
    CHECK_ACL(aclrtMallocHost((void **)&tilingHostBuf, kTilingBytes));
    CHECK_ACL(aclrtMalloc((void **)&coinsDev, F203_ENC_COINS_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, F203_ENCRYPT_PRF_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&reDev, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&tilingDev, kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(coinsHost, coins, F203_ENC_COINS_BYTES);
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);
    CHECK_ACL(aclrtMemcpy(coinsDev, F203_ENC_COINS_BYTES, coinsHost, F203_ENC_COINS_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kReBlockDim, stream, coinsDev, prfDev, reDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(reHost, F203_RE_TOTAL_BYTES, reDev, F203_RE_TOTAL_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(re_out, reHost, F203_RE_TOTAL_BYTES);
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(coinsHost));
    CHECK_ACL(aclrtFreeHost(reHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

/** G2 Launch-3：r[4,256] → r_hat NTT（mixPass=3 S1+S2+S3）。 */
template <typename LaunchFn>
int run_ntt_r(LaunchFn &&launch, const uint8_t *r_in, const uint8_t *lut_even, const uint8_t *lut_odd,
              uint8_t *r_hat_out)
{
    using namespace tiling;
    TilingData tilingHost{};
    tilingHost.tileLength = static_cast<int32_t>(n);
    tilingHost.kPolys = static_cast<int32_t>(kK);
    tilingHost.mixPass = static_cast<int32_t>(kNttMixPass);

#ifdef ASCENDC_CPU_DEBUG
    g_f203_ntt_r_mix_pass = static_cast<int>(kNttMixPass);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *srcGm = (uint8_t *)AscendC::GmAlloc(srcFileBytes);
    uint8_t *dstGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    std::memset(wsGm, 0, wssize);
    std::memcpy(srcGm, r_in, srcFileBytes);
    std::memcpy(wsGm + LUT_EVEN_STACKED, lut_even, lutEvenOddFileBytes);
    std::memcpy(wsGm + LUT_ODD_STACKED, lut_odd, lutEvenOddFileBytes);
    launch(kNttBlockDim, dstGm, srcGm, wsGm, tilingHost);
    std::memcpy(r_hat_out, dstGm, dstFileBytes);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(dstGm);
    AscendC::GmFree(wsGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData tilingDevHost{};
    tilingDevHost.tileLength = tilingHost.tileLength;
    tilingDevHost.kPolys = tilingHost.kPolys;
    tilingDevHost.mixPass = tilingHost.mixPass;

    uint8_t *srcHost = nullptr;
    uint8_t *dstHost = nullptr;
    uint8_t *wsHost = nullptr;
    TilingData *tilingHostPtr = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *dstDev = nullptr;
    uint8_t *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost((void **)&srcHost, srcFileBytes));
    CHECK_ACL(aclrtMallocHost((void **)&dstHost, dstFileBytes));
    CHECK_ACL(aclrtMallocHost((void **)&wsHost, wssize));
    CHECK_ACL(aclrtMallocHost((void **)&tilingHostPtr, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&srcDev, srcFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dstDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(srcHost, r_in, srcFileBytes);
    std::memset(wsHost, 0, wssize);
    std::memcpy(wsHost + LUT_EVEN_STACKED, lut_even, lutEvenOddFileBytes);
    std::memcpy(wsHost + LUT_ODD_STACKED, lut_odd, lutEvenOddFileBytes);
    *tilingHostPtr = tilingDevHost;
    CHECK_ACL(aclrtMemcpy(srcDev, srcFileBytes, srcHost, srcFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost + LUT_ODD_STACKED, lutEvenOddFileBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kNttBlockDim, stream, dstDev, srcDev, wsDev, tilingHostPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileBytes, dstDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(r_hat_out, dstHost, dstFileBytes);
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(dstDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tilingHostPtr));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

#ifndef ASCENDC_CPU_DEBUG
/** SIM/NPU：独立 ACL session 单次 at_r（Âᵀ·r̂ → u_hat）。 */
int run_g3_at_r_device_once(const uint8_t *a_in, const uint8_t *r_in, uint8_t *u_out)
{
    const size_t aBytes = static_cast<size_t>(innerproduct_tiling::kAHatBytes);
    const size_t rBytes = static_cast<size_t>(innerproduct_tiling::kSHatBytes);
    const size_t uBytes = static_cast<size_t>(innerproduct_tiling::kTHatBytes);

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *aHost = nullptr;
    uint8_t *rHost = nullptr;
    uint8_t *uHost = nullptr;
    uint8_t *aDev = nullptr;
    uint8_t *rDev = nullptr;
    uint8_t *uDev = nullptr;
    CHECK_ACL(aclrtMallocHost((void **)&aHost, aBytes));
    CHECK_ACL(aclrtMallocHost((void **)&rHost, rBytes));
    CHECK_ACL(aclrtMallocHost((void **)&uHost, uBytes));
    CHECK_ACL(aclrtMalloc((void **)&aDev, aBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rDev, rBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, uBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(aHost, a_in, aBytes);
    std::memcpy(rHost, r_in, rBytes);
    CHECK_ACL(aclrtMemcpy(aDev, aBytes, aHost, aBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rDev, rBytes, rHost, rBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    const uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r)(kAtRBlockDim, stream, aDev, rDev, uDev);
    if (ret != 0) {
        std::fprintf(stderr, "[main_encrypt] G3 at_r launch failed ret=%u\n", ret);
        CHECK_ACL(aclrtDestroyStream(stream));
        CHECK_ACL(aclrtResetDevice(deviceId));
        CHECK_ACL(aclFinalize());
        return 12;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(uHost, uBytes, uDev, uBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(u_out, uHost, uBytes);
    CHECK_ACL(aclrtFree(aDev));
    CHECK_ACL(aclrtFree(rDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFreeHost(aHost));
    CHECK_ACL(aclrtFreeHost(rHost));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
}

/**
 * t̂[j] 填入 Â 列 p=0：flat (j*K+0)*N，供 at_r 算 row0 = t̂·r̂。
 * 背景：SIM 上 f203_encrypt_t_dot_r launch 507000；与 t_dot_r 核数学等价（见 G3_SIM_AUDIT.md §8）。
 */
static void pack_t_hat_as_at_r_col0(const uint8_t *t_hat, uint8_t *a_hat_col0)
{
    const size_t polyBytes = static_cast<size_t>(F203_ENCRYPT_N * sizeof(int32_t));
    std::memset(a_hat_col0, 0, F203_AHAT_BYTES);
    for (uint32_t j = 0; j < F203_ENCRYPT_K; ++j) {
        const size_t dstOff = (static_cast<size_t>(j) * F203_ENCRYPT_K) * polyBytes;
        std::memcpy(a_hat_col0 + dstOff, t_hat + j * polyBytes, polyBytes);
    }
}

/** SIM：at_r + 仅 row0 → tr_hat（t̂ 列 0 编码，非 u_hat 路径 workaround）。 */
int run_g3_tr_via_at_r_device_once(const uint8_t *t_in, const uint8_t *r_in, uint8_t *tr_out)
{
    std::vector<uint8_t> aCol0(F203_AHAT_BYTES);
    pack_t_hat_as_at_r_col0(t_in, aCol0.data());
    std::vector<uint8_t> uBuf(F203_U_HAT_BYTES);
    if (run_g3_at_r_device_once(aCol0.data(), r_in, uBuf.data()) != 0) {
        return 13;
    }
    std::memcpy(tr_out, uBuf.data(), F203_TR_HAT_BYTES);
    return 0;
}

/**
 * SIM G3：两次独立 ACL session — at_r(真 a_hat)→u_hat；at_r(t̂ 列 0)→tr_hat。
 * t_dot_r / g3_linear 五参 launch 在 SIM 均 507000（§7–§8）。
 */
int run_g3_device_sim_once(const uint8_t *a_in, const uint8_t *r_in, const uint8_t *t_in, uint8_t *u_out,
                           uint8_t *tr_out)
{
    if (run_g3_at_r_device_once(a_in, r_in, u_out) != 0) {
        return 12;
    }
    if (run_g3_tr_via_at_r_device_once(t_in, r_in, tr_out) != 0) {
        return 13;
    }
    return 0;
}
#endif

/** G4 INTT：NTT 域 src [k,256] → 时域 dst（INTT LUT 在 ws 前缀）。 */
template <typename LaunchFn>
int run_intt(LaunchFn &&launch, const uint8_t *src_in, const uint8_t *lut_even, const uint8_t *lut_odd,
             uint8_t *dst_out)
{
    using namespace tiling;
    TilingData tilingHost{};
    tilingHost.tileLength = static_cast<int32_t>(n);
    tilingHost.kPolys = static_cast<int32_t>(kK);
    tilingHost.mixPass = static_cast<int32_t>(kInttMixPass);

#ifdef ASCENDC_CPU_DEBUG
    g_f203_intt_mix_pass = static_cast<int>(kInttMixPass);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *srcGm = (uint8_t *)AscendC::GmAlloc(srcFileBytes);
    uint8_t *dstGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    std::memset(wsGm, 0, wssize);
    std::memcpy(srcGm, src_in, srcFileBytes);
    std::memcpy(wsGm + LUT_EVEN_STACKED, lut_even, lutEvenOddFileBytes);
    std::memcpy(wsGm + LUT_ODD_STACKED, lut_odd, lutEvenOddFileBytes);
    launch(kInttBlockDim, dstGm, srcGm, wsGm, tilingHost);
    std::memcpy(dst_out, dstGm, dstFileBytes);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(dstGm);
    AscendC::GmFree(wsGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData tilingDevHost{};
    tilingDevHost.tileLength = tilingHost.tileLength;
    tilingDevHost.kPolys = tilingHost.kPolys;
    tilingDevHost.mixPass = tilingHost.mixPass;

    uint8_t *srcHost = nullptr;
    uint8_t *dstHost = nullptr;
    uint8_t *wsHost = nullptr;
    TilingData *tilingHostPtr = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *dstDev = nullptr;
    uint8_t *wsDev = nullptr;
    CHECK_ACL(aclrtMallocHost((void **)&srcHost, srcFileBytes));
    CHECK_ACL(aclrtMallocHost((void **)&dstHost, dstFileBytes));
    CHECK_ACL(aclrtMallocHost((void **)&wsHost, wssize));
    CHECK_ACL(aclrtMallocHost((void **)&tilingHostPtr, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&srcDev, srcFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dstDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(srcHost, src_in, srcFileBytes);
    std::memset(wsHost, 0, wssize);
    std::memcpy(wsHost + LUT_EVEN_STACKED, lut_even, lutEvenOddFileBytes);
    std::memcpy(wsHost + LUT_ODD_STACKED, lut_odd, lutEvenOddFileBytes);
    *tilingHostPtr = tilingDevHost;
    CHECK_ACL(aclrtMemcpy(srcDev, srcFileBytes, srcHost, srcFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost + LUT_ODD_STACKED, lutEvenOddFileBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kInttBlockDim, stream, dstDev, srcDev, wsDev, tilingHostPtr);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileBytes, dstDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(dst_out, dstHost, dstFileBytes);
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(dstDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tilingHostPtr));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

template <typename LaunchFn>
int run_g4_noise(LaunchFn &&launch, uint8_t *u, const uint8_t *e1, const uint8_t *tr, const uint8_t *e2,
                 const uint8_t *m, uint8_t *v)
{
#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(F203_U_HAT_BYTES);
    uint8_t *e1Gm = (uint8_t *)AscendC::GmAlloc(F203_E1_POLYVEC_BYTES);
    uint8_t *trGm = (uint8_t *)AscendC::GmAlloc(F203_TR_HAT_BYTES);
    uint8_t *e2Gm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    std::memcpy(uGm, u, F203_U_HAT_BYTES);
    std::memcpy(e1Gm, e1, F203_E1_POLYVEC_BYTES);
    std::memcpy(trGm, tr, F203_TR_HAT_BYTES);
    std::memcpy(e2Gm, e2, F203_E2_POLY_BYTES);
    std::memcpy(mGm, m, F203_MSG_BYTES);
    launch(kG4NoiseBlockDim, uGm, e1Gm, trGm, e2Gm, mGm, vGm);
    std::memcpy(u, uGm, F203_U_HAT_BYTES);
    std::memcpy(v, vGm, F203_E2_POLY_BYTES);
    AscendC::GmFree(uGm);
    AscendC::GmFree(e1Gm);
    AscendC::GmFree(trGm);
    AscendC::GmFree(e2Gm);
    AscendC::GmFree(mGm);
    AscendC::GmFree(vGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));
    uint8_t *uDev = nullptr;
    uint8_t *e1Dev = nullptr;
    uint8_t *trDev = nullptr;
    uint8_t *e2Dev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *uHost = nullptr;
    uint8_t *vHost = nullptr;
    CHECK_ACL(aclrtMalloc((void **)&uDev, F203_U_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e1Dev, F203_E1_POLYVEC_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trDev, F203_TR_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e2Dev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&uHost, F203_U_HAT_BYTES));
    CHECK_ACL(aclrtMallocHost((void **)&vHost, F203_E2_POLY_BYTES));
    std::memcpy(uHost, u, F203_U_HAT_BYTES);
    CHECK_ACL(aclrtMemcpy(uDev, F203_U_HAT_BYTES, uHost, F203_U_HAT_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e1Dev, F203_E1_POLYVEC_BYTES, e1, F203_E1_POLYVEC_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(trDev, F203_TR_HAT_BYTES, tr, F203_TR_HAT_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(e2Dev, F203_E2_POLY_BYTES, e2, F203_E2_POLY_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, F203_MSG_BYTES, m, F203_MSG_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kG4NoiseBlockDim, stream, uDev, e1Dev, trDev, e2Dev, mDev, vDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(uHost, F203_U_HAT_BYTES, uDev, F203_U_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(vHost, F203_E2_POLY_BYTES, vDev, F203_E2_POLY_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(u, uHost, F203_U_HAT_BYTES);
    std::memcpy(v, vHost, F203_E2_POLY_BYTES);
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(e1Dev));
    CHECK_ACL(aclrtFree(trDev));
    CHECK_ACL(aclrtFree(e2Dev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

template <typename LaunchFn>
int run_pack(LaunchFn &&launch, const uint8_t *u, const uint8_t *v, uint8_t *c_out)
{
#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(F203_U_HAT_BYTES);
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    std::memcpy(uGm, u, F203_U_HAT_BYTES);
    std::memcpy(vGm, v, F203_E2_POLY_BYTES);
    launch(kPackBlockDim, uGm, vGm, cGm);
    std::memcpy(c_out, cGm, F203_CT_PKE_BYTES);
    AscendC::GmFree(uGm);
    AscendC::GmFree(vGm);
    AscendC::GmFree(cGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *cDev = nullptr;
    uint8_t *cHost = nullptr;
    CHECK_ACL(aclrtMalloc((void **)&uDev, F203_U_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&cHost, F203_CT_PKE_BYTES));
    CHECK_ACL(aclrtMemcpy(uDev, F203_U_HAT_BYTES, u, F203_U_HAT_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDev, F203_E2_POLY_BYTES, v, F203_E2_POLY_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    launch(kPackBlockDim, stream, uDev, vDev, cDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(cHost, F203_CT_PKE_BYTES, cDev, F203_CT_PKE_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(c_out, cHost, F203_CT_PKE_BYTES);
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}

int write_placeholder_ct(const std::string &path)
{
    std::vector<uint8_t> zeros(F203_CT_PKE_BYTES, 0);
    if (!WriteFile(path, zeros.data(), zeros.size())) {
        std::fprintf(stderr, "[main_encrypt] write placeholder c.bin failed\n");
        return -1;
    }
    return 0;
}

int write_g1_outputs(const std::string &out_dir, const uint8_t *a_hat, const uint8_t *re_flat)
{
    if (!WriteFile(out_dir + "/a_hat.bin", a_hat, F203_AHAT_BYTES)) {
        return -1;
    }
    if (!WriteFile(out_dir + "/r.bin", re_flat, F203_R_POLYVEC_BYTES)) {
        return -2;
    }
    if (!WriteFile(out_dir + "/e1.bin", re_flat + F203_R_POLYVEC_BYTES, F203_E1_POLYVEC_BYTES)) {
        return -3;
    }
    if (!WriteFile(out_dir + "/e2.bin", re_flat + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES, F203_E2_POLY_BYTES)) {
        return -4;
    }
    return 0;
}

}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const std::string case_dir = ".";
    const char *gate_env = std::getenv("ENCRYPT_GATE");
    const int encrypt_gate = gate_env ? std::atoi(gate_env) : 5;

    std::vector<uint8_t> ek_buf(F203_EK_PKE_BYTES);
    std::vector<uint8_t> coins_buf(F203_ENC_COINS_BYTES);
    std::vector<uint8_t> a_hat(F203_AHAT_BYTES);
    std::vector<uint8_t> re_flat(F203_RE_TOTAL_BYTES);
    std::vector<uint8_t> r_hat(F203_R_HAT_BYTES);
    std::vector<uint8_t> t_hat(F203_T_HAT_BYTES);
    std::vector<uint8_t> u_hat(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_hat(F203_TR_HAT_BYTES);
    bool g5_sim_prelude = false;
    size_t rs = 0;
    if (!ReadFile(case_dir + "/input/ek_pke.bin", rs, ek_buf.data(), ek_buf.size()) || rs != F203_EK_PKE_BYTES) {
        std::fprintf(stderr, "[main_encrypt] bad ek_pke.bin size=%zu\n", rs);
        return 1;
    }
    if (!ReadFile(case_dir + "/input/coins.bin", rs, coins_buf.data(), coins_buf.size()) ||
        rs != F203_ENC_COINS_BYTES) {
        std::fprintf(stderr, "[main_encrypt] bad coins.bin size=%zu\n", rs);
        return 1;
    }

    if (encrypt_gate >= 5) {
        std::vector<uint8_t> lut_even(tiling::lutEvenOddFileBytes);
        std::vector<uint8_t> lut_odd(tiling::lutEvenOddFileBytes);
        std::vector<uint8_t> lut_intt_even(tiling::lutEvenOddFileBytes);
        std::vector<uint8_t> lut_intt_odd(tiling::lutEvenOddFileBytes);
        std::vector<uint8_t> m_buf(F203_MSG_BYTES);
        size_t lut_rs = 0;
        if (!ReadFile(case_dir + "/input/lut_even_stacked.bin", lut_rs, lut_even.data(), lut_even.size()) ||
            lut_rs != lut_even.size()) {
            std::fprintf(stderr, "[main_encrypt] bad lut_even_stacked.bin size=%zu\n", lut_rs);
            return 7;
        }
        if (!ReadFile(case_dir + "/input/lut_odd_stacked.bin", lut_rs, lut_odd.data(), lut_odd.size()) ||
            lut_rs != lut_odd.size()) {
            std::fprintf(stderr, "[main_encrypt] bad lut_odd_stacked.bin size=%zu\n", lut_rs);
            return 8;
        }
        if (!ReadFile(case_dir + "/input/lut_intt_even_stacked.bin", rs, lut_intt_even.data(), lut_intt_even.size()) ||
            rs != lut_intt_even.size()) {
            std::fprintf(stderr, "[main_encrypt] bad lut_intt_even_stacked.bin size=%zu\n", rs);
            return 16;
        }
        if (!ReadFile(case_dir + "/input/lut_intt_odd_stacked.bin", rs, lut_intt_odd.data(), lut_intt_odd.size()) ||
            rs != lut_intt_odd.size()) {
            std::fprintf(stderr, "[main_encrypt] bad lut_intt_odd_stacked.bin size=%zu\n", rs);
            return 17;
        }
        if (!ReadFile(case_dir + "/input/m.bin", rs, m_buf.data(), m_buf.size()) || rs != m_buf.size()) {
            std::fprintf(stderr, "[main_encrypt] bad m.bin size=%zu\n", rs);
            return 18;
        }
#ifdef ASCENDC_CPU_DEBUG
        std::vector<uint8_t> c_out(F203_CT_PKE_BYTES);
        const int g5rc = run_encrypt_g5_cpu_full(case_dir, ek_buf.data(), coins_buf.data(), m_buf.data(),
                                                 lut_even.data(), lut_odd.data(), lut_intt_even.data(),
                                                 lut_intt_odd.data(), c_out.data());
        return g5rc != 0 ? g5rc : 0;
#else
        std::vector<uint8_t> c_out(F203_CT_PKE_BYTES);
        // tail-only 快跑：跳过 prep/NTT/G3，读 golden u_hat/tr_hat/e1/e2 只跑 INTT→noise→pack，
        // 始终分阶段 dump，供逐步定位 SIM tail（INTT/noise/pack）首个对不上的阶段。
        const char *tailEnv = std::getenv("ENCRYPT_TAIL_ONLY");
        if (tailEnv != nullptr && std::atoi(tailEnv) != 0) {
            const int trc = run_encrypt_g4_tail_only_sim(case_dir, lut_intt_even.data(), lut_intt_odd.data(),
                                                         c_out.data());
            return trc != 0 ? trc : 0;
        }
        const int g5rc = run_encrypt_g5_sim_full(case_dir, ek_buf.data(), coins_buf.data(), m_buf.data(),
                                               lut_even.data(), lut_odd.data(), lut_intt_even.data(),
                                               lut_intt_odd.data(), c_out.data());
        return g5rc != 0 ? g5rc : 0;
#endif
    }

    if (encrypt_gate < 4 && !g5_sim_prelude && write_placeholder_ct(case_dir + "/output/c.bin") != 0) {
        return 2;
    }

    if (!g5_sim_prelude) {
        if (encrypt_gate < 1) {
            if (launch_marker(1) != 0) {
                return 3;
            }
            std::printf("[main_encrypt] G0 marker only ENCRYPT_GATE=%d\n", encrypt_gate);
            return 0;
        }

        const uint8_t *rho = ek_buf.data() + F203_EK_RHO_OFFSET;

        std::printf("[main_encrypt] G1 prep ENCRYPT_GATE=%d blockDim_a_hat=%u blockDim_re=%u\n", encrypt_gate,
                    kAhatBlockDim, kReBlockDim);

#ifdef ASCENDC_CPU_DEBUG
    auto launch_a_hat = [](uint32_t blockDim, uint8_t *rhoGm, uint8_t *aHatGm) {
        ICPU_RUN_KF(f203_encrypt_prep_a_hat, blockDim, rhoGm, aHatGm);
    };
    auto launch_re = [](uint32_t blockDim, uint8_t *coinsGm, uint8_t *prfGm, uint8_t *reGm, uint8_t *tilingGm) {
        (void)blockDim;
        ICPU_RUN_KF(f203_encrypt_prep_re, 1, coinsGm, prfGm, reGm, tilingGm);
    };
    if (run_prep_a_hat(launch_a_hat, rho, a_hat.data()) != 0) {
        return 4;
    }
    if (run_prep_re(launch_re, coins_buf.data(), re_flat.data()) != 0) {
        return 5;
    }
#else
    if (run_prep_a_hat(
            [](uint32_t blockDim, aclrtStream stream, uint8_t *rhoDev, uint8_t *aHatDev) {
                ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_a_hat)(blockDim, stream, rhoDev, aHatDev);
            },
            rho, a_hat.data()) != 0) {
        return 4;
    }
    if (run_prep_re(
            [](uint32_t blockDim, aclrtStream stream, uint8_t *coinsDev, uint8_t *prfDev, uint8_t *reDev,
               uint8_t *tilingDev) {
                ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_re)(blockDim, stream, coinsDev, prfDev, reDev, tilingDev);
            },
            coins_buf.data(), re_flat.data()) != 0) {
        return 5;
    }
#endif

    const int wo = write_g1_outputs(case_dir + "/output", a_hat.data(), re_flat.data());
    if (wo != 0) {
        std::fprintf(stderr, "[main_encrypt] write G1 outputs failed code=%d\n", wo);
        return 6;
    }
    std::printf("[main_encrypt] G1 prep done a_hat=%uB re=%uB\n", F203_AHAT_BYTES, F203_RE_TOTAL_BYTES);

    if (encrypt_gate < 2) {
        return 0;
    }

    std::vector<uint8_t> lut_even(tiling::lutEvenOddFileBytes);
    std::vector<uint8_t> lut_odd(tiling::lutEvenOddFileBytes);
    size_t lut_rs = 0;
    if (!ReadFile(case_dir + "/input/lut_even_stacked.bin", lut_rs, lut_even.data(), lut_even.size()) ||
        lut_rs != lut_even.size()) {
        std::fprintf(stderr, "[main_encrypt] bad lut_even_stacked.bin size=%zu\n", lut_rs);
        return 7;
    }
    if (!ReadFile(case_dir + "/input/lut_odd_stacked.bin", lut_rs, lut_odd.data(), lut_odd.size()) ||
        lut_rs != lut_odd.size()) {
        std::fprintf(stderr, "[main_encrypt] bad lut_odd_stacked.bin size=%zu\n", lut_rs);
        return 8;
    }

    std::printf("[main_encrypt] G2 NTT r ENCRYPT_GATE=%d blockDim_ntt=%u mixPass=%u\n", encrypt_gate, kNttBlockDim,
                kNttMixPass);

#ifdef ASCENDC_CPU_DEBUG
    auto launch_ntt = [](uint32_t blockDim, uint8_t *dstGm, uint8_t *srcGm, uint8_t *wsGm, TilingData tilingHost) {
        ICPU_RUN_KF(f203_encrypt_ntt_r, blockDim, dstGm, srcGm, wsGm, tilingHost);
    };
#else
    auto launch_ntt = [](uint32_t blockDim, aclrtStream stream, uint8_t *dstDev, uint8_t *srcDev, uint8_t *wsDev,
                         TilingData *tilingHostPtr) {
        ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_r)(blockDim, stream, dstDev, srcDev, wsDev, tilingHostPtr);
    };
#endif

    if (run_ntt_r(launch_ntt, re_flat.data(), lut_even.data(), lut_odd.data(), r_hat.data()) != 0) {
        return 9;
    }
    if (!WriteFile(case_dir + "/output/r_hat.bin", r_hat.data(), r_hat.size())) {
        std::fprintf(stderr, "[main_encrypt] write r_hat.bin failed\n");
        return 10;
    }
    std::printf("[main_encrypt] G2 NTT r_hat done %uB\n", F203_R_HAT_BYTES);

    if (encrypt_gate < 3) {
        return 0;
    }

    if (!ReadFile(case_dir + "/input/t_hat.bin", rs, t_hat.data(), t_hat.size()) || rs != t_hat.size()) {
        std::fprintf(stderr, "[main_encrypt] bad t_hat.bin size=%zu\n", rs);
        return 11;
    }

    std::printf("[main_encrypt] G3 linear ENCRYPT_GATE=%d blockDim=%u (cpu=g3_linear sim=at_r+at_r_col0)\n",
                encrypt_gate, kAtRBlockDim);

#ifdef ASCENDC_CPU_DEBUG
    const size_t uBytes = static_cast<size_t>(innerproduct_tiling::kTHatBytes);
    const size_t trBytes = static_cast<size_t>(F203_TR_HAT_BYTES);
    const size_t aBytes = static_cast<size_t>(innerproduct_tiling::kAHatBytes);
    const size_t rBytes = static_cast<size_t>(innerproduct_tiling::kSHatBytes);
    const size_t tBytes = static_cast<size_t>(F203_T_HAT_BYTES);
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *aGm = (uint8_t *)AscendC::GmAlloc(aBytes);
    uint8_t *tGm = (uint8_t *)AscendC::GmAlloc(tBytes);
    uint8_t *rGm = (uint8_t *)AscendC::GmAlloc(rBytes);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(uBytes);
    uint8_t *trGm = (uint8_t *)AscendC::GmAlloc(trBytes);
    std::memcpy(aGm, a_hat.data(), aBytes);
    std::memcpy(tGm, t_hat.data(), tBytes);
    std::memcpy(rGm, r_hat.data(), rBytes);
    ICPU_RUN_KF(f203_encrypt_g3_linear, kAtRBlockDim, aGm, rGm, tGm, uGm, trGm);
    std::memcpy(u_hat.data(), uGm, uBytes);
    std::memcpy(tr_hat.data(), trGm, trBytes);
    AscendC::GmFree(aGm);
    AscendC::GmFree(tGm);
    AscendC::GmFree(rGm);
    AscendC::GmFree(uGm);
    AscendC::GmFree(trGm);
#else
    if (run_g3_device_sim_once(a_hat.data(), r_hat.data(), t_hat.data(), u_hat.data(), tr_hat.data()) != 0) {
        return 12;
    }
#endif
    if (!WriteFile(case_dir + "/output/u_hat.bin", u_hat.data(), u_hat.size())) {
        std::fprintf(stderr, "[main_encrypt] write u_hat.bin failed\n");
        return 14;
    }
    if (!WriteFile(case_dir + "/output/tr_hat.bin", tr_hat.data(), tr_hat.size())) {
        std::fprintf(stderr, "[main_encrypt] write tr_hat.bin failed\n");
        return 15;
    }
    std::printf("[main_encrypt] G3 linear done u_hat=%uB tr_hat=%uB\n", F203_U_HAT_BYTES, F203_TR_HAT_BYTES);
    } // !g5_sim_prelude

    if (encrypt_gate < 4) {
        return 0;
    }

    std::vector<uint8_t> lut_intt_even(tiling::lutEvenOddFileBytes);
    std::vector<uint8_t> lut_intt_odd(tiling::lutEvenOddFileBytes);
    if (!ReadFile(case_dir + "/input/lut_intt_even_stacked.bin", rs, lut_intt_even.data(), lut_intt_even.size()) ||
        rs != lut_intt_even.size()) {
        std::fprintf(stderr, "[main_encrypt] bad lut_intt_even_stacked.bin size=%zu\n", rs);
        return 16;
    }
    if (!ReadFile(case_dir + "/input/lut_intt_odd_stacked.bin", rs, lut_intt_odd.data(), lut_intt_odd.size()) ||
        rs != lut_intt_odd.size()) {
        std::fprintf(stderr, "[main_encrypt] bad lut_intt_odd_stacked.bin size=%zu\n", rs);
        return 17;
    }

    std::vector<uint8_t> m_buf(F203_MSG_BYTES);
    if (!ReadFile(case_dir + "/input/m.bin", rs, m_buf.data(), m_buf.size()) || rs != m_buf.size()) {
        std::fprintf(stderr, "[main_encrypt] bad m.bin size=%zu\n", rs);
        return 18;
    }

    std::vector<uint8_t> u_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_padded(F203_U_HAT_BYTES, 0);
    std::memcpy(tr_padded.data(), tr_hat.data(), F203_TR_HAT_BYTES);
    std::vector<uint8_t> tr_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> v_poly(F203_E2_POLY_BYTES);
    std::vector<uint8_t> c_out(F203_CT_PKE_BYTES);

    std::printf("[main_encrypt] G4 INTT+noise+pack ENCRYPT_GATE=%d\n", encrypt_gate);

#ifdef ASCENDC_CPU_DEBUG
    auto launch_intt = [](uint32_t blockDim, uint8_t *dstGm, uint8_t *srcGm, uint8_t *wsGm, TilingData tilingHost) {
        ICPU_RUN_KF(f203_encrypt_intt, blockDim, dstGm, srcGm, wsGm, tilingHost);
    };
    auto launch_noise = [](uint32_t blockDim, uint8_t *uGm, uint8_t *e1Gm, uint8_t *trGm, uint8_t *e2Gm,
                           uint8_t *mGm, uint8_t *vGm) {
        ICPU_RUN_KF(f203_encrypt_g4_noise, blockDim, uGm, e1Gm, trGm, e2Gm, mGm, vGm);
    };
    auto launch_pack = [](uint32_t blockDim, uint8_t *uGm, uint8_t *vGm, uint8_t *cGm) {
        ICPU_RUN_KF(f203_encrypt_pack, blockDim, uGm, vGm, cGm);
    };
#else
    auto launch_intt = [](uint32_t blockDim, aclrtStream stream, uint8_t *dstDev, uint8_t *srcDev, uint8_t *wsDev,
                         TilingData *tilingHostPtr) {
        ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(blockDim, stream, dstDev, srcDev, wsDev, tilingHostPtr);
    };
    auto launch_noise = [](uint32_t blockDim, aclrtStream stream, uint8_t *uDev, uint8_t *e1Dev, uint8_t *trDev,
                           uint8_t *e2Dev, uint8_t *mDev, uint8_t *vDev) {
        ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise)(blockDim, stream, uDev, e1Dev, trDev, e2Dev, mDev, vDev);
    };
    auto launch_pack = [](uint32_t blockDim, aclrtStream stream, uint8_t *uDev, uint8_t *vDev, uint8_t *cDev) {
        ACLRT_LAUNCH_KERNEL(f203_encrypt_pack)(blockDim, stream, uDev, vDev, cDev);
    };
#endif

    if (run_intt(launch_intt, u_hat.data(), lut_intt_even.data(), lut_intt_odd.data(), u_time.data()) != 0) {
        return 19;
    }
    if (run_intt(launch_intt, tr_padded.data(), lut_intt_even.data(), lut_intt_odd.data(), tr_time.data()) != 0) {
        return 20;
    }

    const uint8_t *e1_ptr = re_flat.data() + F203_R_POLYVEC_BYTES;
    const uint8_t *e2_ptr = re_flat.data() + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    if (run_g4_noise(launch_noise, u_time.data(), e1_ptr, tr_time.data(), e2_ptr, m_buf.data(), v_poly.data()) != 0) {
        return 21;
    }
    if (run_pack(launch_pack, u_time.data(), v_poly.data(), c_out.data()) != 0) {
        return 22;
    }

    if (!WriteFile(case_dir + "/output/c.bin", c_out.data(), c_out.size())) {
        std::fprintf(stderr, "[main_encrypt] write c.bin failed\n");
        return 23;
    }
    std::printf("[main_encrypt] G4 done c.bin=%uB\n", F203_CT_PKE_BYTES);
    return 0;
}
