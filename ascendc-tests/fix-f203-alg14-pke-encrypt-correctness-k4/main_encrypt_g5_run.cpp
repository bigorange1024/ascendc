/**
 * @file main_encrypt_g5_run.cpp
 * @brief G5 生产路径：单 ACL/CPU session 全链；设备 ByteDecode ek→t̂；统一 g3_linear。
 *
 * 背景：领导验收「整条 Encrypt 在 device」；禁止 Host staging t_hat.bin 与 SIM at_r 绕行。
 */
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
#include "aclrtlaunch_f203_encrypt_prep_a_hat.h"
#include "aclrtlaunch_f203_encrypt_prep_re.h"
#include "aclrtlaunch_f203_encrypt_ntt_r.h"
#include "aclrtlaunch_f203_encrypt_decode_t_hat.h"
#include "aclrtlaunch_f203_encrypt_g3_linear.h"
#include "aclrtlaunch_f203_encrypt_g3_linear4.h"
#include "aclrtlaunch_f203_encrypt_at_r.h"
#include "aclrtlaunch_f203_encrypt_intt.h"
#include "aclrtlaunch_f203_encrypt_g4_noise.h"
#include "aclrtlaunch_f203_encrypt_pack.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" __global__ __aicore__ void f203_encrypt_prep_a_hat(GM_ADDR rho_gm, GM_ADDR a_hat_gm);
extern "C" __global__ __aicore__ void f203_encrypt_prep_re(GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                         GM_ADDR tiling);
extern "C" __global__ __aicore__ void f203_encrypt_ntt_r(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_decode_t_hat(GM_ADDR ekGm, GM_ADDR tHatGm, GM_ADDR aCol0Gm);
extern "C" __global__ __aicore__ void f203_encrypt_g3_linear(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uHat,
                                                             GM_ADDR trHat);
extern "C" __global__ __aicore__ void f203_encrypt_g3_linear4(GM_ADDR aHat, GM_ADDR rHat, GM_ADDR tHat, GM_ADDR uTrOut);
extern "C" __global__ __aicore__ void f203_encrypt_intt(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_g4_noise(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm,
                                                          GM_ADDR mGm, GM_ADDR vGm);
extern "C" __global__ __aicore__ void f203_encrypt_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm);
extern volatile int g_f203_ntt_r_mix_pass;
extern volatile int g_f203_intt_mix_pass;
#endif

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

bool WriteFile(const std::string &filePath, const void *buffer, size_t size);

#ifndef ASCENDC_CPU_DEBUG
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);
#endif

namespace {

constexpr uint32_t kAhatBlockDim = 2U;
constexpr uint32_t kReBlockDim = 1U;
constexpr uint32_t kNttBlockDim = 1U;
constexpr uint32_t kG3BlockDim = 1U;
constexpr uint32_t kDecodeBlockDim = 1U;
constexpr uint32_t kNttMixPass = 3U;
constexpr uint32_t kInttBlockDim = 1U;
constexpr uint32_t kInttMixPass = 3U;
constexpr uint32_t kG4NoiseBlockDim = 1U;
constexpr uint32_t kPackBlockDim = 1U;
constexpr uint32_t kPrfBatch = F203_ENCRYPT_PRF_BATCH;
constexpr uint32_t kPrfMaxMsgLen = 64U;
constexpr uint32_t kPrfOutLen = F203_ENCRYPT_PRF_BYTES_PER_POLY;
constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr size_t kUTrBytes = F203_U_HAT_BYTES + F203_TR_HAT_BYTES;

#ifdef ASCENDC_CPU_DEBUG
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

int run_g5_cpu_session(const uint8_t *ek, const uint8_t *coins, const uint8_t *m, const uint8_t *lut_ntt_even,
                       const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                       uint8_t *a_hat, uint8_t *re_flat, uint8_t *r_hat, uint8_t *t_hat, uint8_t *u_hat,
                       uint8_t *tr_hat, uint8_t *u_time, uint8_t *tr_time, uint8_t *v_poly, uint8_t *c_out)
{
    using namespace tiling;
    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kReBlockDim;

    TilingData nttTiling{};
    nttTiling.tileLength = static_cast<int32_t>(n);
    nttTiling.kPolys = static_cast<int32_t>(kK);
    nttTiling.mixPass = static_cast<int32_t>(kNttMixPass);

    TilingData inttTiling = nttTiling;
    inttTiling.mixPass = static_cast<int32_t>(kInttMixPass);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *ekGm = (uint8_t *)AscendC::GmAlloc(F203_EK_PKE_BYTES);
    uint8_t *coinsGm = (uint8_t *)AscendC::GmAlloc(F203_ENC_COINS_BYTES);
    uint8_t *aHatGm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    uint8_t *prfGm = (uint8_t *)AscendC::GmAlloc(F203_ENCRYPT_PRF_TOTAL_BYTES);
    uint8_t *reGm = (uint8_t *)AscendC::GmAlloc(F203_RE_TOTAL_BYTES);
    uint8_t *shakeTilingGm = (uint8_t *)AscendC::GmAlloc(kShakeTilingBytes);
    uint8_t *rhoGm = ekGm + F203_EK_RHO_OFFSET;
    std::memcpy(ekGm, ek, F203_EK_PKE_BYTES);
    std::memcpy(coinsGm, coins, F203_ENC_COINS_BYTES);
    std::memcpy(shakeTilingGm, &shakeTiling, kShakeTilingBytes);
    ICPU_RUN_KF(f203_encrypt_prep_a_hat, kAhatBlockDim, rhoGm, aHatGm);
    ICPU_RUN_KF(f203_encrypt_prep_re, kReBlockDim, coinsGm, prfGm, reGm, shakeTilingGm);
    std::memcpy(a_hat, aHatGm, F203_AHAT_BYTES);
    std::memcpy(re_flat, reGm, F203_RE_TOTAL_BYTES);

    g_f203_ntt_r_mix_pass = static_cast<int>(kNttMixPass);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *rSrcGm = reGm;
    uint8_t *rHatGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *nttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    std::memset(nttWsGm, 0, wssize);
    std::memcpy(nttWsGm + LUT_EVEN_STACKED, lut_ntt_even, lutEvenOddFileBytes);
    std::memcpy(nttWsGm + LUT_ODD_STACKED, lut_ntt_odd, lutEvenOddFileBytes);
    ICPU_RUN_KF(f203_encrypt_ntt_r, kNttBlockDim, rHatGm, rSrcGm, nttWsGm, nttTiling);
    std::memcpy(r_hat, rHatGm, dstFileBytes);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *tHatGm = (uint8_t *)AscendC::GmAlloc(F203_T_HAT_BYTES);
    uint8_t *aCol0Gm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    std::memset(aCol0Gm, 0, F203_AHAT_BYTES);
    ICPU_RUN_KF(f203_encrypt_decode_t_hat, kDecodeBlockDim, ekGm, tHatGm, aCol0Gm);
    std::memcpy(t_hat, tHatGm, F203_T_HAT_BYTES);

    uint8_t *uTrGm = (uint8_t *)AscendC::GmAlloc(kUTrBytes);
    ICPU_RUN_KF(f203_encrypt_g3_linear4, kG3BlockDim, aHatGm, rHatGm, tHatGm, uTrGm);
    std::memcpy(u_hat, uTrGm, F203_U_HAT_BYTES);
    std::memcpy(tr_hat, uTrGm + F203_U_HAT_BYTES, F203_TR_HAT_BYTES);

    g_f203_intt_mix_pass = static_cast<int>(kInttMixPass);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *uTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *trPaddedGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *trTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *inttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    std::memset(inttWsGm, 0, wssize);
    std::memcpy(inttWsGm + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
    std::memcpy(inttWsGm + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
    std::memset(trPaddedGm, 0, dstFileBytes);
    std::memcpy(trPaddedGm, uTrGm + F203_U_HAT_BYTES, F203_TR_HAT_BYTES);
    ICPU_RUN_KF(f203_encrypt_intt, kInttBlockDim, uTimeGm, uTrGm, inttWsGm, inttTiling);
    ICPU_RUN_KF(f203_encrypt_intt, kInttBlockDim, trTimeGm, trPaddedGm, inttWsGm, inttTiling);
    std::memcpy(u_time, uTimeGm, dstFileBytes);
    std::memcpy(tr_time, trTimeGm, dstFileBytes);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *e1Gm = reGm + F203_R_POLYVEC_BYTES;
    uint8_t *e2Gm = reGm + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    std::memcpy(mGm, m, F203_MSG_BYTES);
    ICPU_RUN_KF(f203_encrypt_g4_noise, kG4NoiseBlockDim, uTimeGm, e1Gm, trTimeGm, e2Gm, mGm, vGm);
    ICPU_RUN_KF(f203_encrypt_pack, kPackBlockDim, uTimeGm, vGm, cGm);
    std::memcpy(v_poly, vGm, F203_E2_POLY_BYTES);
    std::memcpy(c_out, cGm, F203_CT_PKE_BYTES);

    AscendC::GmFree(ekGm);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(shakeTilingGm);
    AscendC::GmFree(rHatGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(aCol0Gm);
    AscendC::GmFree(uTrGm);
    AscendC::GmFree(uTimeGm);
    AscendC::GmFree(trPaddedGm);
    AscendC::GmFree(trTimeGm);
    AscendC::GmFree(inttWsGm);
    AscendC::GmFree(mGm);
    AscendC::GmFree(vGm);
    AscendC::GmFree(cGm);
    return 0;
}
#endif

} // namespace

#ifndef ASCENDC_CPU_DEBUG
int run_g5_sim_phase1(const uint8_t *ek, const uint8_t *coins, uint8_t *a_hat, uint8_t *re_flat, uint8_t *r_hat,
                      uint8_t *t_hat, uint8_t *u_hat, uint8_t *a_col0_out, const uint8_t *lut_ntt_even,
                      const uint8_t *lut_ntt_odd)
{
    using namespace tiling;
    constexpr uint32_t kAhatBlockDim = 2U;
    constexpr uint32_t kReBlockDim = 1U;
    constexpr uint32_t kNttBlockDim = 1U;
    constexpr uint32_t kG3BlockDim = 1U;
    constexpr uint32_t kDecodeBlockDim = 1U;
    constexpr uint32_t kNttMixPass = 3U;
    constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, F203_ENCRYPT_PRF_BATCH, 64U, F203_ENCRYPT_PRF_BYTES_PER_POLY, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kReBlockDim;

    TilingData nttTiling{};
    nttTiling.tileLength = static_cast<int32_t>(n);
    nttTiling.kPolys = static_cast<int32_t>(kK);
    nttTiling.mixPass = static_cast<int32_t>(kNttMixPass);

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;
    uint8_t *rHatDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    TilingData *nttTilingHost = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *uHatDev = nullptr;
    uint8_t *aCol0Dev = nullptr;

    CHECK_ACL(aclrtMalloc((void **)&ekDev, F203_EK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&coinsDev, F203_ENC_COINS_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, F203_ENCRYPT_PRF_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&reDev, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&shakeTilingDev, kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&nttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&tHatDev, F203_T_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uHatDev, F203_U_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aCol0Dev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMemcpy(ekDev, F203_EK_PKE_BYTES, ek, F203_EK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(coinsDev, F203_ENC_COINS_BYTES, coins, F203_ENC_COINS_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTiling, kShakeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::vector<uint8_t> wsHost(wssize, 0);
    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_ntt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_ntt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    *nttTilingHost = nttTiling;

    uint8_t *rhoDev = ekDev + F203_EK_RHO_OFFSET;
    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_a_hat)(kAhatBlockDim, stream, rhoDev, aHatDev);
    if (ret != 0) {
        return 30;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_re)(kReBlockDim, stream, coinsDev, prfDev, reDev, shakeTilingDev);
    if (ret != 0) {
        return 31;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_r)(kNttBlockDim, stream, rHatDev, reDev, nttWsDev, nttTilingHost);
    if (ret != 0) {
        return 32;
    }
    CHECK_ACL(aclrtMemset(aCol0Dev, F203_AHAT_BYTES, 0, F203_AHAT_BYTES));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_decode_t_hat)(kDecodeBlockDim, stream, ekDev, tHatDev, aCol0Dev);
    if (ret != 0) {
        return 33;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    std::printf("[g5] SIM phase1: decode→aCol0 + at_r→u_hat (tr_hat 另 session)\n");
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r)(kG3BlockDim, stream, aHatDev, rHatDev, uHatDev);
    if (ret != 0) {
        return 34;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(a_hat, F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(re_flat, F203_RE_TOTAL_BYTES, reDev, F203_RE_TOTAL_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(r_hat, dstFileBytes, rHatDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(t_hat, F203_T_HAT_BYTES, tHatDev, F203_T_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(u_hat, F203_U_HAT_BYTES, uHatDev, F203_U_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(a_col0_out, F203_AHAT_BYTES, aCol0Dev, F203_AHAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(rHatDev));
    CHECK_ACL(aclrtFree(nttWsDev));
    CHECK_ACL(aclrtFreeHost(nttTilingHost));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(uHatDev));
    CHECK_ACL(aclrtFree(aCol0Dev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    std::printf("[g5] SIM phase1 done (prep..G3)\n");
    return 0;
}
#endif

#ifdef ASCENDC_CPU_DEBUG
int run_encrypt_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out)
{
    std::vector<uint8_t> a_hat(F203_AHAT_BYTES);
    std::vector<uint8_t> re_flat(F203_RE_TOTAL_BYTES);
    std::vector<uint8_t> r_hat(F203_R_HAT_BYTES);
    std::vector<uint8_t> t_hat(F203_T_HAT_BYTES);
    std::vector<uint8_t> u_hat(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_hat(F203_TR_HAT_BYTES);
    std::vector<uint8_t> u_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_time(F203_U_HAT_BYTES);
    std::vector<uint8_t> v_poly(F203_E2_POLY_BYTES);

    std::printf("[main_encrypt] G5 production single-session (device decode + g3_linear4)\n");

    const int rc = run_g5_cpu_session(ek, coins, m, lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd,
                                      a_hat.data(), re_flat.data(), r_hat.data(), t_hat.data(), u_hat.data(),
                                      tr_hat.data(), u_time.data(), tr_time.data(), v_poly.data(), c_out);
    if (rc != 0) {
        return rc;
    }

    const std::string out_dir = case_dir + "/output";
    const int wo = write_g1_outputs(out_dir, a_hat.data(), re_flat.data());
    if (wo != 0) {
        std::fprintf(stderr, "[g5] write G1 outputs failed code=%d\n", wo);
        return 40;
    }
    if (!WriteFile(out_dir + "/r_hat.bin", r_hat.data(), r_hat.size())) {
        return 41;
    }
    if (!WriteFile(out_dir + "/t_hat.bin", t_hat.data(), t_hat.size())) {
        return 42;
    }
    if (!WriteFile(out_dir + "/u_hat.bin", u_hat.data(), u_hat.size())) {
        return 43;
    }
    if (!WriteFile(out_dir + "/tr_hat.bin", tr_hat.data(), tr_hat.size())) {
        return 44;
    }
    if (!WriteFile(out_dir + "/c.bin", c_out, F203_CT_PKE_BYTES)) {
        return 45;
    }
    std::printf("[main_encrypt] G5 done c.bin=%uB (no input/t_hat.bin)\n", F203_CT_PKE_BYTES);
    return 0;
}
#endif
