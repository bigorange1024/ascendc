/**
 * @file main_kem_decaps_phase_d_run.cpp
 * @brief Phase-D Host：Decrypt（Alg.15）→ m'。
 *
 * ## Launch（2026-09-03，2-launch 安全路径，缓解多跑粘性）
 * - **默认**：
 *   1) `f203_decrypt_g4_chain_ntt`：prep + NTT 一轮 Cube + su_dot/pad
 *   2) `f203_decrypt_g4_chain_intt`：INTT 一轮 Cube + Compress₁/Encode₁
 * - **旧单核双 Cube**：`F203_DECRYPT_FUSED=1` → `f203_decrypt_device_fused`
 *
 * Alg.18 行 1–4：dk_pke = dk_kem[0:1536)。SIM 本段自含 aclInit…Finalize。
 * 与 golden：本段只产出 m'。
 */
#include "main_kem_decaps_phase_d_run.hpp"

#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_kem_dec_layout.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "acl_session/acl_session.hpp"
#include "aclrtlaunch_f203_decrypt_device_fused.h"
#include "aclrtlaunch_f203_decrypt_g4_chain_intt.h"
#include "aclrtlaunch_f203_decrypt_g4_chain_ntt.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(
    GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm,
    GM_ADDR wPaddedGm, GM_ADDR nttWsGm, GM_ADDR softSyncGm, TilingData tiling);
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_intt(GM_ADDR vGm, GM_ADDR wPaddedGm, GM_ADDR wTimeGm,
                                                                 GM_ADDR mGm, GM_ADDR inttWsGm, TilingData tiling);
extern "C" __global__ __aicore__ void f203_decrypt_device_fused(
    GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm,
    GM_ADDR wPaddedGm, GM_ADDR wTimeGm, GM_ADDR mGm, GM_ADDR nttWsGm, GM_ADDR inttWsGm, GM_ADDR softSyncGm,
    TilingData tiling);
#endif

#ifndef ASCENDC_CPU_DEBUG
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)
#endif

namespace {

constexpr uint32_t kBlockDim = 1U;
constexpr size_t kSoftSyncBytes = 64U;

void FillNttWs(uint8_t *ws, size_t wsBytes, const uint8_t *lut_even, const uint8_t *lut_odd)
{
    std::memset(ws, 0, wsBytes);
    std::memcpy(ws + tiling::LUT_EVEN_STACKED, lut_even, tiling::lutEvenOddFileBytes);
    std::memcpy(ws + tiling::LUT_ODD_STACKED, lut_odd, tiling::lutEvenOddFileBytes);
}

TilingData MakeTiling()
{
    TilingData t{};
    t.tileLength = static_cast<int32_t>(tiling::n);
    t.kPolys = static_cast<int32_t>(tiling::kK);
    t.mixPass = 3;
    return t;
}

bool DecryptFusedOn()
{
#ifndef ASCENDC_CPU_DEBUG
    return ascendc_acl::EnvFlagOn("F203_DECRYPT_FUSED");
#else
    const char *v = std::getenv("F203_DECRYPT_FUSED");
    return v != nullptr && v[0] == '1' && v[1] == '\0';
#endif
}

}  // namespace

int RunKemDecapsPhaseD(const uint8_t *dk_kem, const uint8_t *c, const uint8_t *lut_even,
                       const uint8_t *lut_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                       uint8_t *m_out)
{
    using namespace tiling;
    const TilingData tilingData = MakeTiling();
    const uint8_t *dk_pke = dk_kem;
    const bool fused = DecryptFusedOn();

#ifdef ASCENDC_CPU_DEBUG
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
    uint8_t *softSyncGm = (uint8_t *)AscendC::GmAlloc(kSoftSyncBytes);
    std::memset(softSyncGm, 0, kSoftSyncBytes);

    std::memcpy(dkGm, dk_pke, F203_DK_PKE_BYTES);
    std::memcpy(cGm, c, F203_CT_PKE_BYTES);
    FillNttWs(nttWsGm, wssize, lut_even, lut_odd);
    FillNttWs(inttWsGm, wssize, lut_intt_even, lut_intt_odd);

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    if (fused) {
        std::fprintf(stderr, "[kem-dec-d] CPU fused (F203_DECRYPT_FUSED=1)\n");
        ICPU_RUN_KF(f203_decrypt_device_fused, kBlockDim, dkGm, cGm, uGm, vGm, sHatGm, uHatGm, wHatGm, wPaddedGm,
                    wTimeGm, mGm, nttWsGm, inttWsGm, softSyncGm, tilingData);
    } else {
        std::fprintf(stderr, "[kem-dec-d] CPU safe 2-launch (prep+ntt | intt)\n");
        ICPU_RUN_KF(f203_decrypt_g4_chain_ntt, kBlockDim, dkGm, cGm, uGm, vGm, sHatGm, uHatGm, wHatGm, wPaddedGm,
                    nttWsGm, softSyncGm, tilingData);
        ICPU_RUN_KF(f203_decrypt_g4_chain_intt, kBlockDim, vGm, wPaddedGm, wTimeGm, mGm, inttWsGm, tilingData);
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
    AscendC::GmFree(softSyncGm);
    return 0;
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    ascendc_acl::DeviceGuard aclGuard(deviceId);
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tilingHost = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHost), sizeof(TilingData)));
    std::memcpy(tilingHost, &tilingData, sizeof(TilingData));

    uint8_t *dkDev = nullptr;
    uint8_t *cDev = nullptr;
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
    uint8_t *softSyncDev = nullptr;

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dkDev), F203_DK_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&cDev), F203_CT_PKE_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&mDev), F203_MSG_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uDev), F203_U_POLYVEC_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&vDev), F203_V_POLY_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&sHatDev), F203_S_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&uHatDev), dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wHatDev), F203_W_HAT_BYTES, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wPaddedDev), dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wTimeDev), dstFileBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&nttWsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&inttWsDev), wssize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&softSyncDev), kSoftSyncBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMemcpy(dkDev, F203_DK_PKE_BYTES, dk_pke, F203_DK_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cDev, F203_CT_PKE_BYTES, c, F203_CT_PKE_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    {
        std::vector<uint8_t> zeros(kSoftSyncBytes, 0);
        CHECK_ACL(aclrtMemcpy(softSyncDev, kSoftSyncBytes, zeros.data(), kSoftSyncBytes, ACL_MEMCPY_HOST_TO_DEVICE));
        std::vector<uint8_t> wsHost(wssize, 0);
        FillNttWs(wsHost.data(), wssize, lut_even, lut_odd);
        CHECK_ACL(aclrtMemcpy(nttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
        FillNttWs(wsHost.data(), wssize, lut_intt_even, lut_intt_odd);
        CHECK_ACL(aclrtMemcpy(inttWsDev, wssize, wsHost.data(), wssize, ACL_MEMCPY_HOST_TO_DEVICE));
    }

    if (!fused) {
        std::fprintf(stderr, "[kem-dec-d] launch 1 f203_decrypt_g4_chain_ntt (prep+NTT, one Cube)\n");
        ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_chain_ntt)
        (kBlockDim, stream, dkDev, cDev, uDev, vDev, sHatDev, uHatDev, wHatDev, wPaddedDev, nttWsDev, softSyncDev,
         tilingHost);
        CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_decrypt_g4_chain_ntt"));

        std::fprintf(stderr, "[kem-dec-d] launch 2 f203_decrypt_g4_chain_intt (INTT+tail, one Cube)\n");
        ACLRT_LAUNCH_KERNEL(f203_decrypt_g4_chain_intt)
        (kBlockDim, stream, vDev, wPaddedDev, wTimeDev, mDev, inttWsDev, tilingHost);
        CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_decrypt_g4_chain_intt"));
    } else {
        std::fprintf(stderr, "[kem-dec-d] launch f203_decrypt_device_fused (F203_DECRYPT_FUSED=1)\n");
        ACLRT_LAUNCH_KERNEL(f203_decrypt_device_fused)
        (kBlockDim, stream, dkDev, cDev, uDev, vDev, sHatDev, uHatDev, wHatDev, wPaddedDev, wTimeDev, mDev, nttWsDev,
         inttWsDev, softSyncDev, tilingHost);
        CHECK_ACL(ascendc_acl::TimedSynchronizeStream(stream, "f203_decrypt_device_fused"));
    }

    CHECK_ACL(aclrtMemcpy(m_out, F203_MSG_BYTES, mDev, F203_MSG_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));

    CHECK_ACL(aclrtFree(dkDev));
    CHECK_ACL(aclrtFree(cDev));
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
    CHECK_ACL(aclrtFree(softSyncDev));
    CHECK_ACL(aclrtFreeHost(tilingHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    return 0;
#endif
}
