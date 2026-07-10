/**
 * @file main.cpp
 * @brief Compress_d 探针 host 侧驱动：input/poly.bin → output/comp.bin
 *
 * 本文件在流水线中的位置：位于设备 kernel（compress_d_custom.cpp）之外的 host 程序，
 * 按编译宏 ASCENDC_CPU_DEBUG 二选一走 CPU 孪生（ICPU_RUN_KF）或真实设备/SIM（ACL 接口）路径；
 * 负责读入 scripts/gen_data.py 生成的 input/poly.bin、发起一次 kernel 计算、把结果写回
 * output/comp.bin。与 golden 的关系：本文件不做任何压缩计算，仅搬运数据；正确性由
 * scripts/verify_result.py 对比 output/comp.bin 与 output/golden_comp.bin 判定。
 */
#include "data_utils.h"
#include "f203_mlkem_params.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_compress_d_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void compress_d_custom(GM_ADDR poly_in, GM_ADDR comp_out, int32_t coeff_n);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // 单个多项式 256 个 int32 系数的字节数，input/output bin 文件与 GM/host 缓冲均按此大小分配。
    constexpr size_t polyBytes = static_cast<size_t>(F203_MLKEM_N) * sizeof(int32_t);
    uint32_t blockDim = 1;  // 本探针纯 per-lane 运算，单核即可，不做多核切分。

#ifdef ASCENDC_CPU_DEBUG
    // ---- CPU 孪生路径：不经过真实 NPU，用 tikicpulib 在 host 进程内模拟 kernel 执行 ----
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    // GmAlloc 分配的是 CPU 孪生用的“仿真 GM”内存；分配尺寸对齐到至少 1024 字节，避免小 buffer 越界告警。
    uint8_t *poly_in = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);
    uint8_t *comp_out = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/poly.bin", rs, poly_in, polyBytes) || rs != polyBytes) {
        return 1;
    }

    // 直接在 host 进程内“调起”kernel 函数（不走真实 launch），blockDim=1 对应单核。
    ICPU_RUN_KF(compress_d_custom, blockDim, poly_in, comp_out, F203_MLKEM_N);

    if (!WriteFile("./output/comp.bin", comp_out, polyBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)poly_in);
    AscendC::GmFree((void *)comp_out);
#else
    // ---- 真实设备 / SIM 路径：走标准 ACL 初始化 → 建流 → host/device 内存搬运 → launch → 同步 → 回读 ----
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *polyHost = nullptr;
    uint8_t *compHost = nullptr;
    uint8_t *polyDevice = nullptr;
    uint8_t *compDevice = nullptr;

    // Host 侧使用锁页内存（MallocHost）加速 H2D/D2H；Device 侧用 huge-page-first 策略分配显存。
    CHECK_ACL(aclrtMallocHost((void **)(&polyHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&polyDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&compHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&compDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/poly.bin", rs, polyHost, polyBytes) || rs != polyBytes) {
        return 1;
    }
    // H2D：把从 bin 文件读入的多项式系数拷贝到设备侧输入 buffer。
    CHECK_ACL(aclrtMemcpy(polyDevice, polyBytes, polyHost, polyBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    // 发起一次 kernel launch（blockDim=1 单核），并同步等待其执行完成。
    ACLRT_LAUNCH_KERNEL(compress_d_custom)(blockDim, stream, polyDevice, compDevice, F203_MLKEM_N);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // D2H：把设备侧输出结果拷回 host，再写入 output/comp.bin 供 verify_result.py 对拍。
    CHECK_ACL(aclrtMemcpy(compHost, polyBytes, compDevice, polyBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/comp.bin", compHost, polyBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(polyDevice));
    CHECK_ACL(aclrtFreeHost(polyHost));
    CHECK_ACL(aclrtFree(compDevice));
    CHECK_ACL(aclrtFreeHost(compHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
