// @probe pass-fix-f203-alg13-device-keygen-k4
// @file main_keygen.cpp
// @layer host
// @role 全链 keygen host：串联 prep launch 与 compute launch，管理 GM 与生产 I/O。 / Full-chain keygen host driver.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: data_utils.h, f203_keygen_layout.h, f203_keygen_prep_layout.h, shake_general_tiling_data.h, tiling.h, tiling_host.hpp, cstdint, cstring, cstdlib, sys/stat.h, iostream, acl/acl.h, … (+2)
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

/**
 * @file main_keygen.cpp
 * @brief 生产 KeyGen：2 次设备 Launch，GM 全程不落中间盘。
 *
 * Launch 1：f203_keygen_prep（行 3–15）
 * Launch 2：mmad_custom + 行 21 ρ 融合（F203_KEYGEN_EK_PKE）
 * 仅写 output/ek_pke.bin、output/dk_pke.bin
 */
#include "data_utils.h"
#include "f203_keygen_layout.h"
#include "f203_keygen_prep_layout.h"
#include "shake_general_tiling_data.h"
#include "tiling.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm);
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling,
                                                  GM_ADDR rho_gm, GM_ADDR ek_pke_gm);
extern volatile int g_2s1e_mix_pass;
#endif

namespace {
using namespace F203KeygenPrep;

bool KeygenDebugDump()
{
    const char *v = std::getenv("KEYGEN_DEBUG_DUMP");
    return v != nullptr && v[0] == '1';
}

void DebugWrite(const char *relPath, const void *data, size_t nbytes)
{
    if (!KeygenDebugDump() || data == nullptr || nbytes == 0U) {
        return;
    }
    (void)mkdir("./output", 0755);
    (void)mkdir("./output/debug", 0755);
    (void)WriteFile(relPath, data, nbytes);
}

#ifndef __CCE_KT_TEST__
void DebugWriteDev(aclrtStream stream, const char *relPath, uint8_t *dev, size_t nbytes, uint8_t *hostScratch)
{
    if (!KeygenDebugDump() || dev == nullptr || nbytes == 0U) {
        return;
    }
    CHECK_ACL(aclrtMemcpy(hostScratch, nbytes, dev, nbytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtSynchronizeStream(stream));
    DebugWrite(relPath, hostScratch, nbytes);
}
#endif

constexpr size_t kShakeTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr size_t kVecTilingBytes = 64U;
constexpr uint32_t kMmadBlockDim = 1U;

void FillVecTiling(TilingData *t)
{
    std::memset(t, 0, sizeof(TilingData));
    t->tileLength = 256;
    t->mixPass = 0;
}

}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t seed_d = 0U;
    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return 1;
    }

    ShakeGeneralTilingData shakeTilingHost{};
    FillShakeTiling(&shakeTilingHost, kSeBatch, kSeMaxMsgLen, kSePrfOutLen, SHAKE256_RATE_BYTES);
    shakeTilingHost.blockDim = kPrepBlockDim;

    TilingData vecTilingHost{};
    FillVecTiling(&vecTilingHost);

    const size_t dstFileSize = tiling::dstFileBytes;
    const size_t tHatFileSize = tiling::tHatFileBytes;
    const size_t ekPolyBytes = byte_encode::polyVecBytes;
    const size_t ekPkeBytes = F203Keygen::kEkPkeBytes;
    const size_t skFileSize = byte_encode::polyVecBytes;
    const size_t lutFileSize = tiling::lutEvenOddFileBytes;
    const size_t wsFileSize = tiling::wssize;

    std::cout << "[main] keygen SEED_D=" << seed_d << " prep_blockDim=" << kPrepBlockDim
              << " mmad_blockDim=" << kMmadBlockDim << " launches=2 (prep | compute+ek MIX)\n";

    /* --- Launch 1：f203_keygen_prep（AIV_ONLY，blockDim=2）---
     * GM 输出：a_hat[16,256]、src[8,256]、rho[32]、prf_out（CBD 中间态，仍占 GM 但默认不落盘）。
     * 输入：seed_d + presample SHAKE tiling（Host 填充 shakeTilingHost）。
     * block0 独占 PRF+CBD；block1 仅参与 Â 分片，末尾 PIPE_ALL 与 block0 对齐。
     */
#ifdef __CCE_KT_TEST__
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *rhoGm = static_cast<uint8_t *>(AscendC::GmAlloc(kRhoBytes));
    uint8_t *xGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeXBytes));
    uint8_t *lenGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeLenBytes));
    uint8_t *wsSeGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeWsBytes));
    uint8_t *shakeTilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kShakeTilingBytes));

    uint8_t *dstGm = static_cast<uint8_t *>(AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024));
    uint8_t *tHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(tHatFileSize > 1024 ? tHatFileSize : 1024));
    uint8_t *ekPolyGm = static_cast<uint8_t *>(AscendC::GmAlloc(ekPolyBytes > 1024 ? ekPolyBytes : 1024));
    uint8_t *ekPkeGm = static_cast<uint8_t *>(AscendC::GmAlloc(ekPkeBytes > 1024 ? ekPkeBytes : 1024));
    uint8_t *skGm = static_cast<uint8_t *>(AscendC::GmAlloc(skFileSize > 1024 ? skFileSize : 1024));
    uint8_t *wsGm = static_cast<uint8_t *>(AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024));

    std::memcpy(seedGm, &seed_d, kSeedBytes);
    std::memcpy(shakeTilingGm, &shakeTilingHost, kShakeTilingBytes);
    for (size_t i = 0; i < wsFileSize; ++i) {
        wsGm[i] = 0;
    }
    rs = lutFileSize;
    if (!ReadFile("./input/lut_even_stacked.bin", rs, wsGm + tiling::LUT_EVEN_STACKED, lutFileSize)) {
        return 10;
    }
    rs = lutFileSize;
    if (!ReadFile("./input/lut_odd_stacked.bin", rs, wsGm + tiling::LUT_ODD_STACKED, lutFileSize)) {
        return 10;
    }

    ICPU_RUN_KF(f203_keygen_prep, kPrepBlockDim, seedGm, aHatGm, prfGm, srcGm, rhoGm, xGm, lenGm, wsSeGm, shakeTilingGm);

    /* --- Launch 2：mmad_custom（MIX 1AIC+2AIV，mixPass=0，F203_KEYGEN_EK_PKE=1）---
     * 读 prep GM：src、a_hat、rho；读 Host LUT→ws GM。
     * 写 ek_pke（1568B）、dk_pke（1536B）；dst/t_hat/ek_poly 为内核中间 GM，生产路径不写盘。
     */
    DebugWrite("./output/debug/after_prep_a_hat.bin", aHatGm, kAHatBytes);
    DebugWrite("./output/debug/after_prep_src.bin", srcGm, kSrcBytes);
    DebugWrite("./output/debug/after_prep_rho.bin", rhoGm, kRhoBytes);
    DebugWrite("./output/debug/after_prep_prf.bin", prfGm, kPrfBytes);
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    g_2s1e_mix_pass = vecTilingHost.mixPass;
    ICPU_RUN_KF(mmad_custom, kMmadBlockDim, dstGm, tHatGm, ekPolyGm, skGm, srcGm, aHatGm, wsGm, vecTilingHost, rhoGm,
                ekPkeGm);
    DebugWrite("./output/debug/after_mmad_dst.bin", dstGm, dstFileSize);
    DebugWrite("./output/debug/after_mmad_ek_poly.bin", ekPolyGm, ekPolyBytes);
    DebugWrite("./output/debug/after_mmad_sk.bin", skGm, skFileSize);
    DebugWrite("./output/debug/after_mmad_ek_pke.bin", ekPkeGm, ekPkeBytes);

    if (!WriteFile("./output/ek_pke.bin", ekPkeGm, ekPkeBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_pke.bin", skGm, skFileSize)) {
        return 3;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(rhoGm);
    AscendC::GmFree(xGm);
    AscendC::GmFree(lenGm);
    AscendC::GmFree(wsSeGm);
    AscendC::GmFree(shakeTilingGm);
    AscendC::GmFree(dstGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(ekPolyGm);
    AscendC::GmFree(ekPkeGm);
    AscendC::GmFree(skGm);
    AscendC::GmFree(wsGm);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *rhoDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *wsSeDev = nullptr;
    uint8_t *shakeTilingDev = nullptr;

    uint8_t *dstDev = nullptr;
    uint8_t *tHatDev = nullptr;
    uint8_t *ekPolyDev = nullptr;
    uint8_t *ekPkeHost = nullptr;
    uint8_t *ekPkeDev = nullptr;
    uint8_t *skHost = nullptr;
    uint8_t *skDev = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *wsDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rhoDev), kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kSeXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kSeLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsSeDev), kSeWsBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&shakeTilingDev), kShakeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&dstDev), dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tHatDev), tHatFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPolyDev), ekPolyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), ekPkeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDev), ekPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&skHost), skFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&skDev), skFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&wsHost), wsFileSize));
    std::memcpy(seedHost, &seed_d, kSeedBytes);
    std::memset(wsHost, 0, wsFileSize);
    rs = lutFileSize;
    ReadFile("./input/lut_even_stacked.bin", rs, wsHost + tiling::LUT_EVEN_STACKED, lutFileSize);
    rs = lutFileSize;
    ReadFile("./input/lut_odd_stacked.bin", rs, wsHost + tiling::LUT_ODD_STACKED, lutFileSize);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(shakeTilingDev, kShakeTilingBytes, &shakeTilingHost, kShakeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDev, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *debugHost = nullptr;
    if (KeygenDebugDump()) {
        CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&debugHost), kAHatBytes));
    }

    f203_keygen_prep_do(kPrepBlockDim, nullptr, stream, seedDev, aHatDev, prfDev, srcDev, rhoDev, xDev, lenDev, wsSeDev,
                        shakeTilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    if (debugHost != nullptr) {
        DebugWriteDev(stream, "./output/debug/after_prep_a_hat.bin", aHatDev, kAHatBytes, debugHost);
        DebugWriteDev(stream, "./output/debug/after_prep_src.bin", srcDev, kSrcBytes, debugHost);
        DebugWriteDev(stream, "./output/debug/after_prep_rho.bin", rhoDev, kRhoBytes, debugHost);
        DebugWriteDev(stream, "./output/debug/after_prep_prf.bin", prfDev, kPrfBytes, debugHost);
    }

    ACLRT_LAUNCH_KERNEL(mmad_custom)(kMmadBlockDim, stream, dstDev, tHatDev, ekPolyDev, skDev, srcDev, aHatDev, wsDev,
                                     &vecTilingHost, rhoDev, ekPkeDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(ekPkeHost, ekPkeBytes, ekPkeDev, ekPkeBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(skHost, skFileSize, skDev, skFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    if (debugHost != nullptr) {
        DebugWriteDev(stream, "./output/debug/after_mmad_ek_poly.bin", ekPolyDev, ekPolyBytes, debugHost);
        DebugWriteDev(stream, "./output/debug/after_mmad_sk.bin", skDev, skFileSize, debugHost);
        DebugWriteDev(stream, "./output/debug/after_mmad_ek_pke.bin", ekPkeDev, ekPkeBytes, debugHost);
    }

    if (!WriteFile("./output/ek_pke.bin", ekPkeHost, ekPkeBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_pke.bin", skHost, skFileSize)) {
        return 3;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFree(wsSeDev));
    CHECK_ACL(aclrtFree(shakeTilingDev));
    CHECK_ACL(aclrtFree(dstDev));
    CHECK_ACL(aclrtFree(tHatDev));
    CHECK_ACL(aclrtFree(ekPolyDev));
    CHECK_ACL(aclrtFree(ekPkeDev));
    CHECK_ACL(aclrtFree(skDev));
    CHECK_ACL(aclrtFree(wsDev));
    if (debugHost != nullptr) {
        CHECK_ACL(aclrtFreeHost(debugHost));
    }
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
    CHECK_ACL(aclrtFreeHost(skHost));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
