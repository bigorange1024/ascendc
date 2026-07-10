// @probe pass-fix-f203-alg13-device-keygen-k4
// @file main.cpp
// @layer legacy
// @role legacy/staged host 入口或旧编排；生产以 main_keygen + run.sh 为准。 / Legacy host entry.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. 本文件可能用于 legacy/staged I/O 或分阶段调试，非默认生产路径。 / May use legacy staging I/O.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: data_utils.h, f203_keygen_layout.h, iostream, acl/acl.h, tikicpulib.h
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file main.cpp
 * @brief Host：G1 门控 — `input/ek_polyvec.bin` + `input/rho.bin` → `output/ek_pke.bin`。
 *
 * ## 流水线位置
 * 本文件为 **legacy / 分阶段调试** Host：仅 launch `f203_keygen_ek_append`（Alg.13 行 21 拼接）。
 * 生产全链以 `main_keygen.cpp` + `run.sh` 为准（行 21 已在 `mmad_custom` 内 `F203_KEYGEN_EK_PKE` 融合）。
 *
 * ## 与 golden 关系
 * 验收仅 I/O 等价：`ek_pke` = ByteEncode₁₂(t̂) ‖ ρ（1568B）；禁止把本 Host 当作 AscendC 规格。
 *
 * ## 输入 / 输出
 * - 入：`ek_polyvec` 1536B、`rho` 32B（均由上游门控或 golden 预置）
 * - 出：`ek_pke` 1568B
 */
#include "data_utils.h"
#include "f203_keygen_layout.h"

#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_keygen_ek_append_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_polyvec_gm,
                                         uint8_t *rho_gm, uint8_t *ek_pke_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_keygen_ek_append(GM_ADDR ek_polyvec_gm, GM_ADDR rho_gm, GM_ADDR ek_pke_gm);
#endif

namespace {
/** AIV_ONLY 单核拼接；blockDim 固定 1 */
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kEkPolyvecBytes = F203Keygen::kEkPolyvecBytes;
constexpr size_t kRhoBytes = F203Keygen::kRhoBytes;
constexpr size_t kEkPkeBytes = F203Keygen::kEkPkeBytes;
}  // namespace

/**
 * G1 Host 主流程：读 ek_polyvec/ρ → launch ek_append → 写 ek_pke。
 * @return 0 成功；1 读盘失败；2 写盘失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "[main] f203_keygen_ek_append ek_polyvec=" << kEkPolyvecBytes << " rho=" << kRhoBytes
              << " ek_pke=" << kEkPkeBytes << "\n";

#ifdef __CCE_KT_TEST__
    // --- CPU 孪生：GmAlloc 直接当 Host/Device 统一缓冲 ---
    uint8_t *ekGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkPolyvecBytes));
    uint8_t *rhoGm = static_cast<uint8_t *>(AscendC::GmAlloc(kRhoBytes));
    uint8_t *outGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkPkeBytes));
    size_t rs = 0;

    if (!ReadFile("./input/ek_polyvec.bin", rs, ekGm, kEkPolyvecBytes) || rs != kEkPolyvecBytes) {
        std::cerr << "[FAIL] input/ek_polyvec.bin\n";
        return 1;
    }
    if (!ReadFile("./input/rho.bin", rs, rhoGm, kRhoBytes) || rs != kRhoBytes) {
        std::cerr << "[FAIL] input/rho.bin\n";
        return 1;
    }

    ICPU_RUN_KF(f203_keygen_ek_append, kBlockDim, ekGm, rhoGm, outGm);

    if (!WriteFile("./output/ek_pke.bin", outGm, kEkPkeBytes)) {
        return 2;
    }

    AscendC::GmFree(ekGm);
    AscendC::GmFree(rhoGm);
    AscendC::GmFree(outGm);
#else
    // --- SIM/NPU：Host 读盘 → H2D → ACL launch → D2H 写盘 ---
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekHost = nullptr;
    uint8_t *rhoHost = nullptr;
    uint8_t *outHost = nullptr;
    uint8_t *ekDev = nullptr;
    uint8_t *rhoDev = nullptr;
    uint8_t *outDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkPolyvecBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&rhoHost), kRhoBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&outHost), kEkPkeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkPolyvecBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rhoDev), kRhoBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&outDev), kEkPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/ek_polyvec.bin", rs, ekHost, kEkPolyvecBytes) || rs != kEkPolyvecBytes) {
        return 1;
    }
    if (!ReadFile("./input/rho.bin", rs, rhoHost, kRhoBytes) || rs != kRhoBytes) {
        return 1;
    }

    CHECK_ACL(aclrtMemcpy(ekDev, kEkPolyvecBytes, ekHost, kEkPolyvecBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(rhoDev, kRhoBytes, rhoHost, kRhoBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_keygen_ek_append_do(kBlockDim, nullptr, stream, ekDev, rhoDev, outDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, kEkPkeBytes, outDev, kEkPkeBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/ek_pke.bin", outHost, kEkPkeBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(rhoDev));
    CHECK_ACL(aclrtFree(outDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
