/**
 * @file main_kem_enc_g5_run.cpp
 * @brief Alg.20 KEM Encaps：单 session vendor Encrypt G5 + KEM 头（prep_re 融合）。
 */
#include "main_kem_enc_g5_run.hpp"
#include "f203_kem_enc_layout.h"
#include "f203_encrypt_layout.h"
#include "f203_ntt_r_tiling.h"
#include "f203_encrypt_intt_tiling.h"
#include "f203_encrypt_pack_config.hpp"
#include "innerproduct_tiling.h"
#include "f203_encrypt_at_r5_tiling.h"  // G3 合并核 at_r5 维度（INTEGRATION_PLAN §2.3）
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_prep_a_hat.h"
#include "aclrtlaunch_f203_kem_enc_prep_re.h"
#include "aclrtlaunch_f203_encrypt_ntt_r.h"
#include "aclrtlaunch_f203_encrypt_decode_t_hat.h"
#if !defined(F203_FUNCKEY_EXPERIMENT)
#include "aclrtlaunch_f203_encrypt_at_r5.h"   // G3 合并核 at_r5（INTEGRATION_PLAN §2.3 取代旧 g3_linear*/at_r/t_dot_r）
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
extern "C" __global__ __aicore__ void f203_encrypt_prep_a_hat(GM_ADDR rho_gm, GM_ADDR a_hat_gm);
extern "C" __global__ __aicore__ void f203_kem_enc_prep_re(GM_ADDR ek_gm, GM_ADDR seed_d_gm, GM_ADDR K_gm, GM_ADDR m_gm,
                                                         GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                         GM_ADDR tiling);
extern "C" __global__ __aicore__ void f203_encrypt_ntt_r(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_decode_t_hat(GM_ADDR ekGm, GM_ADDR tHatGm, GM_ADDR aCol0Gm);
extern "C" __global__ __aicore__ void f203_encrypt_at_r5(GM_ADDR matM, GM_ADDR rHat, GM_ADDR uTr);
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
int run_g5_cpu_session(const uint8_t *ek, uint32_t seed_d, const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                       const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *a_hat, uint8_t *re_flat,
                       uint8_t *r_hat, uint8_t *t_hat, uint8_t *u_hat, uint8_t *tr_hat, uint8_t *u_time,
                       uint8_t *tr_time, uint8_t *v_poly, uint8_t *c_out, uint8_t *K_out)
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
    uint8_t *seedDGm = (uint8_t *)AscendC::GmAlloc(F203KemEnc::kSeedDBytes);
    uint8_t *KGm = (uint8_t *)AscendC::GmAlloc(F203KemEnc::kSharedSecretBytes);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *coinsGm = (uint8_t *)AscendC::GmAlloc(F203_ENC_COINS_BYTES);
    uint8_t *aHatGm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    uint8_t *prfGm = (uint8_t *)AscendC::GmAlloc(F203_ENCRYPT_PRF_TOTAL_BYTES);
    uint8_t *reGm = (uint8_t *)AscendC::GmAlloc(F203_RE_TOTAL_BYTES);
    uint8_t *shakeTilingGm = (uint8_t *)AscendC::GmAlloc(kShakeTilingBytes);
    uint8_t *rhoGm = ekGm + F203_EK_RHO_OFFSET;
    std::memcpy(ekGm, ek, F203_EK_PKE_BYTES);
    *reinterpret_cast<uint32_t *>(seedDGm) = seed_d;
    std::memcpy(shakeTilingGm, &shakeTiling, kShakeTilingBytes);
    ICPU_RUN_KF(f203_encrypt_prep_a_hat, kAhatBlockDim, rhoGm, aHatGm);
    ICPU_RUN_KF(f203_kem_enc_prep_re, kReBlockDim, ekGm, seedDGm, KGm, mGm, coinsGm, prfGm, reGm, shakeTilingGm);
    std::memcpy(K_out, KGm, F203KemEnc::kSharedSecretBytes);
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

    // G3 合并核 at_r5（INTEGRATION_PLAN §2.3）：host 拼 matM[(j*kP+p)*kN+c]
    //   p<4 取 Â[j,p]（aHatGm[(j*K+p)*N+c]，p=4 取 t̂[j]（tHatGm[j*N+c]），
    //   单 launch 输出 uTr=[û(4 行)|tr̂(1 行)]。替代旧 g3_linear4 同算法 4-合-1 路径。
    uint8_t *matGm = (uint8_t *)AscendC::GmAlloc(at_r5_tiling::kMatBytes);
    uint8_t *uTrGm = (uint8_t *)AscendC::GmAlloc(at_r5_tiling::kOutBytes);
    {
        constexpr int32_t kK_g3 = at_r5_tiling::kK;
        constexpr int32_t kP_g3 = at_r5_tiling::kP;
        constexpr int32_t kN_g3 = at_r5_tiling::kN;
        const int32_t *aSrc = reinterpret_cast<const int32_t *>(aHatGm);
        const int32_t *tSrc = reinterpret_cast<const int32_t *>(tHatGm);
        int32_t *matDst = reinterpret_cast<int32_t *>(matGm);
        for (int32_t j = 0; j < kK_g3; ++j) {
            for (int32_t p = 0; p < kP_g3; ++p) {
                int32_t *dst = matDst + (static_cast<size_t>(j) * kP_g3 + static_cast<size_t>(p)) * kN_g3;
                const int32_t *src = (p < kK_g3) ? (aSrc + (static_cast<size_t>(j) * kK_g3 + static_cast<size_t>(p)) * kN_g3)
                                                  : (tSrc + static_cast<size_t>(j) * kN_g3);
                std::memcpy(dst, src, static_cast<size_t>(kN_g3) * sizeof(int32_t));
            }
        }
    }
    ICPU_RUN_KF(f203_encrypt_at_r5, kG3BlockDim, matGm, rHatGm, uTrGm);
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
    uint8_t *e1Gm = reGm + F203_R_POLYVEC_BYTES;
    uint8_t *e2Gm = reGm + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    ICPU_RUN_KF(f203_encrypt_g4_noise, kG4NoiseBlockDim, uTimeGm, e1Gm, trTimeGm, e2Gm, mGm, vGm);
    ICPU_RUN_KF(f203_encrypt_pack, kPackBlockDim, uTimeGm, vGm, cGm);
    std::memcpy(v_poly, vGm, F203_E2_POLY_BYTES);
    std::memcpy(c_out, cGm, F203_CT_PKE_BYTES);

    AscendC::GmFree(ekGm);
    AscendC::GmFree(seedDGm);
    AscendC::GmFree(KGm);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(shakeTilingGm);
    AscendC::GmFree(rHatGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(aCol0Gm);
    AscendC::GmFree(matGm);
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
/**
 * SIM 全链：单 ACL session prep..pack → c.bin（INTEGRATION_PLAN §4，2026-06-30 P2）。
 *
 * G3：at_r5 合并核 + host 拼 matM（§2.3 病根 2 同步点）。
 * G4：INTT×2 + g4_noise + pack 全 device（decode/pack 为 MIX 占位释 AIV func_key）。
 */
int run_g5_sim_full(const uint8_t *ek, uint32_t seed_d, const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                    const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *c_out, uint8_t *K_out)
{
    using namespace tiling;
    constexpr uint32_t kAhatBlockDim = 2U;
    constexpr uint32_t kReBlockDim = 1U;
    constexpr uint32_t kNttBlockDim = 1U;
    constexpr uint32_t kG3BlockDim = at_r5_tiling::kBlockDim;
    constexpr uint32_t kDecodeBlockDim = 1U;
    constexpr uint32_t kNttMixPass = 3U;
    constexpr uint32_t kInttMixPass = 3U;
    constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, F203_ENCRYPT_PRF_BATCH, 64U, F203_ENCRYPT_PRF_BYTES_PER_POLY, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kReBlockDim;

    TilingData nttTiling{};
    nttTiling.tileLength = static_cast<int32_t>(n);
    nttTiling.kPolys = static_cast<int32_t>(kK);
    nttTiling.mixPass = static_cast<int32_t>(kNttMixPass);

    TilingData inttTiling = nttTiling;
    inttTiling.mixPass = static_cast<int32_t>(kInttMixPass);

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekDev = nullptr;
    uint8_t *seedDDev = nullptr;
    uint8_t *KDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;
    uint8_t *rHatDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    TilingData *nttTilingHost = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *aCol0DevUnused = nullptr;  // decode_t_hat 的第 3 参，保留 dummy buffer（kernel 不影响 t̂ 计算）
    uint8_t *matDev = nullptr;          // at_r5 输入 matM [kK, kP, kN] int32
    uint8_t *uTrDev = nullptr;          // at_r5 输出 [û(4 行) | tr̂(1 行)]
    uint8_t *inttWsDev = nullptr;
    TilingData *inttTilingHost = nullptr;
    uint8_t *uTimeDev = nullptr;
    uint8_t *trPaddedDev = nullptr;
    uint8_t *trTimeDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *cDev = nullptr;

    CHECK_ACL(aclrtMalloc((void **)&ekDev, F203_EK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&seedDDev, F203KemEnc::kSeedDBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&KDev, F203KemEnc::kSharedSecretBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&coinsDev, F203_ENC_COINS_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, F203_ENCRYPT_PRF_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&reDev, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&shakeTilingDev, kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&nttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&tHatDev, F203_T_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aCol0DevUnused, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&matDev, at_r5_tiling::kMatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTrDev, at_r5_tiling::kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&inttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&uTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));

    *nttTilingHost = nttTiling;
    *inttTilingHost = inttTiling;
    CHECK_ACL(aclrtMemcpy(seedDDev, F203KemEnc::kSeedDBytes, &seed_d, F203KemEnc::kSeedDBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    {
        std::vector<uint8_t> inttWsHost(wssize, 0);
        std::memcpy(inttWsHost.data() + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
        std::memcpy(inttWsHost.data() + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
        CHECK_ACL(aclrtMemcpy(inttWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, inttWsHost.data() + LUT_EVEN_STACKED,
                              lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(inttWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, inttWsHost.data() + LUT_ODD_STACKED,
                              lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    }
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTiling, kShakeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::vector<uint8_t> wsHost(wssize, 0);
    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_ntt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_ntt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMemcpy(ekDev, F203_EK_PKE_BYTES, ek, F203_EK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    uint8_t *rhoDev = ekDev + F203_EK_RHO_OFFSET;
    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_a_hat)(kAhatBlockDim, stream, rhoDev, aHatDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] prep_a_hat launch ret=%u\n", ret);
        return 30;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_kem_enc_prep_re)(kReBlockDim, stream, ekDev, seedDDev, KDev, mDev, coinsDev, prfDev,
                                                  reDev, shakeTilingDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] prep_re launch ret=%u\n", ret);
        return 31;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_r)(kNttBlockDim, stream, rHatDev, reDev, nttWsDev, nttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] ntt_r launch ret=%u\n", ret);
        return 32;
    }
    CHECK_ACL(aclrtMemset(aCol0DevUnused, F203_AHAT_BYTES, 0, F203_AHAT_BYTES));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_decode_t_hat)(kDecodeBlockDim, stream, ekDev, tHatDev, aCol0DevUnused);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] decode_t_hat launch ret=%u\n", ret);
        return 33;
    }
    // 同步：等 prep_a_hat/decode_t_hat 完成，否则下面 D2H 拼 matM 时 aHat/tHat 可能未写完（§2.3 病根 2）
    CHECK_ACL(aclrtSynchronizeStream(stream));

#if defined(F203_FUNCKEY_EXPERIMENT)
    // funckey 实验：at_r5 已从 KERNEL_FILES 移除（FUNCKEY=1 时），不 launch；fake [u_hat | tr_hat] = 0。
    // 目的是让 main_encrypt 后续 INTT/G4 仍能跑到 g4_noise probe，观察 g4_noise launch ret。
    std::printf("[funckey-exp] SIM phase1: skip at_r5 (u_hat/tr_hat fake=0)\n");
    CHECK_ACL(aclrtMemset(uTrDev, at_r5_tiling::kOutBytes, 0, at_r5_tiling::kOutBytes));
    CHECK_ACL(aclrtSynchronizeStream(stream));
#else
    // host 拼 matM：D2H aHat/tHat → 按 (j*kP+p)*kN 排布 → H2D matDev（详 INTEGRATION_PLAN §2.3）
    {
        std::vector<uint8_t> aHatHost(F203_AHAT_BYTES);
        std::vector<uint8_t> tHatHost(F203_T_HAT_BYTES);
        CHECK_ACL(aclrtMemcpy(aHatHost.data(), F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(tHatHost.data(), F203_T_HAT_BYTES, tHatDev, F203_T_HAT_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        std::vector<uint8_t> matHost(at_r5_tiling::kMatBytes);
        constexpr int32_t kK_g3 = at_r5_tiling::kK;
        constexpr int32_t kP_g3 = at_r5_tiling::kP;
        constexpr int32_t kN_g3 = at_r5_tiling::kN;
        const int32_t *aSrc = reinterpret_cast<const int32_t *>(aHatHost.data());
        const int32_t *tSrc = reinterpret_cast<const int32_t *>(tHatHost.data());
        int32_t *matDst = reinterpret_cast<int32_t *>(matHost.data());
        for (int32_t j = 0; j < kK_g3; ++j) {
            for (int32_t p = 0; p < kP_g3; ++p) {
                int32_t *dst = matDst + (static_cast<size_t>(j) * kP_g3 + static_cast<size_t>(p)) * kN_g3;
                const int32_t *src = (p < kK_g3) ? (aSrc + (static_cast<size_t>(j) * kK_g3 + static_cast<size_t>(p)) * kN_g3)
                                                  : (tSrc + static_cast<size_t>(j) * kN_g3);
                std::memcpy(dst, src, static_cast<size_t>(kN_g3) * sizeof(int32_t));
            }
        }
        CHECK_ACL(aclrtMemcpy(matDev, at_r5_tiling::kMatBytes, matHost.data(), at_r5_tiling::kMatBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
    }
    std::printf("[g5] SIM: prep+ntt+decode+at_r5 单 session\n");
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r5)(kG3BlockDim, stream, matDev, rHatDev, uTrDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] at_r5 launch ret=%u\n", ret);
        return 34;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
#endif

#if !defined(F203_FUNCKEY_EXPERIMENT)
    // G4 device tail：INTT×2 → g4_noise → pack（同 CPU run_g5_cpu_session 顺序）
    CHECK_ACL(aclrtMemset(trPaddedDev, dstFileBytes, 0, dstFileBytes));
    CHECK_ACL(aclrtMemcpy(trPaddedDev, F203_TR_HAT_BYTES,
                          uTrDev + static_cast<size_t>(F203_U_HAT_BYTES), F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, uTimeDev, uTrDev, inttWsDev, inttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] INTT u launch ret=%u\n", ret);
        return 35;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, trTimeDev, trPaddedDev, inttWsDev,
                                                 inttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] INTT tr launch ret=%u\n", ret);
        return 36;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    uint8_t *e1Dev = reDev + F203_R_POLYVEC_BYTES;
    uint8_t *e2Dev = reDev + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise)(kG4NoiseBlockDim, stream, uTimeDev, e1Dev, trTimeDev, e2Dev,
                                                     mDev, vDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] g4_noise launch ret=%u\n", ret);
        return 37;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_pack)(kPackBlockDim, stream, uTimeDev, vDev, cDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] pack launch ret=%u\n", ret);
        return 38;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(c_out, F203_CT_PKE_BYTES, cDev, F203_CT_PKE_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(K_out, F203KemEnc::kSharedSecretBytes, KDev, F203KemEnc::kSharedSecretBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));
#else
    // funckey 实验：G4 device 路径跳过；c_out 由 caller 的 host fallback 处理
    (void)c_out;
#endif

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(seedDDev));
    CHECK_ACL(aclrtFree(KDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(rHatDev));
    CHECK_ACL(aclrtFree(nttWsDev));
    CHECK_ACL(aclrtFreeHost(nttTilingHost));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(aCol0DevUnused));
    CHECK_ACL(aclrtFree(matDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(inttWsDev));
    CHECK_ACL(aclrtFreeHost(inttTilingHost));
    CHECK_ACL(aclrtFree(uTimeDev));
    CHECK_ACL(aclrtFree(trPaddedDev));
    CHECK_ACL(aclrtFree(trTimeDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    std::printf("[g5] SIM full done (prep..pack device 单 session)\n");
    return 0;
}
#endif

#ifdef ASCENDC_CPU_DEBUG
int run_kem_enc_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, uint32_t seed_d,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out, uint8_t *K_out)
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

    std::printf("[main_kem_enc] Alg.20 Encaps single-session (device m+G + Encrypt G5)\n");

    const int rc = run_g5_cpu_session(ek, seed_d, lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd,
                                      a_hat.data(), re_flat.data(), r_hat.data(), t_hat.data(), u_hat.data(),
                                      tr_hat.data(), u_time.data(), tr_time.data(), v_poly.data(), c_out, K_out);
    if (rc != 0) {
        return rc;
    }

    const std::string out_dir = case_dir + "/output";
    if (!WriteFile(out_dir + "/c.bin", c_out, F203_CT_PKE_BYTES)) {
        return 45;
    }
    if (!WriteFile(out_dir + "/K.bin", K_out, F203KemEnc::kSharedSecretBytes)) {
        return 46;
    }
    std::printf("[main_kem_enc] done c.bin=%uB K.bin=%uB\n", F203_CT_PKE_BYTES, F203KemEnc::kSharedSecretBytes);
    return 0;
}
#endif
