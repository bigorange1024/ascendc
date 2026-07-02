/**
 * @file main_kem_dec_g5_run.cpp
 * @brief Alg.21 KEM Decaps：单 session Phase-D（Decrypt+G）+ Phase-E（Re-Encrypt+FO）。
 */
#include "main_kem_dec_g5_run.hpp"
#include "f203_encrypt_layout.h"
#include "f203_kem_dec_layout.h"
#include "f203_ntt_r_tiling.h"
#include "f203_encrypt_intt_tiling.h"
#include "f203_encrypt_at_r5_tiling.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_decrypt_g4_prep.h"
#include "aclrtlaunch_f203_decrypt_g4_chain_ntt.h"
#include "aclrtlaunch_f203_kem_dec_chain_intt.h"
#include "aclrtlaunch_f203_kem_dec_g.h"
#include "aclrtlaunch_f203_encrypt_prep_a_hat.h"
#include "aclrtlaunch_f203_encrypt_prep_re.h"
#include "aclrtlaunch_f203_encrypt_ntt_r.h"
#include "aclrtlaunch_f203_encrypt_decode_t_hat.h"
#include "aclrtlaunch_f203_encrypt_at_r5.h"
#include "aclrtlaunch_f203_encrypt_intt.h"
#include "aclrtlaunch_f203_encrypt_g4_noise.h"
#include "aclrtlaunch_f203_kem_dec_pack.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" __global__ __aicore__ void f203_decrypt_g4_prep(GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm,
                                                             GM_ADDR sHatGm);
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(GM_ADDR uGm, GM_ADDR sHatGm, GM_ADDR uHatGm,
                                                                   GM_ADDR wHatGm, GM_ADDR wPaddedGm,
                                                                   GM_ADDR nttWsGm, TilingData tiling);
extern "C" __global__ __aicore__ void f203_kem_dec_chain_intt(GM_ADDR vGm, GM_ADDR wPaddedGm, GM_ADDR wTimeGm,
                                                                GM_ADDR mGm, GM_ADDR inttWsGm, TilingData tiling);
extern "C" __global__ __aicore__ void f203_kem_dec_g(GM_ADDR m_gm, GM_ADDR h_gm, GM_ADDR Kprime_gm, GM_ADDR coins_gm);
extern "C" __global__ __aicore__ void f203_encrypt_prep_a_hat(GM_ADDR rho_gm, GM_ADDR a_hat_gm);
extern "C" __global__ __aicore__ void f203_encrypt_prep_re(GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                           GM_ADDR tiling);
extern "C" __global__ __aicore__ void f203_encrypt_ntt_r(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_decode_t_hat(GM_ADDR ekGm, GM_ADDR tHatGm, GM_ADDR aCol0Gm);
extern "C" __global__ __aicore__ void f203_encrypt_at_r5(GM_ADDR matM, GM_ADDR rHat, GM_ADDR uTr);
extern "C" __global__ __aicore__ void f203_encrypt_intt(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern "C" __global__ __aicore__ void f203_encrypt_g4_noise(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm,
                                                          GM_ADDR mGm, GM_ADDR vGm);
extern "C" __global__ __aicore__ void f203_kem_dec_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cPrimeGm, GM_ADDR cInGm,
                                                        GM_ADDR zGm, GM_ADDR KprimeGm, GM_ADDR KoutGm);
extern volatile int g_f203_decrypt_g4_chain_ntt_mix_pass;
extern volatile int g_f203_decrypt_g4_chain_intt_mix_pass;
extern volatile int g_f203_ntt_r_mix_pass;
extern volatile int g_f203_intt_mix_pass;
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

bool WriteFile(const std::string &filePath, const void *buffer, size_t size);

#ifndef ASCENDC_CPU_DEBUG
int run_g5_sim_full(const uint8_t *ek, const uint8_t *coins, const uint8_t *m, const uint8_t *lut_ntt_even,
                    const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                    uint8_t *c_out);
#endif

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

constexpr uint32_t kDkPkeBytes = F203KemDec::kEkOffset;
constexpr uint32_t kUPolyvecBytes = F203_ENCRYPT_K * F203_ENCRYPT_N * static_cast<uint32_t>(sizeof(int32_t));
constexpr uint32_t kVPolyBytes = F203_ENCRYPT_N * static_cast<uint32_t>(sizeof(int32_t));
constexpr uint32_t kSHatBytes = kUPolyvecBytes;
constexpr uint32_t kWHatBytes = kVPolyBytes;

constexpr uint32_t kG4BlockDim = 1U;
constexpr uint32_t kKemGBlockDim = 1U;
constexpr uint32_t kG4MixPass = 3U;
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

static void fill_ntt_ws(uint8_t *ws, size_t wsBytes, const uint8_t *lut_even, const uint8_t *lut_odd)
{
    std::memset(ws, 0, wsBytes);
    std::memcpy(ws + tiling::LUT_EVEN_STACKED, lut_even, tiling::lutEvenOddFileBytes);
    std::memcpy(ws + tiling::LUT_ODD_STACKED, lut_odd, tiling::lutEvenOddFileBytes);
}

static void build_at_r5_mat(const uint8_t *a_hat, const uint8_t *t_hat, uint8_t *mat_out)
{
    constexpr int32_t kK_g3 = at_r5_tiling::kK;
    constexpr int32_t kP_g3 = at_r5_tiling::kP;
    constexpr int32_t kN_g3 = at_r5_tiling::kN;
    const int32_t *aSrc = reinterpret_cast<const int32_t *>(a_hat);
    const int32_t *tSrc = reinterpret_cast<const int32_t *>(t_hat);
    int32_t *matDst = reinterpret_cast<int32_t *>(mat_out);
    for (int32_t j = 0; j < kK_g3; ++j) {
        for (int32_t p = 0; p < kP_g3; ++p) {
            int32_t *dst = matDst + (static_cast<size_t>(j) * kP_g3 + static_cast<size_t>(p)) * kN_g3;
            const int32_t *src = (p < kK_g3) ? (aSrc + (static_cast<size_t>(j) * kK_g3 + static_cast<size_t>(p)) * kN_g3)
                                              : (tSrc + static_cast<size_t>(j) * kN_g3);
            std::memcpy(dst, src, static_cast<size_t>(kN_g3) * sizeof(int32_t));
        }
    }
}

int run_decaps_session(const uint8_t *dk_kem, const uint8_t *c_in, const uint8_t *lut_ntt_even,
                       const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                       uint8_t *K_out)
{
    const uint8_t *dk_pke = dk_kem + F203KemDec::kDkPkeOffset;
    const uint8_t *ek = dk_kem + F203KemDec::kEkOffset;
    const uint8_t *h = dk_kem + F203KemDec::kHOffset;
    const uint8_t *z = dk_kem + F203KemDec::kZOffset;

    using namespace tiling;
    TilingData g4Tiling{};
    g4Tiling.tileLength = static_cast<int32_t>(n);
    g4Tiling.kPolys = static_cast<int32_t>(kK);
    g4Tiling.mixPass = static_cast<int32_t>(kG4MixPass);

    TilingData nttTiling = g4Tiling;
    TilingData inttTiling = g4Tiling;
    inttTiling.mixPass = static_cast<int32_t>(kInttMixPass);

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
    shakeTiling.blockDim = kReBlockDim;

#ifdef ASCENDC_CPU_DEBUG
    g_f203_decrypt_g4_chain_ntt_mix_pass = static_cast<int>(kG4MixPass);
    g_f203_decrypt_g4_chain_intt_mix_pass = static_cast<int>(kG4MixPass);
    g_f203_ntt_r_mix_pass = static_cast<int>(kNttMixPass);
    g_f203_intt_mix_pass = static_cast<int>(kInttMixPass);

    uint8_t *dkGm = (uint8_t *)AscendC::GmAlloc(kDkPkeBytes);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    uint8_t *hGm = (uint8_t *)AscendC::GmAlloc(F203KemDec::kHashBytes);
    uint8_t *zGm = (uint8_t *)AscendC::GmAlloc(F203KemDec::kHashBytes);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *KprimeGm = (uint8_t *)AscendC::GmAlloc(F203KemDec::kSharedSecretBytes);
    uint8_t *coinsGm = (uint8_t *)AscendC::GmAlloc(F203_ENC_COINS_BYTES);
    uint8_t *KoutGm = (uint8_t *)AscendC::GmAlloc(F203KemDec::kSharedSecretBytes);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(kUPolyvecBytes);
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(kVPolyBytes);
    uint8_t *sHatGm = (uint8_t *)AscendC::GmAlloc(kSHatBytes);
    uint8_t *uHatGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wHatGm = (uint8_t *)AscendC::GmAlloc(kWHatBytes);
    uint8_t *wPaddedGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *nttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *inttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);

    std::memcpy(dkGm, dk_pke, kDkPkeBytes);
    std::memcpy(cGm, c_in, F203_CT_PKE_BYTES);
    std::memcpy(hGm, h, F203KemDec::kHashBytes);
    std::memcpy(zGm, z, F203KemDec::kHashBytes);
    fill_ntt_ws(nttWsGm, wssize, lut_ntt_even, lut_ntt_odd);
    fill_ntt_ws(inttWsGm, wssize, lut_intt_even, lut_intt_odd);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_decrypt_g4_prep, kG4BlockDim, dkGm, cGm, uGm, vGm, sHatGm);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_decrypt_g4_chain_ntt, kG4BlockDim, uGm, sHatGm, uHatGm, wHatGm, wPaddedGm, nttWsGm, g4Tiling);
    ICPU_RUN_KF(f203_kem_dec_chain_intt, kG4BlockDim, vGm, wPaddedGm, wTimeGm, mGm, inttWsGm, g4Tiling);
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_kem_dec_g, kKemGBlockDim, mGm, hGm, KprimeGm, coinsGm);

    uint8_t *ekGm = (uint8_t *)AscendC::GmAlloc(F203_EK_PKE_BYTES);
    uint8_t *aHatGm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    uint8_t *prfGm = (uint8_t *)AscendC::GmAlloc(F203_ENCRYPT_PRF_TOTAL_BYTES);
    uint8_t *reGm = (uint8_t *)AscendC::GmAlloc(F203_RE_TOTAL_BYTES);
    uint8_t *shakeTilingGm = (uint8_t *)AscendC::GmAlloc(kShakeTilingBytes);
    uint8_t *rHatGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *nttEncWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *tHatGm = (uint8_t *)AscendC::GmAlloc(F203_T_HAT_BYTES);
    uint8_t *aCol0Gm = (uint8_t *)AscendC::GmAlloc(F203_AHAT_BYTES);
    uint8_t *matGm = (uint8_t *)AscendC::GmAlloc(at_r5_tiling::kMatBytes);
    uint8_t *uTrGm = (uint8_t *)AscendC::GmAlloc(at_r5_tiling::kOutBytes);
    uint8_t *inttEncWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *uTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *trPaddedGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *trTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *vPolyGm = (uint8_t *)AscendC::GmAlloc(F203_E2_POLY_BYTES);
    uint8_t *cPrimeGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);

    std::memcpy(ekGm, ek, F203_EK_PKE_BYTES);
    std::memcpy(shakeTilingGm, &shakeTiling, kShakeTilingBytes);
    fill_ntt_ws(nttEncWsGm, wssize, lut_ntt_even, lut_ntt_odd);
    fill_ntt_ws(inttEncWsGm, wssize, lut_intt_even, lut_intt_odd);
    std::memset(aCol0Gm, 0, F203_AHAT_BYTES);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *rhoGm = ekGm + F203_EK_RHO_OFFSET;
    ICPU_RUN_KF(f203_encrypt_prep_a_hat, kAhatBlockDim, rhoGm, aHatGm);
    ICPU_RUN_KF(f203_encrypt_prep_re, kReBlockDim, coinsGm, prfGm, reGm, shakeTilingGm);

    g_f203_ntt_r_mix_pass = static_cast<int>(kNttMixPass);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_encrypt_ntt_r, kNttBlockDim, rHatGm, reGm, nttEncWsGm, nttTiling);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_encrypt_decode_t_hat, kDecodeBlockDim, ekGm, tHatGm, aCol0Gm);

    std::vector<uint8_t> aHatHost(F203_AHAT_BYTES);
    std::vector<uint8_t> tHatHost(F203_T_HAT_BYTES);
    std::memcpy(aHatHost.data(), aHatGm, F203_AHAT_BYTES);
    std::memcpy(tHatHost.data(), tHatGm, F203_T_HAT_BYTES);
    build_at_r5_mat(aHatHost.data(), tHatHost.data(), matGm);

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_encrypt_at_r5, kG3BlockDim, matGm, rHatGm, uTrGm);

    std::memset(trPaddedGm, 0, dstFileBytes);
    std::memcpy(trPaddedGm, uTrGm + F203_U_HAT_BYTES, F203_TR_HAT_BYTES);
    ICPU_RUN_KF(f203_encrypt_intt, kInttBlockDim, uTimeGm, uTrGm, inttEncWsGm, inttTiling);
    ICPU_RUN_KF(f203_encrypt_intt, kInttBlockDim, trTimeGm, trPaddedGm, inttEncWsGm, inttTiling);

    uint8_t *e1Gm = reGm + F203_R_POLYVEC_BYTES;
    uint8_t *e2Gm = reGm + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_encrypt_g4_noise, kG4NoiseBlockDim, uTimeGm, e1Gm, trTimeGm, e2Gm, mGm, vPolyGm);
    ICPU_RUN_KF(f203_kem_dec_pack, kPackBlockDim, uTimeGm, vPolyGm, cPrimeGm, cGm, zGm, KprimeGm, KoutGm);

    std::memcpy(K_out, KoutGm, F203KemDec::kSharedSecretBytes);

    AscendC::GmFree(dkGm);
    AscendC::GmFree(cGm);
    AscendC::GmFree(hGm);
    AscendC::GmFree(zGm);
    AscendC::GmFree(mGm);
    AscendC::GmFree(KprimeGm);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(KoutGm);
    AscendC::GmFree(uGm);
    AscendC::GmFree(vGm);
    AscendC::GmFree(sHatGm);
    AscendC::GmFree(uHatGm);
    AscendC::GmFree(wHatGm);
    AscendC::GmFree(wPaddedGm);
    AscendC::GmFree(wTimeGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(inttWsGm);
    AscendC::GmFree(ekGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(shakeTilingGm);
    AscendC::GmFree(rHatGm);
    AscendC::GmFree(nttEncWsGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(aCol0Gm);
    AscendC::GmFree(matGm);
    AscendC::GmFree(uTrGm);
    AscendC::GmFree(inttEncWsGm);
    AscendC::GmFree(uTimeGm);
    AscendC::GmFree(trPaddedGm);
    AscendC::GmFree(trTimeGm);
    AscendC::GmFree(vPolyGm);
    AscendC::GmFree(cPrimeGm);
    return 0;
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *cDev = nullptr;
    uint8_t *dkDev = nullptr;
    uint8_t *hDev = nullptr;
    uint8_t *zDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *KprimeDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *KoutDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *sHatDev = nullptr;
    uint8_t *uHatDev = nullptr;
    uint8_t *wHatDev = nullptr;
    uint8_t *wPaddedDev = nullptr;
    uint8_t *wTimeDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    uint8_t *inttWsDev = nullptr;
    TilingData *g4TilingHost = nullptr;

    uint8_t *ekDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;
    uint8_t *rHatDev = nullptr;
    uint8_t *nttEncWsDev = nullptr;
    TilingData *nttTilingHost = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *aCol0Dev = nullptr;
    uint8_t *matDev = nullptr;
    uint8_t *uTrDev = nullptr;
    uint8_t *inttEncWsDev = nullptr;
    TilingData *inttTilingHost = nullptr;
    uint8_t *uTimeDev = nullptr;
    uint8_t *trPaddedDev = nullptr;
    uint8_t *trTimeDev = nullptr;
    uint8_t *vPolyDev = nullptr;
    uint8_t *cPrimeDev = nullptr;

    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dkDev, kDkPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&hDev, F203KemDec::kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&zDev, F203KemDec::kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&KprimeDev, F203KemDec::kSharedSecretBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&coinsDev, F203_ENC_COINS_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&KoutDev, F203KemDec::kSharedSecretBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, kUPolyvecBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, kVPolyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sHatDev, kSHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wHatDev, kWHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&g4TilingHost, sizeof(TilingData)));
    *g4TilingHost = g4Tiling;

    CHECK_ACL(aclrtMalloc((void **)&ekDev, F203_EK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, F203_ENCRYPT_PRF_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&reDev, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&shakeTilingDev, kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttEncWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&nttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&tHatDev, F203_T_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aCol0Dev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&matDev, at_r5_tiling::kMatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTrDev, at_r5_tiling::kOutBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttEncWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&inttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&uTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vPolyDev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cPrimeDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));

    *nttTilingHost = nttTiling;
    *inttTilingHost = inttTiling;

    CHECK_ACL(aclrtMemcpy(cDev, F203_CT_PKE_BYTES, c_in, F203_CT_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, kDkPkeBytes, dk_pke, kDkPkeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, F203KemDec::kHashBytes, h, F203KemDec::kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, F203KemDec::kHashBytes, z, F203KemDec::kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ekDev, F203_EK_PKE_BYTES, ek, F203_EK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTiling, kShakeTilingBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    {
        std::vector<uint8_t> wsHost(wssize, 0);
        fill_ntt_ws(wsHost.data(), wssize, lut_ntt_even, lut_ntt_odd);
        CHECK_ACL(aclrtMemcpy(nttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(nttEncWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        fill_ntt_ws(wsHost.data(), wssize, lut_intt_even, lut_intt_odd);
        CHECK_ACL(aclrtMemcpy(inttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(inttEncWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    }

    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_prep)(kG4BlockDim, stream, dkDev, cDev, uDev, vDev, sHatDev);
    if (ret != 0) {
        return 30;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_chain_ntt)(kG4BlockDim, stream, uDev, sHatDev, uHatDev, wHatDev,
                                                         wPaddedDev, nttWsDev, g4TilingHost);
    if (ret != 0) {
        return 31;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_kem_dec_chain_intt)(kG4BlockDim, stream, vDev, wPaddedDev, wTimeDev, mDev,
                                                       inttWsDev, g4TilingHost);
    if (ret != 0) {
        return 32;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_kem_dec_g)(kKemGBlockDim, stream, mDev, hDev, KprimeDev, coinsDev);
    if (ret != 0) {
        return 33;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

#ifndef ASCENDC_CPU_DEBUG
    // SIM 诊断：Phase-D 结束后 dump m'/K'/coins（定位 c'≠c 分歧点）
    {
        std::vector<uint8_t> mHost(F203_MSG_BYTES), kpHost(F203KemDec::kSharedSecretBytes),
            coinsHost(F203_ENC_COINS_BYTES);
        CHECK_ACL(aclrtMemcpy(mHost.data(), F203_MSG_BYTES, mDev, F203_MSG_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(kpHost.data(), F203KemDec::kSharedSecretBytes, KprimeDev,
                              F203KemDec::kSharedSecretBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(coinsHost.data(), F203_ENC_COINS_BYTES, coinsDev, F203_ENC_COINS_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile("./output/dbg_m_prime.bin", mHost.data(), mHost.size());
        WriteFile("./output/dbg_K_prime.bin", kpHost.data(), kpHost.size());
        WriteFile("./output/dbg_coins.bin", coinsHost.data(), coinsHost.size());

        // 默认（单库单 session 修复后）：不在此 aclFinalize，直接落到下方 Phase-E（Re-Encrypt + 设备 FO）。
        // KEM_DECAPS_SIM_2SESSION=1 = 非默认回退：保留原双库时代的两段 session 排障路径
        //   （aclFinalize 后 fresh run_g5_sim_full 重加密 + host memcmp 取 K'）。
        //   单库合并（decrypt+encrypt 同 func_key 空间）已消除双库单 session c' 污染，故默认走单 session。
        const char *w2s = std::getenv("KEM_DECAPS_SIM_2SESSION");
        if (w2s != nullptr && w2s[0] == '1') {
            // Phase-D 完成：释放 decrypt 专用 GM，避免长 session 内与 Phase-E 互相干扰（对齐 alg14 单 session 布局）
            CHECK_ACL(aclrtFree(dkDev));
        CHECK_ACL(aclrtFree(uDev));
        CHECK_ACL(aclrtFree(vDev));
        CHECK_ACL(aclrtFree(sHatDev));
        CHECK_ACL(aclrtFree(uHatDev));
        CHECK_ACL(aclrtFree(wHatDev));
        CHECK_ACL(aclrtFree(wPaddedDev));
        CHECK_ACL(aclrtFree(wTimeDev));
        CHECK_ACL(aclrtFree(nttWsDev));
        CHECK_ACL(aclrtFree(inttWsDev));
        dkDev = nullptr;
        uDev = vDev = sHatDev = uHatDev = wHatDev = wPaddedDev = wTimeDev = nttWsDev = inttWsDev = nullptr;

        // 重载 Phase-E workspace LUT（与 alg14/alg20 SIM 一致：按 offset 写入）
        {
            std::vector<uint8_t> inttWsHost(wssize, 0);
            std::memcpy(inttWsHost.data() + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
            std::memcpy(inttWsHost.data() + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
            CHECK_ACL(aclrtMemcpy(inttEncWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes,
                                  inttWsHost.data() + LUT_EVEN_STACKED, lutEvenOddFileBytes,
                                  ACL_MEMCPY_HOST_TO_DEVICE));
            CHECK_ACL(aclrtMemcpy(inttEncWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes,
                                  inttWsHost.data() + LUT_ODD_STACKED, lutEvenOddFileBytes,
                                  ACL_MEMCPY_HOST_TO_DEVICE));
            std::vector<uint8_t> nttWsHost(wssize, 0);
            std::memcpy(nttWsHost.data() + LUT_EVEN_STACKED, lut_ntt_even, lutEvenOddFileBytes);
            std::memcpy(nttWsHost.data() + LUT_ODD_STACKED, lut_ntt_odd, lutEvenOddFileBytes);
            CHECK_ACL(aclrtMemcpy(nttEncWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes,
                                  nttWsHost.data() + LUT_EVEN_STACKED, lutEvenOddFileBytes,
                                  ACL_MEMCPY_HOST_TO_DEVICE));
            CHECK_ACL(aclrtMemcpy(nttEncWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes,
                                  nttWsHost.data() + LUT_ODD_STACKED, lutEvenOddFileBytes,
                                  ACL_MEMCPY_HOST_TO_DEVICE));
        }
        // 重新 H2D m/coins，确保 Phase-E prep_re 读到 Phase-D 最终值
        CHECK_ACL(aclrtMemcpy(mDev, F203_MSG_BYTES, mHost.data(), F203_MSG_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
        CHECK_ACL(aclrtMemcpy(coinsDev, F203_ENC_COINS_BYTES, coinsHost.data(), F203_ENC_COINS_BYTES,
                              ACL_MEMCPY_HOST_TO_DEVICE));

        // CAModel 对 Decrypt+Encrypt 超长单 session 存在状态污染：Phase-D/K1 已验证 max=0，
        // 因此首版 SIM 正确性采用 fresh alg14 G5 session 重加密，先验证合法密文路径。
        CHECK_ACL(aclrtFree(cDev));
        CHECK_ACL(aclrtFree(hDev));
        CHECK_ACL(aclrtFree(zDev));
        CHECK_ACL(aclrtFree(mDev));
        CHECK_ACL(aclrtFree(KprimeDev));
        CHECK_ACL(aclrtFree(coinsDev));
        CHECK_ACL(aclrtFree(KoutDev));
        CHECK_ACL(aclrtFreeHost(g4TilingHost));
        CHECK_ACL(aclrtFree(ekDev));
        CHECK_ACL(aclrtFree(aHatDev));
        CHECK_ACL(aclrtFree(prfDev));
        CHECK_ACL(aclrtFree(reDev));
        CHECK_ACL(aclrtFree(shakeTilingDev));
        CHECK_ACL(aclrtFree(rHatDev));
        CHECK_ACL(aclrtFree(nttEncWsDev));
        CHECK_ACL(aclrtFreeHost(nttTilingHost));
        CHECK_ACL(aclrtFree(tHatDev));
        CHECK_ACL(aclrtFree(aCol0Dev));
        CHECK_ACL(aclrtFree(matDev));
        CHECK_ACL(aclrtFree(uTrDev));
        CHECK_ACL(aclrtFree(inttEncWsDev));
        CHECK_ACL(aclrtFreeHost(inttTilingHost));
        CHECK_ACL(aclrtFree(uTimeDev));
        CHECK_ACL(aclrtFree(trPaddedDev));
        CHECK_ACL(aclrtFree(trTimeDev));
        CHECK_ACL(aclrtFree(vPolyDev));
        CHECK_ACL(aclrtFree(cPrimeDev));
        CHECK_ACL(aclrtDestroyStream(stream));
        CHECK_ACL(aclrtResetDevice(deviceId));
        CHECK_ACL(aclFinalize());

        std::vector<uint8_t> cFresh(F203KemDec::kCtBytes);
        const int encRc = run_g5_sim_full(ek, coinsHost.data(), mHost.data(), lut_ntt_even, lut_ntt_odd,
                                          lut_intt_even, lut_intt_odd, cFresh.data());
        if (encRc != 0) {
            return 60 + encRc;
        }
        WriteFile("./output/dbg_c_prime.bin", cFresh.data(), cFresh.size());
        if (std::memcmp(cFresh.data(), c_in, F203KemDec::kCtBytes) != 0) {
            std::fprintf(stderr, "[kem_decaps] fresh encrypt c' != c\n");
            return 61;
        }
            std::memcpy(K_out, kpHost.data(), F203KemDec::kSharedSecretBytes);
            std::printf("[kem_decaps] SIM 2-session (fallback) valid path done\n");
            return 0;
        }
        // 默认继续单 session Phase-E：mDev/coinsDev 仍持 Phase-D 输出，LUT workspace 已装载。
        std::printf("[kem_decaps] SIM single-session Phase-E (Re-Encrypt + device FO)\n");
    }
#endif

    uint8_t *rhoDev = ekDev + F203_EK_RHO_OFFSET;
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_a_hat)(kAhatBlockDim, stream, rhoDev, aHatDev);
    if (ret != 0) {
        return 34;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_re)(kReBlockDim, stream, coinsDev, prfDev, reDev, shakeTilingDev);
    if (ret != 0) {
        return 35;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_r)(kNttBlockDim, stream, rHatDev, reDev, nttEncWsDev, nttTilingHost);
    if (ret != 0) {
        return 36;
    }
    CHECK_ACL(aclrtMemset(aCol0Dev, F203_AHAT_BYTES, 0, F203_AHAT_BYTES));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_decode_t_hat)(kDecodeBlockDim, stream, ekDev, tHatDev, aCol0Dev);
    if (ret != 0) {
        return 37;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    {
        std::vector<uint8_t> aHatHost(F203_AHAT_BYTES);
        std::vector<uint8_t> tHatHost(F203_T_HAT_BYTES);
        CHECK_ACL(aclrtMemcpy(aHatHost.data(), F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(tHatHost.data(), F203_T_HAT_BYTES, tHatDev, F203_T_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
        std::vector<uint8_t> matHost(at_r5_tiling::kMatBytes);
        build_at_r5_mat(aHatHost.data(), tHatHost.data(), matHost.data());
        CHECK_ACL(aclrtMemcpy(matDev, at_r5_tiling::kMatBytes, matHost.data(), at_r5_tiling::kMatBytes,
                              ACL_MEMCPY_HOST_TO_DEVICE));
    }

    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r5)(kG3BlockDim, stream, matDev, rHatDev, uTrDev);
    if (ret != 0) {
        return 38;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemset(trPaddedDev, dstFileBytes, 0, dstFileBytes));
    CHECK_ACL(aclrtMemcpy(trPaddedDev, F203_TR_HAT_BYTES, uTrDev + F203_U_HAT_BYTES, F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, uTimeDev, uTrDev, inttEncWsDev, inttTilingHost);
    if (ret != 0) {
        return 39;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, trTimeDev, trPaddedDev, inttEncWsDev,
                                                 inttTilingHost);
    if (ret != 0) {
        return 40;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    uint8_t *e1Dev = reDev + F203_R_POLYVEC_BYTES;
    uint8_t *e2Dev = reDev + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES;
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise)(kG4NoiseBlockDim, stream, uTimeDev, e1Dev, trTimeDev, e2Dev, mDev,
                                                     vPolyDev);
    if (ret != 0) {
        return 41;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_kem_dec_pack)(kPackBlockDim, stream, uTimeDev, vPolyDev, cPrimeDev, cDev, zDev,
                                                 KprimeDev, KoutDev);
    if (ret != 0) {
        return 42;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(K_out, F203KemDec::kSharedSecretBytes, KoutDev, F203KemDec::kSharedSecretBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));

#ifndef ASCENDC_CPU_DEBUG
    {
        std::vector<uint8_t> cpHost(F203KemDec::kCtBytes);
        CHECK_ACL(aclrtMemcpy(cpHost.data(), F203KemDec::kCtBytes, cPrimeDev, F203KemDec::kCtBytes,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile("./output/dbg_c_prime.bin", cpHost.data(), cpHost.size());
    }
#endif

    // 调试：dump 中间量以定位 SIM 分歧点（KEM_DECAPS_DEBUG=1）
    if (const char *dbg = std::getenv("KEM_DECAPS_DEBUG"); dbg != nullptr && dbg[0] == '1') {
        std::vector<uint8_t> mHost(F203_MSG_BYTES), kpHost(F203KemDec::kSharedSecretBytes),
            coinsHost(F203_ENC_COINS_BYTES), cpHost(F203KemDec::kCtBytes);
        CHECK_ACL(aclrtMemcpy(mHost.data(), F203_MSG_BYTES, mDev, F203_MSG_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(kpHost.data(), F203KemDec::kSharedSecretBytes, KprimeDev,
                              F203KemDec::kSharedSecretBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(coinsHost.data(), F203_ENC_COINS_BYTES, coinsDev, F203_ENC_COINS_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(cpHost.data(), F203KemDec::kCtBytes, cPrimeDev, F203KemDec::kCtBytes,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile("./output/dbg_m_prime.bin", mHost.data(), mHost.size());
        WriteFile("./output/dbg_K_prime.bin", kpHost.data(), kpHost.size());
        WriteFile("./output/dbg_coins.bin", coinsHost.data(), coinsHost.size());
        WriteFile("./output/dbg_c_prime.bin", cpHost.data(), cpHost.size());
    }

    CHECK_ACL(aclrtFree(cDev));
    if (dkDev != nullptr) {
        CHECK_ACL(aclrtFree(dkDev));
    }
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(KprimeDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(KoutDev));
    if (uDev != nullptr) {
        CHECK_ACL(aclrtFree(uDev));
    }
    if (vDev != nullptr) {
        CHECK_ACL(aclrtFree(vDev));
    }
    if (sHatDev != nullptr) {
        CHECK_ACL(aclrtFree(sHatDev));
    }
    if (uHatDev != nullptr) {
        CHECK_ACL(aclrtFree(uHatDev));
    }
    if (wHatDev != nullptr) {
        CHECK_ACL(aclrtFree(wHatDev));
    }
    if (wPaddedDev != nullptr) {
        CHECK_ACL(aclrtFree(wPaddedDev));
    }
    if (wTimeDev != nullptr) {
        CHECK_ACL(aclrtFree(wTimeDev));
    }
    if (nttWsDev != nullptr) {
        CHECK_ACL(aclrtFree(nttWsDev));
    }
    if (inttWsDev != nullptr) {
        CHECK_ACL(aclrtFree(inttWsDev));
    }
    CHECK_ACL(aclrtFreeHost(g4TilingHost));
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(rHatDev));
    CHECK_ACL(aclrtFree(nttEncWsDev));
    CHECK_ACL(aclrtFreeHost(nttTilingHost));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(aCol0Dev));
    CHECK_ACL(aclrtFree(matDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(inttEncWsDev));
    CHECK_ACL(aclrtFreeHost(inttTilingHost));
    CHECK_ACL(aclrtFree(uTimeDev));
    CHECK_ACL(aclrtFree(trPaddedDev));
    CHECK_ACL(aclrtFree(trTimeDev));
    CHECK_ACL(aclrtFree(vPolyDev));
    CHECK_ACL(aclrtFree(cPrimeDev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    std::printf("[kem_decaps] SIM full done\n");
    return 0;
#endif
}

}  // namespace

#ifdef ASCENDC_CPU_DEBUG
int run_kem_decaps_cpu_full(const std::string &case_dir, const uint8_t *dk_kem, const uint8_t *c_in,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *K_out)
{
    std::printf("[main_kem_decaps] Alg.21 Decaps single-session (Decrypt+G+Re-Encrypt+FO)\n");
    const int rc = run_decaps_session(dk_kem, c_in, lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd, K_out);
    if (rc != 0) {
        return rc;
    }
    const std::string out_dir = case_dir + "/output";
    if (!WriteFile(out_dir + "/K.bin", K_out, F203KemDec::kSharedSecretBytes)) {
        return 50;
    }
    std::printf("[main_kem_decaps] done K.bin=%uB\n", F203KemDec::kSharedSecretBytes);
    return 0;
}
#endif

#ifndef ASCENDC_CPU_DEBUG
int run_kem_decaps_sim_full(const uint8_t *dk_kem, const uint8_t *c_in, const uint8_t *lut_ntt_even,
                            const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                            uint8_t *K_out)
{
    return run_decaps_session(dk_kem, c_in, lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd, K_out);
}
#endif
