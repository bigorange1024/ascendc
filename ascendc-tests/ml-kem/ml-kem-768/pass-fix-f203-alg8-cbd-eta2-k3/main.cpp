/**
 * @file main.cpp
 * @brief Alg.8 CBD η=2 探针 Host：读 prf_out[6,128] → 启动核 → 写 output/src.bin（ML-KEM-768）。
 *
 * 流水线位置：读取 `scripts/gen_data.py` 产出的 `input/prf_out.bin`（6 行 PRF
 * 输出，行优先 uint8），下发设备核 `f203_cbd_eta2_batch6`（CPU 孪生走
 * ICPU_RUN_KF，SIM/NPU 走 acl 下发），落盘计算结果到 `output/src.bin`（6 行
 * int32 系数）。与 golden 的关系：本文件只负责跑通计算并落盘，真正的逐元素
 * 对拍在 `scripts/verify_result.py` 中完成（与
 * `library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
 * 产出的 golden_src.bin 比对），验收标准为 I/O 等价，不要求实现同构。
 */
#include "data_utils.h"
#include "f203_cbd_eta2_config.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_cbd_eta2_batch6_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *prf_gm,
                                        uint8_t *src_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_cbd_eta2_batch6(GM_ADDR prf_gm, GM_ADDR src_gm);
#endif

namespace {
constexpr size_t kPrfBytes = F203CbdEta2Host::PRF_TOTAL_BYTES;               // 输入总字节数：6*128=768
constexpr size_t kSrcBytes = static_cast<size_t>(F203CbdEta2Host::SRC_COEFFS) * sizeof(int32_t);  // 输出总字节数：6*256*4=6144
}  // namespace

/**
 * 主流程：读 input/prf_out.bin → 构造 GM 缓冲 → 下发核函数（CPU 孪生 / SIM-NPU
 * 二选一编译分支）→ 落盘 output/src.bin。
 * @return 0=成功；1=读输入文件失败；2=写输出文件失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t rs = 0;
    uint8_t prf_host[kPrfBytes];
    if (!ReadFile("./input/prf_out.bin", rs, prf_host, kPrfBytes) || rs != kPrfBytes) {
        std::cerr << "[FAIL] read input/prf_out.bin\n";
        return 1;
    }

    std::cout << "[main] Alg.8 CBD eta=2 k=3 rows=" << F203CbdEta2Host::ROWS << " prf_bytes=" << kPrfBytes
              << " launch_blockDim=" << kHostBlockDim << " F203_CBD_BLOCK_DIM=" << F203_CBD_BLOCK_DIM << "\n";

#ifdef __CCE_KT_TEST__
    /* CPU 孪生：910B 每 blockDim 会 fork 1 AIC+2 AIV；AIV_ONLY 探针固定 launch=1。
     * 背景：常量仅 CPU 分支使用；若放在共同命名空间，SIM/Clang -Werror=-Wunused-const-variable 会挂。 */
    constexpr uint32_t kCpuLaunchBlockDim = 1U;
    /* CPU 孪生路径：固定 launch blockDim=1（AIV_ONLY 探针避免 tikicpu 按 launch
     * blockDim 误 fork 出多颗 AIC 进程）；核内若检测到 GetBlockNum()==1 会自动
     * 退化为单核串行 6 行，与 P2 双核结果语义一致。GM 缓冲用 AscendC::GmAlloc
     * 模拟设备内存，计算完成后直接从该内存落盘（无需显式 Device→Host 拷贝）。 */
    std::cout << "[main] CPU twin launch_blockDim=" << kCpuLaunchBlockDim
              << " (P2 kernel serializes 6 rows when GetBlockNum()==1)\n";
    uint8_t *prf_gm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    int32_t *src_gm = static_cast<int32_t *>(AscendC::GmAlloc(kSrcBytes));
    std::memcpy(prf_gm, prf_host, kPrfBytes);

    ICPU_RUN_KF(f203_cbd_eta2_batch6, kCpuLaunchBlockDim, prf_gm, reinterpret_cast<uint8_t *>(src_gm));

    if (!WriteFile("./output/src.bin", src_gm, kSrcBytes)) {
        return 2;
    }
    AscendC::GmFree(prf_gm);
    AscendC::GmFree(src_gm);
#else
    /* SIM/NPU 路径：acl 初始化 → 建流 → 分配 Device 侧输入/输出缓冲 + Host 侧输出
     * 回读缓冲 → Host→Device 拷入 PRF → 下发核函数（kHostBlockDim，探针默认 2，
     * 即 P2 双 AIV）→ 同步 → Device→Host 拷回结果 → 落盘。 */
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *prf_dev = nullptr;
    int32_t *src_dev = nullptr;
    int32_t *src_host = nullptr;

    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prf_dev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&src_dev), kSrcBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&src_host), kSrcBytes));

    CHECK_ACL(aclrtMemcpy(prf_dev, kPrfBytes, prf_host, kPrfBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_cbd_eta2_batch6_do(kHostBlockDim, nullptr, stream, prf_dev, reinterpret_cast<uint8_t *>(src_dev));
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(src_host, kSrcBytes, src_dev, kSrcBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/src.bin", src_host, kSrcBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(prf_dev));
    CHECK_ACL(aclrtFree(src_dev));
    CHECK_ACL(aclrtFreeHost(src_host));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif

    return 0;
}
