/**
 * @file main_decrypt_g4_run.cpp
 * @brief G4 生产路径：**2 launch**（prep → compute），compute 内含 NTT 链 + INTT 链两次 kernel（中间 sync）。
 */
#include "f203_decrypt_g4_run.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_decrypt_g4_prep.h"
#include "aclrtlaunch_f203_decrypt_g4_chain_ntt.h"
#include "aclrtlaunch_f203_decrypt_g4_chain_intt.h"
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
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_intt(GM_ADDR vGm, GM_ADDR wPaddedGm, GM_ADDR wTimeGm,
                                                                   GM_ADDR mGm, GM_ADDR inttWsGm, TilingData tiling);
extern volatile int g_f203_decrypt_g4_chain_ntt_mix_pass;
extern volatile int g_f203_decrypt_g4_chain_intt_mix_pass;
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

constexpr uint32_t kG4BlockDim = 1U;
constexpr uint32_t kG4MixPass = 3U;

static void fill_ntt_ws(uint8_t *ws, size_t wsBytes, const uint8_t *lut_even, const uint8_t *lut_odd)
{
    std::memset(ws, 0, wsBytes);
    std::memcpy(ws + tiling::LUT_EVEN_STACKED, lut_even, tiling::lutEvenOddFileBytes);
    std::memcpy(ws + tiling::LUT_ODD_STACKED, lut_odd, tiling::lutEvenOddFileBytes);
}

int run_g4_session(const uint8_t *dk, const uint8_t *c, uint8_t *u, uint8_t *v, uint8_t *s_hat, uint8_t *u_hat,
                   uint8_t *w_hat, uint8_t *w_time, const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                   const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *m_out)
{
    using namespace tiling;
    TilingData g4Tiling{};
    g4Tiling.tileLength = static_cast<int32_t>(n);
    g4Tiling.kPolys = static_cast<int32_t>(kK);
    g4Tiling.mixPass = static_cast<int32_t>(kG4MixPass);

#ifdef ASCENDC_CPU_DEBUG
    g_f203_decrypt_g4_chain_ntt_mix_pass = static_cast<int>(kG4MixPass);
    g_f203_decrypt_g4_chain_intt_mix_pass = static_cast<int>(kG4MixPass);

    uint8_t *dkGm = (uint8_t *)AscendC::GmAlloc(F203_DK_PKE_BYTES);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(F203_CT_PKE_BYTES);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(F203_MSG_BYTES);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(F203_U_POLYVEC_BYTES);
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(F203_V_POLY_BYTES);
    uint8_t *sHatGm = (uint8_t *)AscendC::GmAlloc(F203_S_HAT_BYTES);
    uint8_t *uHatGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wHatGm = (uint8_t *)AscendC::GmAlloc(F203_W_HAT_BYTES);
    uint8_t *wPaddedGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *wTimeGm = (uint8_t *)AscendC::GmAlloc(dstFileBytes);
    uint8_t *nttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);
    uint8_t *inttWsGm = (uint8_t *)AscendC::GmAlloc(wssize > 1024 ? wssize : 1024);

    std::memcpy(dkGm, dk, F203_DK_PKE_BYTES);
    std::memcpy(cGm, c, F203_CT_PKE_BYTES);
    fill_ntt_ws(nttWsGm, wssize, lut_ntt_even, lut_ntt_odd);
    fill_ntt_ws(inttWsGm, wssize, lut_intt_even, lut_intt_odd);

    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_decrypt_g4_prep, kG4BlockDim, dkGm, cGm, uGm, vGm, sHatGm);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    ICPU_RUN_KF(f203_decrypt_g4_chain_ntt, kG4BlockDim, uGm, sHatGm, uHatGm, wHatGm, wPaddedGm, nttWsGm, g4Tiling);
    ICPU_RUN_KF(f203_decrypt_g4_chain_intt, kG4BlockDim, vGm, wPaddedGm, wTimeGm, mGm, inttWsGm, g4Tiling);

    std::memcpy(u, uGm, F203_U_POLYVEC_BYTES);
    std::memcpy(v, vGm, F203_V_POLY_BYTES);
    std::memcpy(s_hat, sHatGm, F203_S_HAT_BYTES);
    std::memcpy(u_hat, uHatGm, F203_U_HAT_BYTES);
    std::memcpy(w_hat, wHatGm, F203_W_HAT_BYTES);
    if (w_time != nullptr) {
        std::memcpy(w_time, wTimeGm, F203_V_POLY_BYTES);
    }
    std::memcpy(m_out, mGm, F203_MSG_BYTES);

    AscendC::GmFree(dkGm);
    AscendC::GmFree(cGm);
    AscendC::GmFree(mGm);
    AscendC::GmFree(uGm);
    AscendC::GmFree(vGm);
    AscendC::GmFree(sHatGm);
    AscendC::GmFree(uHatGm);
    AscendC::GmFree(wHatGm);
    AscendC::GmFree(wPaddedGm);
    AscendC::GmFree(wTimeGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(inttWsGm);
    return 0;
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *cDev = nullptr;
    uint8_t *dkDev = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *sHatDev = nullptr;
    uint8_t *uHatDev = nullptr;
    uint8_t *wHatDev = nullptr;
    uint8_t *wPaddedDev = nullptr;
    uint8_t *wTimeDev = nullptr;
    uint8_t *nttWsDev = nullptr;
    uint8_t *inttWsDev = nullptr;
    TilingData *tilingHost = nullptr;

    CHECK_ACL(aclrtMalloc((void **)&cDev, F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dkDev, F203_DK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&mDev, F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, F203_U_POLYVEC_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, F203_V_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sHatDev, F203_S_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uHatDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wHatDev, F203_W_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wPaddedDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&wTimeDev, dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&nttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&inttWsDev, wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)&tilingHost, sizeof(TilingData)));
    *tilingHost = g4Tiling;

    CHECK_ACL(aclrtMemcpy(cDev, F203_CT_PKE_BYTES, c, F203_CT_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkDev, F203_DK_PKE_BYTES, dk, F203_DK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    {
        std::vector<uint8_t> wsHost(wssize, 0);
        fill_ntt_ws(wsHost.data(), wssize, lut_ntt_even, lut_ntt_odd);
        CHECK_ACL(aclrtMemcpy(nttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        fill_ntt_ws(wsHost.data(), wssize, lut_intt_even, lut_intt_odd);
        CHECK_ACL(aclrtMemcpy(inttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    }

    uint32_t ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_prep)(kG4BlockDim, stream, dkDev, cDev, uDev, vDev, sHatDev);
    if (ret != 0) {
        return 30;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_chain_ntt)(kG4BlockDim, stream, uDev, sHatDev, uHatDev, wHatDev,
                                                         wPaddedDev, nttWsDev, tilingHost);
    if (ret != 0) {
        return 31;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    ret = ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_chain_intt)(kG4BlockDim, stream, vDev, wPaddedDev, wTimeDev, mDev,
                                                         inttWsDev, tilingHost);
    if (ret != 0) {
        return 32;
    }
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(u, F203_U_POLYVEC_BYTES, uDev, F203_U_POLYVEC_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(v, F203_V_POLY_BYTES, vDev, F203_V_POLY_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(s_hat, F203_S_HAT_BYTES, sHatDev, F203_S_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(u_hat, F203_U_HAT_BYTES, uHatDev, F203_U_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(w_hat, F203_W_HAT_BYTES, wHatDev, F203_W_HAT_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    if (w_time != nullptr) {
        CHECK_ACL(aclrtMemcpy(w_time, F203_V_POLY_BYTES, wTimeDev, F203_V_POLY_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    }
    CHECK_ACL(aclrtMemcpy(m_out, F203_MSG_BYTES, mDev, F203_MSG_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(sHatDev));
    CHECK_ACL(aclrtFree(uHatDev));
    CHECK_ACL(aclrtFree(wHatDev));
    CHECK_ACL(aclrtFree(wPaddedDev));
    CHECK_ACL(aclrtFree(wTimeDev));
    CHECK_ACL(aclrtFree(nttWsDev));
    CHECK_ACL(aclrtFree(inttWsDev));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
#endif
}

} // namespace

#ifdef ASCENDC_CPU_DEBUG
int run_decrypt_g4_cpu_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                              const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                              const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *m_out)
{
    std::vector<uint8_t> u(F203_U_POLYVEC_BYTES);
    std::vector<uint8_t> v(F203_V_POLY_BYTES);
    std::vector<uint8_t> s_hat(F203_S_HAT_BYTES);
    std::vector<uint8_t> u_hat(F203_U_HAT_BYTES);
    std::vector<uint8_t> w_hat(F203_W_HAT_BYTES);
    std::vector<uint8_t> w_time(F203_V_POLY_BYTES);
    const int rc = run_g4_session(dk, c, u.data(), v.data(), s_hat.data(), u_hat.data(), w_hat.data(), w_time.data(),
                                  lut_ntt_even, lut_ntt_odd, lut_intt_even, lut_intt_odd, m_out);
    if (rc != 0) {
        return rc;
    }
    const std::string out_dir = case_dir + "/output";
    if (!WriteFile(out_dir + "/u.bin", u.data(), u.size()) || !WriteFile(out_dir + "/v.bin", v.data(), v.size()) ||
        !WriteFile(out_dir + "/s_hat.bin", s_hat.data(), s_hat.size()) ||
        !WriteFile(out_dir + "/u_hat.bin", u_hat.data(), u_hat.size()) ||
        !WriteFile(out_dir + "/w_hat.bin", w_hat.data(), w_hat.size()) ||
        !WriteFile(out_dir + "/w_time.bin", w_time.data(), w_time.size()) ||
        !WriteFile(out_dir + "/m.bin", m_out, F203_MSG_BYTES)) {
        return 40;
    }
    std::printf("[main_decrypt] G4 2-launch done m.bin=%uB\n", F203_MSG_BYTES);
    return 0;
}
#endif

#ifndef ASCENDC_CPU_DEBUG
int run_decrypt_g4_sim_full(const uint8_t *dk, const uint8_t *c, uint8_t *u, uint8_t *v, uint8_t *s_hat,
                              uint8_t *u_hat, uint8_t *w_hat, uint8_t *w_time, const uint8_t *lut_ntt_even,
                              const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *m_out)
{
    return run_g4_session(dk, c, u, v, s_hat, u_hat, w_hat, w_time, lut_ntt_even, lut_ntt_odd, lut_intt_even,
                          lut_intt_odd, m_out);
}
#endif
