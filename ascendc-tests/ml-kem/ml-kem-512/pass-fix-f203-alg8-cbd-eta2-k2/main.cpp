/**
 * @file main.cpp
 * @brief Alg.8 CBD η=2 探针 Host：读 prf_out[4,128] → 启动核 → 写 output/src.bin。
 *
 * 流水线位置：读取 `scripts/gen_data.py` 产出的 `input/prf_out.bin`，下发设备核
 * `f203_cbd_eta2_batch4`，并把 `src[4,256]` int32 结果落盘。CPU 孪生走
 * `ICPU_RUN_KF`；SIM/NPU 走 ACL 初始化、Device 内存分配、核函数 launch、同步与回读。
 * golden 对拍在 `scripts/verify_result.py` 中完成，本文件不内嵌 oracle。
 */
#include "data_utils.h"
#include "f203_cbd_eta2_config.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_cbd_eta2_batch4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *prf_gm,
                                        uint8_t *src_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_cbd_eta2_batch4(GM_ADDR prf_gm, GM_ADDR src_gm);
#endif

namespace {
constexpr size_t kPrfBytes = F203CbdEta2Host::PRF_TOTAL_BYTES;  // 4*128=512
constexpr size_t kSrcBytes = static_cast<size_t>(F203CbdEta2Host::SRC_COEFFS) * sizeof(int32_t);  // 4*256*4=4096
}  // namespace

/**
 * 主流程：读输入 → 分配 GM/Device 缓冲 → 执行核函数 → 写输出。
 * @return 0=成功；1=输入读取失败；2=输出写入失败
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

    std::cout << "[main] Alg.8 CBD eta=2 k=2 rows=" << F203CbdEta2Host::ROWS << " prf_bytes=" << kPrfBytes
              << " launch_blockDim=" << kHostBlockDim << " F203_CBD_BLOCK_DIM=" << F203_CBD_BLOCK_DIM << "\n";

#ifdef __CCE_KT_TEST__
    /* CPU 孪生：固定 launch blockDim=1，避免 tikicpu 按 blockDim 额外 fork AIC/AIV 进程。
     * 默认设备代码仍编译为 P2 双 AIV；运行时 `GetBlockNum()==1` 时 block0 串行处理 4 行。 */
    constexpr uint32_t kCpuLaunchBlockDim = 1U;
    std::cout << "[main] CPU twin launch_blockDim=" << kCpuLaunchBlockDim
              << " (P2 kernel serializes 4 rows when GetBlockNum()==1)\n";
    uint8_t *prf_gm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    int32_t *src_gm = static_cast<int32_t *>(AscendC::GmAlloc(kSrcBytes));
    std::memcpy(prf_gm, prf_host, kPrfBytes);

    ICPU_RUN_KF(f203_cbd_eta2_batch4, kCpuLaunchBlockDim, prf_gm, reinterpret_cast<uint8_t *>(src_gm));

    if (!WriteFile("./output/src.bin", src_gm, kSrcBytes)) {
        return 2;
    }
    AscendC::GmFree(prf_gm);
    AscendC::GmFree(src_gm);
#else
    /* SIM/NPU：按标准 KernelLaunch 壳初始化 ACL，H2D 拷入 PRF，launch 默认双 AIV，
     * 同步后 D2H 回读整块 src。所有数值正确性留给 verify_result.py。 */
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

    f203_cbd_eta2_batch4_do(kHostBlockDim, nullptr, stream, prf_dev, reinterpret_cast<uint8_t *>(src_dev));
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
