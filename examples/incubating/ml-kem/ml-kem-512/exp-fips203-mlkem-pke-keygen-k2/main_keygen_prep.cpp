// @probe exp-fips203-mlkem-pke-keygen-k2
// @file main_keygen_prep.cpp
// @layer host
// @role prep 子工程 host 入口（单独调试 prep launch）。 / Prep-only host main.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: data_utils.h, f203_keygen_prep_layout.h, shake_general_tiling_data.h, tiling_host.hpp, cstdint, cstring, iostream, acl/acl.h, tikicpulib.h
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file main_keygen_prep.cpp
 * @brief Host：prep 子工程入口 — SEED_D → 单内核 `f203_keygen_prep` → a_hat + src GM。
 *
 * ## 流水线位置
 * 单独调试 Launch 1（Alg.13 行 3–15）：AIV×2 生成 Â、ŝ/ê（src）、ρ、prf_out。
 * 生产全链由 `main_keygen.cpp` 串联 prep+compute；本文件供 prep 子 CMake / 门控调试。
 *
 * ## 与 golden 关系
 * 写出 `output/a_hat.bin`、`src.bin`、`prf_out.bin`、`rho.bin` 供对拍；
 * 验收仅 I/O 等价，禁止把 Host 流程当作 AscendC 实现规格。
 *
 * ## 输入
 * `input/seed_d.bin`（uint32 LE）；SHAKE tiling 由 Host `FillShakeTiling` 填充。
 */
#include "data_utils.h"
#include "f203_keygen_prep_layout.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm);
#endif

namespace {
using namespace F203KeygenPrep;

/** ShakeGeneralTilingData 字节数（拷入 se_tiling GM） */
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);
}  // namespace

/**
 * prep-only Host：读 seed → launch f203_keygen_prep → 落盘中间 GM（调试用）。
 * @return 0 成功；1 读 seed 失败；2–5 写 a_hat/src/prf/rho 失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // --- 读生产种子 d（32-bit LE）---
    uint32_t seed_d = 0U;
    size_t rs = 0;
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return 1;
    }

    // presample SHAKE batch tiling：batch/PRF 长度与 prep 双 AIV 一致
    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, kSeBatch, kSeMaxMsgLen, kSePrfOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = kPrepBlockDim;

    std::cout << "[main] f203_keygen_prep SEED_D=" << seed_d << " blockDim=" << kPrepBlockDim
              << " a_hat_bytes=" << kAHatBytes << " src_bytes=" << kSrcBytes << "\n";

#ifdef __CCE_KT_TEST__
    // --- CPU 孪生：分配 prep 全套 GM 并 ICPU_RUN_KF ---
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *srcGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSrcBytes));
    uint8_t *rhoGm = static_cast<uint8_t *>(AscendC::GmAlloc(kRhoBytes));
    uint8_t *xGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeXBytes));
    uint8_t *lenGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeLenBytes));
    uint8_t *wsGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeWsBytes));
    uint8_t *tilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kTilingBytes));

    // Host→GM：种子与 tiling；其余输出 GM 由核写入
    std::memcpy(seedGm, &seed_d, kSeedBytes);
    std::memcpy(tilingGm, &tilingHost, kTilingBytes);

    ICPU_RUN_KF(f203_keygen_prep, kPrepBlockDim, seedGm, aHatGm, prfGm, srcGm, rhoGm, xGm, lenGm, wsGm, tilingGm);

    // 调试落盘：Â / src(ŝ‖ê) / PRF 中间 / ρ
    if (!WriteFile("./output/a_hat.bin", aHatGm, kAHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcGm, kSrcBytes)) {
        return 3;
    }
    if (!WriteFile("./output/prf_out.bin", prfGm, kPrfBytes)) {
        return 4;
    }
    if (!WriteFile("./output/rho.bin", rhoGm, kRhoBytes)) {
        return 5;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(srcGm);
    AscendC::GmFree(rhoGm);
    AscendC::GmFree(xGm);
    AscendC::GmFree(lenGm);
    AscendC::GmFree(wsGm);
    AscendC::GmFree(tilingGm);
#else
    // --- SIM/NPU：ACL 分配 Host/Device，H2D → prep_do → D2H 写盘 ---
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *prfHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *rhoHost = nullptr;
    uint8_t *tilingHostBuf = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *srcDev = nullptr;
    uint8_t *rhoDev = nullptr;
    uint8_t *xDev = nullptr;
    uint8_t *lenDev = nullptr;
    uint8_t *wsDev = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&prfHost), kPrfBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&srcHost), kSrcBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&rhoHost), kRhoBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), kTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&srcDev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rhoDev), kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xDev), kSeXBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&lenDev), kSeLenBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&wsDev), kSeWsBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_keygen_prep_do(kPrepBlockDim, nullptr, stream, seedDev, aHatDev, prfDev, srcDev, rhoDev, xDev, lenDev,
                        wsDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(srcHost, kSrcBytes, srcDev, kSrcBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(prfHost, kPrfBytes, prfDev, kPrfBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(rhoHost, kRhoBytes, rhoDev, kRhoBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes)) {
        return 2;
    }
    if (!WriteFile("./output/src.bin", srcHost, kSrcBytes)) {
        return 3;
    }
    if (!WriteFile("./output/prf_out.bin", prfHost, kPrfBytes)) {
        return 4;
    }
    if (!WriteFile("./output/rho.bin", rhoHost, kRhoBytes)) {
        return 5;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(srcDev));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFree(xDev));
    CHECK_ACL(aclrtFree(lenDev));
    CHECK_ACL(aclrtFree(wsDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(prfHost));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
