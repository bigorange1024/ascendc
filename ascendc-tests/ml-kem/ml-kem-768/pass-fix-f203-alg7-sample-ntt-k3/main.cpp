/**
 * @file main.cpp
 * @brief Alg.7 SampleNTT 探针 Host 入口：读 SEED_D+(j,i) → 启动设备核 → 写 output 各 bin 对拍。
 *
 * 流水线位置：
 *   run.sh → gen_data.py 生成 input/golden → 本 main → f203_alg7_sample_ntt_d12（设备）
 *   → output/{xof,d1,d2,a_hat}.bin → verify_result.py。
 *   SHAKE128 全在设备单 TPipe 内完成；Host 不展开 XOF，仅搬运标量输入与对拍输出。
 *
 * 与 golden 关系：
 *   - input/seed_d.bin：uint32 LE
 *   - input/poly_ij.bin：byte j, byte i
 *   - output/a_hat.bin：256×int32，与 golden/a_hat.bin 逐字比较
 *   - d1/d2/xof 为中间量对拍（xof 仅 F203_ALG7_DUMP_XOF=1 时有效）
 *
 * 双路径：__CCE_KT_TEST__ 为 CPU 孪生（GmAlloc + ICPU_RUN_KF）；否则 ACL H2D/D2H + stream 同步。
 */
#include "data_utils.h"
#include "f203_alg7_config.h"
#include "f203_alg7_layout.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_alg7_sample_ntt_d12_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                            uint8_t *poly_ij_gm, uint8_t *xof_gm, uint8_t *d1_gm, uint8_t *d2_gm,
                                            uint8_t *a_hat_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_alg7_sample_ntt_d12(GM_ADDR seed_d_gm, GM_ADDR poly_ij_gm, GM_ADDR xof_gm,
                                                                GM_ADDR d1_gm, GM_ADDR d2_gm, GM_ADDR a_hat_gm);
#endif

namespace {
/** 探针锁定：单 AIV、单 poly，与 entry.cpp 一致。 */
constexpr uint32_t kBlockDim = 1U;
constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kIjBytes = 2U;
constexpr size_t kXofBytes = F203Alg7::kXofBytes;       // 672
constexpr size_t kD12Bytes = F203Alg7::kD12Bytes;     // 896 = 224×4
constexpr size_t kAHatBytes = F203Alg7::kAHatBytes;     // 1024 = 256×4
}  // namespace

/**
 * Host main：分配 GM/设备内存、启动核、落盘 output。
 * @return 0 成功；1 读入失败；2–5 写出失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t seed_d = 20260619U;  // 仅当读文件失败时的占位（正常由 seed_d.bin 覆盖）
    uint8_t poly_ij[2] = {0U, 0U};
    size_t rs = 0;

    // 从 input/ 读入对拍输入（尺寸须与 layout.h 严格一致）
    if (!ReadFile("./input/seed_d.bin", rs, &seed_d, kSeedBytes) || rs != kSeedBytes) {
        std::cerr << "[FAIL] read input/seed_d.bin\n";
        return 1;
    }
    if (!ReadFile("./input/poly_ij.bin", rs, poly_ij, kIjBytes) || rs != kIjBytes) {
        std::cerr << "[FAIL] read input/poly_ij.bin\n";
        return 1;
    }

    // 打印当前编译期 rej 路径，便于 SIM/CPU 日志区分对照组
    const char *rejTag = "unknown";
    switch (F203_ALG7_REJ_IMPL) {
    case F203_ALG7_REJ_SCALAR:
        rejTag = "scalar";
        break;
    case F203_ALG7_REJ_VEC_MINS:
        rejTag = "vec_mins";
        break;
    case F203_ALG7_REJ_VEC_MASK:
        rejTag = "vec_mask";
        break;
    default:
        break;
    }
    std::cout << "[main] f203_alg7_sample_ntt_d12 SEED_D=" << seed_d << " j=" << static_cast<unsigned>(poly_ij[0])
              << " i=" << static_cast<unsigned>(poly_ij[1]) << " rej_impl=" << F203_ALG7_REJ_IMPL << "(" << rejTag
              << ") blockDim=" << kBlockDim << "\n";

#ifdef __CCE_KT_TEST__
    // CPU 孪生：tikicpulib 模拟 GM，直接 ICPU_RUN_KF 调用设备核源码
    uint8_t *seedGm = static_cast<uint8_t *>(AscendC::GmAlloc(kSeedBytes));
    uint8_t *ijGm = static_cast<uint8_t *>(AscendC::GmAlloc(kIjBytes));
    uint8_t *xofGm = static_cast<uint8_t *>(AscendC::GmAlloc(kXofBytes));
    uint8_t *d1Gm = static_cast<uint8_t *>(AscendC::GmAlloc(kD12Bytes));
    uint8_t *d2Gm = static_cast<uint8_t *>(AscendC::GmAlloc(kD12Bytes));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));

    std::memcpy(seedGm, &seed_d, kSeedBytes);
    std::memcpy(ijGm, poly_ij, kIjBytes);

    ICPU_RUN_KF(f203_alg7_sample_ntt_d12, kBlockDim, seedGm, ijGm, xofGm, d1Gm, d2Gm, aHatGm);

    if (!WriteFile("./output/xof.bin", xofGm, kXofBytes)) {
        return 2;
    }
    if (!WriteFile("./output/d1.bin", d1Gm, kD12Bytes)) {
        return 3;
    }
    if (!WriteFile("./output/d2.bin", d2Gm, kD12Bytes)) {
        return 4;
    }
    if (!WriteFile("./output/a_hat.bin", aHatGm, kAHatBytes)) {
        return 5;
    }

    AscendC::GmFree(seedGm);
    AscendC::GmFree(ijGm);
    AscendC::GmFree(xofGm);
    AscendC::GmFree(d1Gm);
    AscendC::GmFree(d2Gm);
    AscendC::GmFree(aHatGm);
#else
    // SIM/NPU：ACL 分配 Host/Device 缓冲，H2D 输入 → kernel → D2H 输出
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seedHost = nullptr;
    uint8_t *ijHost = nullptr;
    uint8_t *xofHost = nullptr;
    uint8_t *d1Host = nullptr;
    uint8_t *d2Host = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *ijDev = nullptr;
    uint8_t *xofDev = nullptr;
    uint8_t *d1Dev = nullptr;
    uint8_t *d2Dev = nullptr;
    uint8_t *aHatDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seedHost), kSeedBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ijHost), kIjBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&xofHost), kXofBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&d1Host), kD12Bytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&d2Host), kD12Bytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seedDev), kSeedBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ijDev), kIjBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&xofDev), kXofBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&d1Dev), kD12Bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&d2Dev), kD12Bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    std::memcpy(seedHost, &seed_d, kSeedBytes);
    std::memcpy(ijHost, poly_ij, kIjBytes);
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedBytes, seedHost, kSeedBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(ijDev, kIjBytes, ijHost, kIjBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_alg7_sample_ntt_d12_do(kBlockDim, nullptr, stream, seedDev, ijDev, xofDev, d1Dev, d2Dev, aHatDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(xofHost, kXofBytes, xofDev, kXofBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(d1Host, kD12Bytes, d1Dev, kD12Bytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(d2Host, kD12Bytes, d2Dev, kD12Bytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/xof.bin", xofHost, kXofBytes)) {
        return 2;
    }
    if (!WriteFile("./output/d1.bin", d1Host, kD12Bytes)) {
        return 3;
    }
    if (!WriteFile("./output/d2.bin", d2Host, kD12Bytes)) {
        return 4;
    }
    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes)) {
        return 5;
    }

    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(ijDev));
    CHECK_ACL(aclrtFree(xofDev));
    CHECK_ACL(aclrtFree(d1Dev));
    CHECK_ACL(aclrtFree(d2Dev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(ijHost));
    CHECK_ACL(aclrtFreeHost(xofHost));
    CHECK_ACL(aclrtFreeHost(d1Host));
    CHECK_ACL(aclrtFreeHost(d2Host));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
