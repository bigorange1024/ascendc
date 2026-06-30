/**
 * @file main_encrypt.cpp
 * @brief Alg.14 Encrypt 探针 Host 入口。
 *
 * **生产路径（G5，默认）**：`run_encrypt_g5_cpu_full` / `run_g5_sim_full`（见 main_encrypt_g5_run.cpp）。
 *
 * **G0–G4**：过渡路线，G5 测通后已冻结（见 frozen-gates/FROZEN.md）；`ENCRYPT_GATE<5` 仅历史回放，启动打 WARN。
 *
 * 生产 I/O（G5）：
 *   input/  — ek_pke.bin + m.bin + coins.bin + LUT
 *   output/ — c.bin (1568B) + 中间张量（verify_gate）
 */
#include "data_utils.h"
#include "f203_encrypt_g5_run.hpp"
#include "f203_encrypt_layout.h"
#include "f203_encrypt_g4_host_scalar.hpp"   // frozen-gates/frozen-g4-host-scalar-tail/（G4 过渡，禁止 G5 使用）
#include "f203_encrypt_pack_host_scalar.hpp" // frozen-gates/frozen-g4-host-scalar-tail/
#include "f203_ntt_r_tiling.h"
#include "f203_encrypt_intt_tiling.h"
#include "f203_encrypt_pack_config.hpp"
#include "innerproduct_tiling.h"
#include "f203_encrypt_at_r5_tiling.h"  // G3 合并核 at_r5 维度（INTEGRATION_PLAN §2.3）
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_marker_custom.h"
#include "aclrtlaunch_f203_encrypt_prep_a_hat.h"
#include "aclrtlaunch_f203_encrypt_prep_re.h"
#include "aclrtlaunch_f203_encrypt_ntt_r.h"
#if !defined(F203_FUNCKEY_EXPERIMENT)
#include "aclrtlaunch_f203_encrypt_at_r5.h"   // G3 合并核 at_r5（INTEGRATION_PLAN §2.3 取代旧 g3_linear/at_r/t_dot_r）
#endif
#include "aclrtlaunch_f203_encrypt_intt.h"
#include "aclrtlaunch_f203_encrypt_g4_noise.h"
#if !defined(F203_FUNCKEY_EXPERIMENT)
#include "aclrtlaunch_f203_encrypt_pack.h"
#endif
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
extern "C" __global__ __aicore__ void f203_encrypt_at_r5(GM_ADDR matM, GM_ADDR rHat, GM_ADDR uTr);
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
/**
 * SIM/NPU G3：单 ACL session 跑 at_r5（手工组装 matM[(j*kP+p)*kN+c]）。
 *
 * 旧 SIM G3 路径（run_g3_at_r_device_once + col0 hack + run_g3_tr_via_at_r_device_once 两 session）
 * 在 2026-06-30 funckey 实验中确诊为 507000 主因之一（多 session 清空 device binary cache 触发 ACL 重载，
 * SIM CAModel 对 func_key ≥ 5 触发 ACL_ERROR_RT_INTERNAL_ERROR），整段废弃（详 qa/2026-06/2026-06-30-funckey...纪要）。
 * 仅 encrypt_gate<5 旧字节级测试入口仍调用本函数；生产路径走 run_g5_sim_full 单 session。
 *
 * @param a_in 已 NTT 化 Â polyvec（16 poly，行主序）
 * @param r_in r̂ polyvec（4 poly）
 * @param t_in t̂ polyvec（4 poly；本函数把它当作 Â 的虚拟列 4）
 * @param u_out û（4 poly = 4*N int32）
 * @param tr_out tr̂（1 poly = N int32）
 */
int run_g3_device_sim_once(const uint8_t *a_in, const uint8_t *r_in, const uint8_t *t_in, uint8_t *u_out,
                           uint8_t *tr_out)
{
    constexpr int32_t kK_g3 = at_r5_tiling::kK;
    constexpr int32_t kP_g3 = at_r5_tiling::kP;
    constexpr int32_t kN_g3 = at_r5_tiling::kN;
    const size_t matBytes = static_cast<size_t>(at_r5_tiling::kMatBytes);
    const size_t rBytes = static_cast<size_t>(at_r5_tiling::kRBytes);
    const size_t uTrBytes = static_cast<size_t>(at_r5_tiling::kOutBytes);

    // host 拼 matM[(j*kP+p)*kN+c]：p<4 取 Â[j,p]，p=4 取 t̂[j]（INTEGRATION_PLAN §2.3）
    std::vector<uint8_t> matHost(matBytes);
    const int32_t *aSrc = reinterpret_cast<const int32_t *>(a_in);
    const int32_t *tSrc = reinterpret_cast<const int32_t *>(t_in);
    int32_t *matDst = reinterpret_cast<int32_t *>(matHost.data());
    for (int32_t j = 0; j < kK_g3; ++j) {
        for (int32_t p = 0; p < kP_g3; ++p) {
            int32_t *dst = matDst + (static_cast<size_t>(j) * kP_g3 + static_cast<size_t>(p)) * kN_g3;
            const int32_t *src = (p < kK_g3) ? (aSrc + (static_cast<size_t>(j) * kK_g3 + static_cast<size_t>(p)) * kN_g3)
                                              : (tSrc + static_cast<size_t>(j) * kN_g3);
            std::memcpy(dst, src, static_cast<size_t>(kN_g3) * sizeof(int32_t));
        }
    }

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *matDev = nullptr;
    uint8_t *rDev = nullptr;
    uint8_t *uTrDev = nullptr;
    CHECK_ACL(aclrtMalloc((void **)&matDev, matBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rDev, rBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTrDev, uTrBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMemcpy(matDev, matBytes, matHost.data(), matBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rDev, rBytes, r_in, rBytes, ACL_MEMCPY_HOST_TO_DEVICE));

#if defined(F203_FUNCKEY_EXPERIMENT)
    // funckey 实验：at_r5 已从 KERNEL_FILES 移除，不 launch；fake [u | tr̂] = 0 用以验证后续 G4 路径
    std::printf("[funckey-exp] run_g3_device_sim_once: skip at_r5 (uTr fake=0)\n");
    CHECK_ACL(aclrtMemset(uTrDev, uTrBytes, 0, uTrBytes));
    CHECK_ACL(aclrtSynchronizeStream(stream));
#else
    const uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r5)(at_r5_tiling::kBlockDim, stream, matDev, rDev, uTrDev);
    if (ret != 0) {
        std::fprintf(stderr, "[main_encrypt] G3 at_r5 launch failed ret=%u\n", ret);
        CHECK_ACL(aclrtFree(matDev));
        CHECK_ACL(aclrtFree(rDev));
        CHECK_ACL(aclrtFree(uTrDev));
        CHECK_ACL(aclrtDestroyStream(stream));
        CHECK_ACL(aclrtResetDevice(deviceId));
        CHECK_ACL(aclFinalize());
        return 12;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
#endif

    // 拆分：uTrDev[0..4*N*4 B] = û；后 1*N*4 B = tr̂
    CHECK_ACL(aclrtMemcpy(u_out, F203_U_HAT_BYTES, uTrDev, F203_U_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(tr_out, F203_TR_HAT_BYTES,
                          uTrDev + static_cast<size_t>(F203_U_HAT_BYTES), F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtFree(matDev));
    CHECK_ACL(aclrtFree(rDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
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

#ifndef ASCENDC_CPU_DEBUG
/** SIM：单 session INTT(û)+INTT(tr̂) → host 时域 buffer。 */
int run_g4_intt_pair_sim_once(const uint8_t *u_hat_ntt, const uint8_t *tr_hat_ntt, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *u_time_out, uint8_t *tr_time_out)
{
    using namespace tiling;
    TilingData tilingHost{};
    tilingHost.tileLength = static_cast<int32_t>(n);
    tilingHost.kPolys = static_cast<int32_t>(kK);
    tilingHost.mixPass = static_cast<int32_t>(kInttMixPass);

    std::vector<uint8_t> tr_padded(F203_U_HAT_BYTES, 0);
    std::memcpy(tr_padded.data(), tr_hat_ntt, F203_TR_HAT_BYTES);

    int rc = 0;
    int32_t deviceId = 0;
    aclrtStream stream = nullptr;
    TilingData *tilingHostPtr = nullptr;
    uint8_t *uHatSrcDev = nullptr;
    uint8_t *trSrcDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *uTimeDev = nullptr;
    uint8_t *trTimeDev = nullptr;
    std::vector<uint8_t> wsHost(wssize, 0);

    CHECK_ACL(aclInit(nullptr));
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateStream(&stream));
    CHECK_ACL(aclrtMallocHost((void **)&tilingHostPtr, sizeof(TilingData)));
    *tilingHostPtr = tilingHost;
    CHECK_ACL(aclrtMalloc((void **)&uHatSrcDev, srcFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trSrcDev, srcFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uHatSrcDev, srcFileBytes, u_hat_ntt, srcFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(trSrcDev, srcFileBytes, tr_padded.data(), srcFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, uTimeDev, uHatSrcDev, wsDev,
                                                          tilingHostPtr);
    if (ret != 0) {
        std::fprintf(stderr, "[main_encrypt] G4 INTT u launch failed ret=%u\n", ret);
        rc = 19;
        goto cleanup;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, trTimeDev, trSrcDev, wsDev, tilingHostPtr);
    if (ret != 0) {
        std::fprintf(stderr, "[main_encrypt] G4 INTT tr launch failed ret=%u\n", ret);
        rc = 20;
        goto cleanup;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(u_time_out, dstFileBytes, uTimeDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(tr_time_out, dstFileBytes, trTimeDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));

cleanup:
    if (uHatSrcDev != nullptr) {
        CHECK_ACL(aclrtFree(uHatSrcDev));
    }
    if (trSrcDev != nullptr) {
        CHECK_ACL(aclrtFree(trSrcDev));
    }
    if (wsDev != nullptr) {
        CHECK_ACL(aclrtFree(wsDev));
    }
    if (uTimeDev != nullptr) {
        CHECK_ACL(aclrtFree(uTimeDev));
    }
    if (trTimeDev != nullptr) {
        CHECK_ACL(aclrtFree(trTimeDev));
    }
    if (tilingHostPtr != nullptr) {
        CHECK_ACL(aclrtFreeHost(tilingHostPtr));
    }
    if (stream != nullptr) {
        CHECK_ACL(aclrtDestroyStream(stream));
    }
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return rc;
}

#if defined(F203_FUNCKEY_EXPERIMENT)
/**
 * funckey 实验探针：独立 ACL session，单点 launch g4_noise（zero 输入），
 * 仅记录 ret 值。期望：实验模式下 g4_noise 落 func_key=4，launch 返回 0；
 * 若仍 507000，则家里 agent 的 func_key 假说不成立或不完整。
 */
int run_funckey_probe_g4_noise_once()
{
    int rc = 0;
    int32_t deviceId = 0;
    aclrtStream stream = nullptr;
    uint8_t *uDev = nullptr, *e1Dev = nullptr, *trDev = nullptr;
    uint8_t *e2Dev = nullptr, *mDev = nullptr, *vDev = nullptr;
    const size_t uBytes = F203_U_HAT_BYTES;
    const size_t e1Bytes = F203_E1_POLYVEC_BYTES;
    const size_t trBytes = F203_TR_HAT_BYTES;
    const size_t e2Bytes = F203_E2_POLY_BYTES;
    const size_t mBytes = F203_MSG_BYTES;
    const size_t vBytes = F203_E2_POLY_BYTES;

    CHECK_ACL(aclInit(nullptr));
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateStream(&stream));
    CHECK_ACL(aclrtMalloc((void **)&uDev, uBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e1Dev, e1Bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trDev, trBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&e2Dev, e2Bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, vBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMemset(uDev, uBytes, 0, uBytes));
    CHECK_ACL(aclrtMemset(e1Dev, e1Bytes, 0, e1Bytes));
    CHECK_ACL(aclrtMemset(trDev, trBytes, 0, trBytes));
    CHECK_ACL(aclrtMemset(e2Dev, e2Bytes, 0, e2Bytes));
    CHECK_ACL(aclrtMemset(mDev, mBytes, 0, mBytes));
    CHECK_ACL(aclrtMemset(vDev, vBytes, 0, vBytes));

    const uint32_t blockDim = 1;
    std::fprintf(stderr, "[funckey-exp] launching f203_encrypt_g4_noise (expect func_key=4 in 5-AIV build)...\n");
    const uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise)(blockDim, stream, uDev, e1Dev, trDev, e2Dev, mDev,
                                                                   vDev);
    std::fprintf(stderr, "[funckey-exp] g4_noise launch ret=%u %s\n", ret,
                 (ret == 0) ? "(SUCCESS → func_key 假说成立)" : "(FAIL → 仍 507000 等)");
    if (ret == 0) {
        CHECK_ACL(aclrtSynchronizeStream(stream));
    }
    rc = static_cast<int>(ret);

    if (uDev) CHECK_ACL(aclrtFree(uDev));
    if (e1Dev) CHECK_ACL(aclrtFree(e1Dev));
    if (trDev) CHECK_ACL(aclrtFree(trDev));
    if (e2Dev) CHECK_ACL(aclrtFree(e2Dev));
    if (mDev) CHECK_ACL(aclrtFree(mDev));
    if (vDev) CHECK_ACL(aclrtFree(vDev));
    if (stream) CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return rc;
}
#endif

/**
 * SIM G4 tail（**已冻结**，frozen-gates/FROZEN.md）：device INTT×2 → host 噪声/μ+pack。
 * 仅 ENCRYPT_GATE=4 过渡回放；G5 走 run_g5_sim_full 全 device，不得调用本函数。
 */
int run_g4_tail_sim_once(const uint8_t *u_hat_ntt, const uint8_t *tr_hat_ntt, const uint8_t *e1,
                         const uint8_t *e2, const uint8_t *m, const uint8_t *lut_intt_even,
                         const uint8_t *lut_intt_odd, uint8_t *c_out)
{
    std::vector<uint8_t> u_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> v_poly(F203_E2_POLY_BYTES);

#if defined(F203_FUNCKEY_EXPERIMENT)
    // funckey 实验：在 INTT 之前先 single-shot launch g4_noise，看是否仍 507000。
    // 注意：此 launch 与下面 INTT 在不同 ACL session（probe 自带 aclInit/Finalize），
    // 与 INTT 不互相干扰；目的纯粹是观测 g4_noise launch ret。
    const int probe = run_funckey_probe_g4_noise_once();
    std::fprintf(stderr, "[funckey-exp] probe returned %d\n", probe);
#endif

    const int ir = run_g4_intt_pair_sim_once(u_hat_ntt, tr_hat_ntt, lut_intt_even, lut_intt_odd, u_time.data(),
                                             tr_time.data());
    if (ir != 0) {
        return ir;
    }
    f203_g4_host::add_noise_embed(u_time.data(), e1, tr_time.data(), e2, m, v_poly.data());
    f203_pack_host::pack_ciphertext(u_time.data(), v_poly.data(), c_out);
    return 0;
}
#endif

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
    if (encrypt_gate < 5) {
        std::fprintf(stderr,
                     "[WARN] ENCRYPT_GATE=%d：过渡路线已冻结（G5 为唯一生产路径）；见 frozen-gates/FROZEN.md\n",
                     encrypt_gate);
    }

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
        std::printf("[main_encrypt] G5 SIM: 单 session prep..pack（device G4，INTEGRATION_PLAN §4）\n");
        const int rc = run_g5_sim_full(ek_buf.data(), coins_buf.data(), m_buf.data(), a_hat.data(), re_flat.data(),
                                       r_hat.data(), t_hat.data(), u_hat.data(), tr_hat.data(), lut_even.data(),
                                       lut_odd.data(), lut_intt_even.data(), lut_intt_odd.data(), c_out.data());
        if (rc != 0) {
            return rc;
        }
        const int wo = write_g1_outputs(case_dir + "/output", a_hat.data(), re_flat.data());
        if (wo != 0) {
            return 6;
        }
        if (!WriteFile(case_dir + "/output/r_hat.bin", r_hat.data(), r_hat.size()) ||
            !WriteFile(case_dir + "/output/t_hat.bin", t_hat.data(), t_hat.size()) ||
            !WriteFile(case_dir + "/output/u_hat.bin", u_hat.data(), u_hat.size()) ||
            !WriteFile(case_dir + "/output/tr_hat.bin", tr_hat.data(), tr_hat.size()) ||
            !WriteFile(case_dir + "/output/c.bin", c_out.data(), c_out.size())) {
            return 10;
        }
        std::printf("[main_encrypt] G5 done c.bin=%uB (SIM device 全链)\n", F203_CT_PKE_BYTES);
        return 0;
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

    std::printf("[main_encrypt] G3 linear ENCRYPT_GATE=%d blockDim=%u (at_r5 合并核：单 launch 出 û+tr̂)\n",
                encrypt_gate, kAtRBlockDim);

#ifdef ASCENDC_CPU_DEBUG
    // G3 CPU 孪生：host 拼 matM[(j*kP+p)*kN+c]（详 INTEGRATION_PLAN §2.3 / at_r5_layout.h::mat_offset），
    // 调 at_r5 一次出 [û | tr̂]，替代旧 g3_linear 4-合-1 路径（旧 4 核：g3_linear/g3_linear4/at_r/t_dot_r）。
    const size_t uBytes = static_cast<size_t>(F203_U_HAT_BYTES);
    const size_t trBytes = static_cast<size_t>(F203_TR_HAT_BYTES);
    const size_t aBytes = static_cast<size_t>(F203_AHAT_BYTES);
    const size_t rBytes = static_cast<size_t>(F203_R_HAT_BYTES);
    [[maybe_unused]] const size_t tBytes = static_cast<size_t>(F203_T_HAT_BYTES);
    const size_t matBytes = static_cast<size_t>(at_r5_tiling::kMatBytes);
    const size_t uTrBytes = static_cast<size_t>(at_r5_tiling::kOutBytes);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *matGm = (uint8_t *)AscendC::GmAlloc(matBytes);
    uint8_t *rGm = (uint8_t *)AscendC::GmAlloc(rBytes);
    uint8_t *uTrGm = (uint8_t *)AscendC::GmAlloc(uTrBytes);

    // matM[(j*kP+p)*kN+c]：p<4 取 Â[j,p]（aHat[(j*K+p)*N+c]），p=4 取 t̂[j]（tHat[j*N+c]）。
    // 等价 KeyGen 行 18 索引对调（FIPS 203 Alg.14 §18）：out[p] = Σ_j Â[j,p] *_NTT r̂[j] = û[p]，p=4 给出 tr̂。
    constexpr int32_t kK_g3 = at_r5_tiling::kK;
    constexpr int32_t kP_g3 = at_r5_tiling::kP;
    constexpr int32_t kN_g3 = at_r5_tiling::kN;
    const int32_t *aHatI32 = reinterpret_cast<const int32_t *>(a_hat.data());
    const int32_t *tHatI32 = reinterpret_cast<const int32_t *>(t_hat.data());
    int32_t *matI32 = reinterpret_cast<int32_t *>(matGm);
    for (int32_t j = 0; j < kK_g3; ++j) {
        for (int32_t p = 0; p < kP_g3; ++p) {
            int32_t *dst = matI32 + (static_cast<size_t>(j) * kP_g3 + static_cast<size_t>(p)) * kN_g3;
            const int32_t *src = (p < kK_g3) ? (aHatI32 + (static_cast<size_t>(j) * kK_g3 + static_cast<size_t>(p)) * kN_g3)
                                              : (tHatI32 + static_cast<size_t>(j) * kN_g3);
            std::memcpy(dst, src, static_cast<size_t>(kN_g3) * sizeof(int32_t));
        }
    }
    std::memcpy(rGm, r_hat.data(), rBytes);

    ICPU_RUN_KF(f203_encrypt_at_r5, kAtRBlockDim, matGm, rGm, uTrGm);

    // 拆分：uTrGm[0..4*N*4 B] = û；[4*N*4 B..5*N*4 B] = tr̂（at_r5 输出为 [kP, kN] 行主序连续）。
    std::memcpy(u_hat.data(),  uTrGm,                       uBytes);
    std::memcpy(tr_hat.data(), uTrGm + uBytes,              trBytes);
    AscendC::GmFree(matGm);
    AscendC::GmFree(rGm);
    AscendC::GmFree(uTrGm);
    // 抑制 unused 警告（aBytes 仅文档用途，保留以便 reader 对照旧路径）
    (void)aBytes;
    (void)tBytes;
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

    const uint8_t *e1_ptr = re_flat.data() + F203_R_POLYVEC_BYTES;
    const uint8_t *e2_ptr = re_flat.data() + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;

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
    if (run_intt(launch_intt, u_hat.data(), lut_intt_even.data(), lut_intt_odd.data(), u_time.data()) != 0) {
        return 19;
    }
    if (run_intt(launch_intt, tr_padded.data(), lut_intt_even.data(), lut_intt_odd.data(), tr_time.data()) != 0) {
        return 20;
    }
    if (run_g4_noise(launch_noise, u_time.data(), e1_ptr, tr_time.data(), e2_ptr, m_buf.data(), v_poly.data()) != 0) {
        return 21;
    }
    if (run_pack(launch_pack, u_time.data(), v_poly.data(), c_out.data()) != 0) {
        return 22;
    }
#else
    if (run_g4_tail_sim_once(u_hat.data(), tr_hat.data(), e1_ptr, e2_ptr, m_buf.data(), lut_intt_even.data(),
                             lut_intt_odd.data(), c_out.data()) != 0) {
        return 21;
    }
#endif

    if (!WriteFile(case_dir + "/output/c.bin", c_out.data(), c_out.size())) {
        std::fprintf(stderr, "[main_encrypt] write c.bin failed\n");
        return 23;
    }
    std::printf("[main_encrypt] G4 done c.bin=%uB\n", F203_CT_PKE_BYTES);
    return 0;
}
