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
// G3 单 launch 合并核 at_r5：一次出 [û|tr̂]。SIM AIV 核压到 ≤5 使 at_r5 落 func_key≤4（病根门禁）。
#include "aclrtlaunch_f203_encrypt_at_r5.h"
#include "aclrtlaunch_f203_encrypt_intt.h"
#include "aclrtlaunch_f203_encrypt_g4_noise.h"
#include "aclrtlaunch_f203_encrypt_g4_noise_ws.h"
#include "aclrtlaunch_f203_encrypt_pack.h"
#include "f203_encrypt_g4_ws_layout.hpp"
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
#include <cstdlib>
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

#ifdef ASCENDC_CPU_DEBUG
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
/**
 * G4 tail：INTT(û)→INTT(tr̂)→g4_noise(u+e₁，v=tr+e₂+μ)→pack→c。独立 ACL session。
 *
 * 章法（逐步调试）：本函数在每个关键阶段后 D2H 落盘中间量，供 scripts/verify_g4_tail.py
 * 与 Python 金标准 golden_*（CPU 全链 max=0，故 golden 可信）逐阶段对拍，定位
 * INTT / noise / pack 哪一步先对不上，再判断是计算错还是 tiling/搬运错。
 *
 * @param out_dir          output/ 目录，落 c.bin 与（dump 时）sim_*.bin
 * @param u_tr_host        [u_hat(K*N) | tr_hat(N)] 拼接，NTT 域，INTT 输入
 * @param re_flat          [r | e1 | e2]，tail 仅读 e1/e2（noise 输入）
 * @param m                32B 明文，消息嵌入用
 * @param inttTiling       INTT tiling（mixPass=kInttMixPass）
 * @param c_out            输出密文 1568B
 * @param dump_intermediate 为 true 时落盘 sim_u_time/sim_tr_time/sim_u_noisy/sim_v
 *
 * 供 run_encrypt_g5_sim_full（全链）与 run_encrypt_g4_tail_only_sim（tail-only 快跑）复用。
 */
static int run_g4_tail_sim(const std::string &out_dir, const uint8_t *u_tr_host, const uint8_t *re_flat,
                           const uint8_t *m, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                           const TilingData &inttTiling, uint8_t *c_out, bool dump_intermediate)
{
    using namespace tiling;
    int32_t deviceId = 0;
    aclrtStream stream = nullptr;
    CHECK_ACL(aclInit(nullptr));
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *reDevB = nullptr;
    uint8_t *mDevB = nullptr;
    uint8_t *uTrDevB = nullptr;
    uint8_t *uTimeDevB = nullptr;
    uint8_t *trPaddedDevB = nullptr;
    uint8_t *trTimeDevB = nullptr;
    uint8_t *inttWsDevB = nullptr;
    uint8_t *vDevB = nullptr;
    uint8_t *cDevB = nullptr;
    uint8_t *wsDevB = nullptr;
    TilingData *inttTilingHostB = nullptr;

    CHECK_ACL(aclrtMalloc((void **)&reDevB, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDevB, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTrDevB, kUTrBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTimeDevB, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trPaddedDevB, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trTimeDevB, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDevB, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDevB, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDevB, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wsDevB, f203_g4_ws::kTotalBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&inttTilingHostB, sizeof(TilingData)));
    *inttTilingHostB = inttTiling;

    std::vector<uint8_t> wsHost(wssize, 0);
    CHECK_ACL(aclrtMemcpy(reDevB, F203_RE_TOTAL_BYTES, re_flat, F203_RE_TOTAL_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDevB, F203_MSG_BYTES, m, F203_MSG_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uTrDevB, kUTrBytes, u_tr_host, kUTrBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(inttWsDevB + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(inttWsDevB + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMemset(trPaddedDevB, dstFileBytes, 0, dstFileBytes));
    CHECK_ACL(aclrtMemcpy(trPaddedDevB, F203_TR_HAT_BYTES, uTrDevB + F203_U_HAT_BYTES, F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));

    std::vector<uint8_t> g4WsHost(f203_g4_ws::kTotalBytes, 0);
    // ── Stage INTT：û→u_time（时域），tr̂→tr_time ──
    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, uTimeDevB, uTrDevB, inttWsDevB,
                                                          inttTilingHostB);
    if (ret != 0) {
        std::fprintf(stderr, "[g4_tail] intt u ret=%u\n", ret);
        return 56;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, trTimeDevB, trPaddedDevB, inttWsDevB,
                                                 inttTilingHostB);
    if (ret != 0) {
        std::fprintf(stderr, "[g4_tail] intt tr ret=%u\n", ret);
        return 57;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // dump #1：INTT 后（加噪前）→ 对 golden_u_time / golden_tr_time（定位 INTT 计算/搬运）
    if (dump_intermediate) {
        std::vector<uint8_t> tmp(dstFileBytes);
        CHECK_ACL(aclrtMemcpy(tmp.data(), dstFileBytes, uTimeDevB, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile(out_dir + "/sim_u_time.bin", tmp.data(), dstFileBytes);
        CHECK_ACL(aclrtMemcpy(tmp.data(), dstFileBytes, trTimeDevB, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile(out_dir + "/sim_tr_time.bin", tmp.data(), dstFileBytes);
        std::printf("[g4_tail] dump sim_u_time/sim_tr_time\n");
    }

    // ── Stage noise：把 u_time/tr_time + e1/e2 + m 打包进单块 workspace，2 参 launch ──
    CHECK_ACL(aclrtMemcpy(g4WsHost.data() + f203_g4_ws::kUOff, dstFileBytes, uTimeDevB, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(g4WsHost.data() + f203_g4_ws::kTrOff, dstFileBytes, trTimeDevB, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(g4WsHost.data() + f203_g4_ws::kE1Off, re_flat + F203_R_POLYVEC_BYTES, F203_E1_POLYVEC_BYTES);
    std::memcpy(g4WsHost.data() + f203_g4_ws::kE2Off,
                re_flat + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES, F203_E2_POLY_BYTES);
    std::memcpy(g4WsHost.data() + f203_g4_ws::kMOff, m, F203_MSG_BYTES);
    CHECK_ACL(aclrtMemcpy(wsDevB, f203_g4_ws::kTotalBytes, g4WsHost.data(), f203_g4_ws::kTotalBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise_ws)(kG4NoiseBlockDim, stream, wsDevB, vDevB);
    if (ret != 0) {
        std::fprintf(stderr, "[g4_tail] g4_noise_ws ret=%u\n", ret);
        return 58;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // dump #2：noise 后 → 对 golden_u_noisy / golden_v（定位 g4_noise 计算/搬运）
    if (dump_intermediate) {
        std::vector<uint8_t> tmpU(dstFileBytes);
        CHECK_ACL(aclrtMemcpy(tmpU.data(), dstFileBytes, wsDevB + f203_g4_ws::kUOff, dstFileBytes,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile(out_dir + "/sim_u_noisy.bin", tmpU.data(), dstFileBytes);
        std::vector<uint8_t> tmpV(F203_E2_POLY_BYTES);
        CHECK_ACL(aclrtMemcpy(tmpV.data(), F203_E2_POLY_BYTES, vDevB, F203_E2_POLY_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
        WriteFile(out_dir + "/sim_v.bin", tmpV.data(), F203_E2_POLY_BYTES);
        std::printf("[g4_tail] dump sim_u_noisy/sim_v\n");
    }

    // ── Stage pack：noise 后的 u（在 wsDevB 的 u 区）+ v → c ──
    CHECK_ACL(aclrtMemcpy(uTimeDevB, dstFileBytes, wsDevB + f203_g4_ws::kUOff, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_pack)(kPackBlockDim, stream, uTimeDevB, vDevB, cDevB);
    if (ret != 0) {
        std::fprintf(stderr, "[g4_tail] pack ret=%u\n", ret);
        return 59;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(c_out, F203_CT_PKE_BYTES, cDevB, F203_CT_PKE_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    CHECK_ACL(aclrtFree(reDevB));
    CHECK_ACL(aclrtFree(mDevB));
    CHECK_ACL(aclrtFree(uTrDevB));
    CHECK_ACL(aclrtFree(uTimeDevB));
    CHECK_ACL(aclrtFree(trPaddedDevB));
    CHECK_ACL(aclrtFree(trTimeDevB));
    CHECK_ACL(aclrtFree(inttWsDevB));
    CHECK_ACL(aclrtFree(wsDevB));
    CHECK_ACL(aclrtFree(vDevB));
    CHECK_ACL(aclrtFree(cDevB));
    CHECK_ACL(aclrtFreeHost(inttTilingHostB));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());

    if (!WriteFile(out_dir + "/c.bin", c_out, F203_CT_PKE_BYTES)) {
        return 65;
    }
    std::printf("[g4_tail] done c.bin=%uB dump=%d\n", F203_CT_PKE_BYTES, static_cast<int>(dump_intermediate));
    return 0;
}

/**
 * tail-only 快跑入口：跳过 prep/NTT/decode/G3，直接读已验证 golden（u_hat/tr_hat/e1/e2）+
 * input/m.bin，只跑 INTT→noise→pack。把 SIM 迭代从全链 ~500s 降到秒级，便于逐步调试 tail。
 *
 * 前置：scripts/host_golden/gen_g4_tail_golden.py 已生成 output/golden_{u_hat,tr_hat,e1,e2}.bin。
 * 始终开启 dump，产出 sim_{u_time,tr_time,u_noisy,v}.bin 供 verify_g4_tail.py 对拍。
 */
int run_encrypt_g4_tail_only_sim(const std::string &case_dir, const uint8_t *lut_intt_even,
                                 const uint8_t *lut_intt_odd, uint8_t *c_out)
{
    using namespace tiling;
    const std::string out_dir = case_dir + "/output";
    std::printf("[main_encrypt] G4 tail-only SIM (read golden u_hat/tr_hat/e1/e2; INTT→noise→pack)\n");

    auto readBin = [](const std::string &p, std::vector<uint8_t> &buf, size_t want) -> bool {
        FILE *f = std::fopen(p.c_str(), "rb");
        if (f == nullptr) {
            std::fprintf(stderr, "[g4_tail_only] missing %s\n", p.c_str());
            return false;
        }
        buf.resize(want);
        const size_t got = std::fread(buf.data(), 1, want, f);
        std::fclose(f);
        if (got != want) {
            std::fprintf(stderr, "[g4_tail_only] %s size %zu != %zu\n", p.c_str(), got, want);
            return false;
        }
        return true;
    };

    std::vector<uint8_t> u_hat;
    std::vector<uint8_t> tr_hat;
    std::vector<uint8_t> e1;
    std::vector<uint8_t> e2;
    std::vector<uint8_t> m;
    if (!readBin(out_dir + "/golden_u_hat.bin", u_hat, F203_U_HAT_BYTES) ||
        !readBin(out_dir + "/golden_tr_hat.bin", tr_hat, F203_TR_HAT_BYTES) ||
        !readBin(out_dir + "/golden_e1.bin", e1, F203_E1_POLYVEC_BYTES) ||
        !readBin(out_dir + "/golden_e2.bin", e2, F203_E2_POLY_BYTES) ||
        !readBin(case_dir + "/input/m.bin", m, F203_MSG_BYTES)) {
        return 70;
    }

    // u_tr_host = [u_hat | tr_hat]，与 G3 输出布局一致
    std::vector<uint8_t> u_tr_host(kUTrBytes, 0);
    std::memcpy(u_tr_host.data(), u_hat.data(), F203_U_HAT_BYTES);
    std::memcpy(u_tr_host.data() + F203_U_HAT_BYTES, tr_hat.data(), F203_TR_HAT_BYTES);

    // re_flat = [r(占位) | e1 | e2]；tail 只读 e1/e2 段
    std::vector<uint8_t> re_flat(F203_RE_TOTAL_BYTES, 0);
    std::memcpy(re_flat.data() + F203_R_POLYVEC_BYTES, e1.data(), F203_E1_POLYVEC_BYTES);
    std::memcpy(re_flat.data() + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES, e2.data(), F203_E2_POLY_BYTES);

    TilingData inttTiling{};
    inttTiling.tileLength = static_cast<int32_t>(n);
    inttTiling.kPolys = static_cast<int32_t>(kK);
    inttTiling.mixPass = static_cast<int32_t>(kInttMixPass);

    return run_g4_tail_sim(out_dir, u_tr_host.data(), re_flat.data(), m.data(), lut_intt_even, lut_intt_odd,
                           inttTiling, c_out, /*dump_intermediate=*/true);
}

/**
 * SIM/NPU G5：单 ACL session 跑完全链（prep→NTT→decode→g3→INTT→noise→pack）。
 * 背景：旧 phase1+多段 Finalize+G4 tail 导致 SIM c.bin 错；对齐 CPU run_g5_cpu_session 的 GM 生命周期。
 */
int run_encrypt_g5_sim_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out)
{
    using namespace tiling;
    std::printf("[main_encrypt] G5 SIM single-session full chain (device decode + g3_linear4)\n");

    ShakeGeneralTilingData shakeTiling{};
    FillShakeTiling(&shakeTiling, kPrfBatch, kPrfMaxMsgLen, kPrfOutLen, SHAKE256_RATE_BYTES);
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
    uint8_t *coinsDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;
    uint8_t *rHatDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    TilingData *nttTilingHost = nullptr;
    TilingData *inttTilingHost = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *aCol0Dev = nullptr;
    uint8_t *matDev = nullptr; // G3 单 launch at_r5：host 拼的 5×4 矩阵 [Âᵀ(4 列)|t̂(1 列)]，列步长 5
    uint8_t *uTrDev = nullptr;
    uint8_t *uTimeDev = nullptr;
    uint8_t *trPaddedDev = nullptr;
    uint8_t *trTimeDev = nullptr;
    uint8_t *inttWsDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *cDev = nullptr;
    uint8_t *wsDev = nullptr; // G4 noise workspace：u_time|tr_time|e1|e2|m 单块打包（g4_noise_ws 2 参）

    CHECK_ACL(aclrtMalloc((void **)&ekDev, F203_EK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&coinsDev, F203_ENC_COINS_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aHatDev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&prfDev, F203_ENCRYPT_PRF_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&reDev, F203_RE_TOTAL_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&shakeTilingDev, kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&rHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&nttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMallocHost((void **)&inttTilingHost, sizeof(TilingData)));
    CHECK_ACL(aclrtMalloc((void **)&tHatDev, F203_T_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&aCol0Dev, F203_AHAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&matDev, static_cast<size_t>(5 * 4 * 256) * sizeof(int32_t),
                          ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTrDev, kUTrBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&trTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_E2_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wsDev, f203_g4_ws::kTotalBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMemcpy(ekDev, F203_EK_PKE_BYTES, ek, F203_EK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(coinsDev, F203_ENC_COINS_BYTES, coins, F203_ENC_COINS_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(mDev, F203_MSG_BYTES, m, F203_MSG_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTiling, kShakeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::vector<uint8_t> wsHost(wssize, 0);
    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_ntt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_ntt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(nttWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::memset(wsHost.data(), 0, wssize);
    std::memcpy(wsHost.data() + LUT_EVEN_STACKED, lut_intt_even, lutEvenOddFileBytes);
    std::memcpy(wsHost.data() + LUT_ODD_STACKED, lut_intt_odd, lutEvenOddFileBytes);
    CHECK_ACL(aclrtMemcpy(inttWsDev + LUT_EVEN_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_EVEN_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(inttWsDev + LUT_ODD_STACKED, lutEvenOddFileBytes, wsHost.data() + LUT_ODD_STACKED,
                          lutEvenOddFileBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    *nttTilingHost = nttTiling;
    *inttTilingHost = inttTiling;

    uint8_t *rhoDev = ekDev + F203_EK_RHO_OFFSET;
    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_a_hat)(kAhatBlockDim, stream, rhoDev, aHatDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] prep_a_hat launch ret=%u\n", ret);
        return 50;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_prep_re)(kReBlockDim, stream, coinsDev, prfDev, reDev, shakeTilingDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] prep_re launch ret=%u\n", ret);
        return 51;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_ntt_r)(kNttBlockDim, stream, rHatDev, reDev, nttWsDev, nttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] ntt_r launch ret=%u\n", ret);
        return 52;
    }
    CHECK_ACL(aclrtMemset(aCol0Dev, F203_AHAT_BYTES, 0, F203_AHAT_BYTES));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_decode_t_hat)(kDecodeBlockDim, stream, ekDev, tHatDev, aCol0Dev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] decode_t_hat launch ret=%u\n", ret);
        return 53;
    }
    // ===== G3 单 launch（病根修正核心）：合并核 at_r5，一次出 [û|tr̂] =====
    // 历史 G3_SIM_AUDIT：同一 session 第二次 at_r/t_dot_r -> 507000 或 tr̂ 全错；旧路径靠「每 at_r
    //   独立 session」绕过，恰是全链多 session -> 507000 病根。曾改「at_r×2 假 MIX」收进单 launch，
    //   但假 MIX（AIC 空跑、无 CrossCore 握手）产出 û/tr̂ 全 0（与用户判决一致）。
    // 再修正（2026-06-30）实测：单 session 中 MIX(ntt_r) binary 加载后，**仅第一次**自定义 AIV launch
    //   可靠：decode_t_hat(t̂)+第一发 at_r(û) 均与 golden 一致；但第二发自定义 AIV——不同符号
    //   at_r_col0 → 507000、同符号 at_r 二次 → tr̂ 错值（got=25/ref=3181，与 §9.9 一致）。
    // 故 G3 必须把 û+tr̂ 收进**一次** launch：host 拼 5×4 矩阵 matM（列步长 5，p<4=Â[j,p]、p=4=t̂[j]），
    //   at_r5(matM,r̂) 输出 uTrDev=[û(4)=Âᵀ·r̂ | tr̂(1)=t̂·r̂]。与 proven ProcessAtR 同算法，仅 kPOut 4→5。
    {
        // 病根修正（2026-06-30）：matM 由 host 读回 aHatDev/tHatDev 拼装；必须先同步 stream，
        //   否则 prep_a_hat/decode_t_hat 的异步 launch 未完成时 D2H 取到 0 → matM 的 Â 列全 0
        //   → at_r5 算出 û=Σ0·r̂=0（实测 u_hat 全 0 即此因；proven at_r 直接在设备读 aHatDev 故无此问题）。
        CHECK_ACL(aclrtSynchronizeStream(stream));
        constexpr int kN = 256;
        std::vector<int32_t> aHatHost(static_cast<size_t>(F203_AHAT_BYTES) / sizeof(int32_t));
        std::vector<int32_t> tHatHost(static_cast<size_t>(F203_T_HAT_BYTES) / sizeof(int32_t));
        CHECK_ACL(aclrtMemcpy(aHatHost.data(), F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        CHECK_ACL(aclrtMemcpy(tHatHost.data(), F203_T_HAT_BYTES, tHatDev, F203_T_HAT_BYTES,
                              ACL_MEMCPY_DEVICE_TO_HOST));
        // matM[(j*5+p)*N + n]：p<4 取 Â[j,p]（aHat 行优先 [j,p] poly），p=4 取 t̂[j]。
        std::vector<int32_t> matHost(static_cast<size_t>(5 * 4 * kN));
        for (int j = 0; j < 4; ++j) {
            for (int p = 0; p < 5; ++p) {
                int32_t *dst = matHost.data() + static_cast<size_t>(j * 5 + p) * kN;
                const int32_t *src = (p < 4) ? (aHatHost.data() + static_cast<size_t>(j * 4 + p) * kN)
                                             : (tHatHost.data() + static_cast<size_t>(j) * kN);
                std::memcpy(dst, src, static_cast<size_t>(kN) * sizeof(int32_t));
            }
        }
        const size_t matBytes = matHost.size() * sizeof(int32_t);
        CHECK_ACL(aclrtMemcpy(matDev, matBytes, matHost.data(), matBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r5)(kG3BlockDim, stream, matDev, rHatDev, uTrDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] at_r5(u_hat|tr_hat) launch ret=%u\n", ret);
        return 54;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // Session A 结束：D2H G1–G3 中间量（设备已算完 NTT 域 û/tr̂）
    std::vector<uint8_t> a_hat(F203_AHAT_BYTES);
    std::vector<uint8_t> re_flat(F203_RE_TOTAL_BYTES);
    std::vector<uint8_t> r_hat(F203_R_HAT_BYTES);
    std::vector<uint8_t> t_hat(F203_T_HAT_BYTES);
    std::vector<uint8_t> u_hat(F203_U_HAT_BYTES);
    std::vector<uint8_t> tr_hat(F203_TR_HAT_BYTES);
    CHECK_ACL(aclrtMemcpy(a_hat.data(), F203_AHAT_BYTES, aHatDev, F203_AHAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(re_flat.data(), F203_RE_TOTAL_BYTES, reDev, F203_RE_TOTAL_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(r_hat.data(), dstFileBytes, rHatDev, dstFileBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(t_hat.data(), F203_T_HAT_BYTES, tHatDev, F203_T_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(u_hat.data(), F203_U_HAT_BYTES, uTrDev, F203_U_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(tr_hat.data(), F203_TR_HAT_BYTES, uTrDev + F203_U_HAT_BYTES, F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_HOST));

    // ===== G4 tail（同一 session 内继续；病根修正核心）=====
    // 旧探针在此 aclFinalize 结束 Session A，再 run_g4_tail_sim 另起 Session B(aclInit) ->
    //   SIM device_aiv.o 重注册失败 507000 + 末尾 free(): invalid pointer。本探针对齐 keygen
    //   单 session 模板（prep AIV 最先、NTT 之后全 MIX 核），全链一次 aclInit/aclFinalize。
    // INTT: u_hat->u_time（uTimeDev）；tr_hat padding 到整 poly->tr_time（trTimeDev）
    CHECK_ACL(aclrtMemset(trPaddedDev, dstFileBytes, 0, dstFileBytes));
    CHECK_ACL(aclrtMemcpy(trPaddedDev, F203_TR_HAT_BYTES, uTrDev + F203_U_HAT_BYTES, F203_TR_HAT_BYTES,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, uTimeDev, uTrDev, inttWsDev, inttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] intt u ret=%u\n", ret);
        return 56;
    }
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_intt)(kInttBlockDim, stream, trTimeDev, trPaddedDev, inttWsDev,
                                                 inttTilingHost);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] intt tr ret=%u\n", ret);
        return 57;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // 组装 G4 noise workspace：u_time | tr_time | e1 | e2 | m（单块 H2D，g4_noise_ws 2 参）
    std::vector<uint8_t> g4WsHost(f203_g4_ws::kTotalBytes, 0);
    CHECK_ACL(aclrtMemcpy(g4WsHost.data() + f203_g4_ws::kUOff, dstFileBytes, uTimeDev, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(g4WsHost.data() + f203_g4_ws::kTrOff, dstFileBytes, trTimeDev, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(g4WsHost.data() + f203_g4_ws::kE1Off, re_flat.data() + F203_R_POLYVEC_BYTES, F203_E1_POLYVEC_BYTES);
    std::memcpy(g4WsHost.data() + f203_g4_ws::kE2Off,
                re_flat.data() + F203_R_POLYVEC_BYTES + F203_E1_POLYVEC_BYTES, F203_E2_POLY_BYTES);
    std::memcpy(g4WsHost.data() + f203_g4_ws::kMOff, m, F203_MSG_BYTES);
    CHECK_ACL(aclrtMemcpy(wsDev, f203_g4_ws::kTotalBytes, g4WsHost.data(), f203_g4_ws::kTotalBytes,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_g4_noise_ws)(kG4NoiseBlockDim, stream, wsDev, vDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] g4_noise_ws ret=%u\n", ret);
        return 58;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // pack：noise 后的 u（wsDev 的 u 区，g4_noise_ws 原位写回）+ v -> c
    CHECK_ACL(aclrtMemcpy(uTimeDev, dstFileBytes, wsDev + f203_g4_ws::kUOff, dstFileBytes,
                          ACL_MEMCPY_DEVICE_TO_DEVICE));
    ret = ACLRT_LAUNCH_KERNEL(f203_encrypt_pack)(kPackBlockDim, stream, uTimeDev, vDev, cDev);
    if (ret != 0) {
        std::fprintf(stderr, "[g5_sim] pack ret=%u\n", ret);
        return 59;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(c_out, F203_CT_PKE_BYTES, cDev, F203_CT_PKE_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    // ===== 全链单 session 结束：统一释放 + 一次 aclFinalize =====
    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(rHatDev));
    CHECK_ACL(aclrtFree(nttWsDev));
    CHECK_ACL(aclrtFreeHost(nttTilingHost));
    CHECK_ACL(aclrtFreeHost(inttTilingHost));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(aCol0Dev));
    CHECK_ACL(aclrtFree(matDev));
    CHECK_ACL(aclrtFree(uTrDev));
    CHECK_ACL(aclrtFree(uTimeDev));
    CHECK_ACL(aclrtFree(trPaddedDev));
    CHECK_ACL(aclrtFree(trTimeDev));
    CHECK_ACL(aclrtFree(inttWsDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());

    // 中间量 + c.bin 落盘（Finalize 后用 host 副本写盘）
    const std::string out_dir = case_dir + "/output";
    const int woA = write_g1_outputs(out_dir, a_hat.data(), re_flat.data());
    if (woA != 0) {
        return 60 + woA;
    }
    if (!WriteFile(out_dir + "/r_hat.bin", r_hat.data(), r_hat.size()) ||
        !WriteFile(out_dir + "/t_hat.bin", t_hat.data(), t_hat.size()) ||
        !WriteFile(out_dir + "/u_hat.bin", u_hat.data(), u_hat.size()) ||
        !WriteFile(out_dir + "/tr_hat.bin", tr_hat.data(), tr_hat.size())) {
        return 65;
    }
    if (!WriteFile(out_dir + "/c.bin", c_out, F203_CT_PKE_BYTES)) {
        return 66;
    }
    std::printf("[main_encrypt] G5 SIM single-session done c.bin=%uB\n", F203_CT_PKE_BYTES);
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
