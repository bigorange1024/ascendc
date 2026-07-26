/*
 * Host 驱动：Launch1 f203_se_vector_k4 (1×AIV) → Launch2 mmad_custom MIX (1×AIC+2×AIV)
 * 验收至 Alg.13 行 17（mixPass=5：S1+S2+S3，无行 18–20）。
 *
 * 同一 GM src 缓冲：Launch1 写、Launch2 读（无 Host 注入 src.bin）。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "chain_ntt17_layout.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifdef ASCENDC_CPU_DEBUG
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" __global__ __aicore__ void f203_se_vector_k4(GM_ADDR seed_d_gm, GM_ADDR prf_out_gm, GM_ADDR src_gm,
                                                        GM_ADDR x_gm, GM_ADDR lengths_gm, GM_ADDR workspace,
                                                        GM_ADDR tiling);
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling);
extern volatile int g_2s1e_mix_pass;
#else
#include "acl/acl.h"
#if defined(F203_CHAIN_NTT17_SIM_DLOPEN)
#include <dlfcn.h>
#else
#include "aclrtlaunch_mmad_custom.h"
#endif
extern "C" void f203_se_vector_k4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                     uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *x_gm, uint8_t *lengths_gm,
                                     uint8_t *workspace, uint8_t *tiling);
#endif

namespace {
constexpr uint32_t kSeBlockDim = 1U;
constexpr uint32_t kNttBlockDim = 1U;
constexpr uint32_t kSeBatch = 8U;
constexpr uint32_t kSeMaxMsgLen = 33U;
constexpr uint32_t kSeOutLen = 128U;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kSeBatch) * kSeOutLen;
constexpr size_t kSrcBytes = 8U * 256U * sizeof(int32_t);
constexpr size_t kDstBytes = chain_ntt17::dstFileBytes;
constexpr size_t kTHatBytes = chain_ntt17::tHatFileBytes;
constexpr size_t kAHatBytes = chain_ntt17::aHatFileBytes;
constexpr size_t kLutBytes = chain_ntt17::lutEvenOddFileBytes;
constexpr size_t kWsBytes = chain_ntt17::wssize;
constexpr size_t kSeTilingBytes = sizeof(ShakeGeneralTilingData);
constexpr size_t kNttTilingBytes = 64U;
constexpr size_t kSeXBytes = static_cast<size_t>(kSeBatch) * kSeMaxMsgLen;
constexpr size_t kSeLenBytes = static_cast<size_t>(kSeBatch) * sizeof(uint32_t);
constexpr size_t kEkSkBytes = chain_ntt17::kEkSkBytes;
}  // namespace

static bool ReadSeedD(uint32_t &seed_d)
{
    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return false;
    }
    return true;
}

static void InitSeTiling(ShakeGeneralTilingData &tilingHost)
{
    FillShakeTiling(&tilingHost, kSeBatch, kSeMaxMsgLen, kSeOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kSeBlockDim;
}

static bool ReadNttTiling(TilingData &nttTiling)
{
    uint8_t buf[kNttTilingBytes]{};
    size_t rs = 0;
    if (!ReadFile("./input/tiling_ntt.bin", rs, buf, kNttTilingBytes) || rs < sizeof(TilingData)) {
        std::cerr << "[FAIL] read input/tiling_ntt.bin\n";
        return false;
    }
    std::memcpy(&nttTiling, buf, sizeof(TilingData));
    return true;
}

#ifdef ASCENDC_CPU_DEBUG
static int RunChainCpu(uint32_t seed_d)
{
    ShakeGeneralTilingData seTilingHost{};
    InitSeTiling(seTilingHost);

    TilingData nttTiling{};
    if (!ReadNttTiling(nttTiling)) {
        return 1;
    }
    if (nttTiling.mixPass != 5) {
        std::cerr << "[FAIL] chain expects mixPass=5, got " << nttTiling.mixPass << "\n";
        return 1;
    }
    g_2s1e_mix_pass = nttTiling.mixPass;

    AscendC::SetKernelMode(KernelMode::MIX_MODE);

    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *xGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeXBytes));
    uint8_t *lenGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeLenBytes));
    uint8_t *seWsGm = static_cast<uint8_t *>(AscendC::GmAlloc(64));
    uint8_t *seTilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeTilingBytes));

    uint8_t *dstGm = static_cast<uint8_t *>(AscendC::GmAlloc(kDstBytes));
    uint8_t *tHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kTHatBytes));
    uint8_t *ekGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkSkBytes));
    uint8_t *skGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkSkBytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *nttWsGm = static_cast<uint8_t *>(AscendC::GmAlloc(kWsBytes));
    uint8_t *nttTilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kNttTilingBytes));

    std::memcpy(seedGm, &seed_d, kSeedBytes);
    std::memcpy(seTilingGm, &seTilingHost, kSeTilingBytes);
    std::memcpy(nttTilingGm, &nttTiling, sizeof(TilingData));

    size_t rs = 0;
    if (!ReadFile("./input/a_hat.bin", rs, aHatGm, kAHatBytes) || rs != kAHatBytes) {
        return 10;
    }
    rs = 0;
    if (!ReadFile("./input/lut_even_stacked.bin", rs, nttWsGm + chain_ntt17::LUT_EVEN_STACKED, kLutBytes) ||
        rs != kLutBytes) {
        return 11;
    }
    rs = 0;
    if (!ReadFile("./input/lut_odd_stacked.bin", rs, nttWsGm + chain_ntt17::LUT_ODD_STACKED, kLutBytes) ||
        rs != kLutBytes) {
        return 12;
    }

    std::cout << "[chain] Launch1 f203_se_vector_k4 blockDim=" << kSeBlockDim << " SEED_D=" << seed_d << "\n";
    ICPU_RUN_KF(f203_se_vector_k4, kSeBlockDim, seedGm, prfGm, srcGm, xGm, lenGm, seWsGm, seTilingGm);

    std::cout << "[chain] Launch2 mmad_custom MIX blockDim=" << kNttBlockDim << " mixPass=" << nttTiling.mixPass
              << "\n";
    ICPU_RUN_KF(mmad_custom, kNttBlockDim, dstGm, tHatGm, ekGm, skGm, srcGm, aHatGm, nttWsGm, nttTiling);

    if (!WriteFile("./output/prf_out.bin", prfGm, kPrfBytes)) {
        return 20;
    }
    if (!WriteFile("./output/src.bin", srcGm, kSrcBytes)) {
        return 21;
    }
    if (!WriteFile("./output/dst.bin", dstGm, kDstBytes)) {
        return 22;
    }
    if (!WriteFile("./output/s0.bin", nttWsGm + chain_ntt17::S0, chain_ntt17::s0FileBytes)) {
        return 23;
    }
    if (!WriteFile("./output/mat_c.bin", nttWsGm + chain_ntt17::MAT_C_PLANAR, chain_ntt17::matCFileBytes)) {
        return 24;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(xGm);
    AscendC::GmFree(lenGm);
    AscendC::GmFree(seWsGm);
    AscendC::GmFree(seTilingGm);
    AscendC::GmFree(dstGm);
    AscendC::GmFree(tHatGm);
    AscendC::GmFree(ekGm);
    AscendC::GmFree(skGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(nttWsGm);
    AscendC::GmFree(nttTilingGm);
    return 0;
}
#endif

#ifndef ASCENDC_CPU_DEBUG
#if defined(F203_CHAIN_NTT17_SIM_DLOPEN)
using MmadLaunchFn = uint32_t (*)(uint32_t, aclrtStream, void *, void *, void *, void *, void *, void *, void *,
                                  TilingData *);
#endif

static int RunChainSim(uint32_t seed_d)
{
    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, kSeBatch, kSeMaxMsgLen, kSeOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kSeBlockDim;

#if defined(F203_CHAIN_NTT17_SIM_DLOPEN)
    void *nttLib = nullptr;
    MmadLaunchFn launchMmad = nullptr;
#endif

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *prfHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *tilingDev = nullptr;
    uint8_t *tilingHostBuf = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prfHost), kPrfBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), kSeTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kSeXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kSeLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), 64, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), kSeTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    std::memcpy(tilingHostBuf, &tilingHost, kSeTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kSeTilingBytes, tilingHostBuf, kSeTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    std::cout << "[chain] Launch1 f203_se_vector_k4 blockDim=" << kSeBlockDim << " SEED_D=" << seed_d << "\n";
    f203_se_vector_k4_do(kSeBlockDim, nullptr, stream, seedDev, prfDev, srcDev, xDev, lenDev, wsDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

#include "main_chain_ntt17_launch2.inc"
}
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t seed_d = 0;
    if (!ReadSeedD(seed_d)) {
        return 1;
    }

    std::cout << "[main] f203_se_chain_ntt17 SEED_D=" << seed_d << "\n";

#ifdef ASCENDC_CPU_DEBUG
    return RunChainCpu(seed_d);
#else
    return RunChainSim(seed_d);
#endif
}
