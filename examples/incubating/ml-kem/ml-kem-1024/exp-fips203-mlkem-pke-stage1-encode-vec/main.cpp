/**
 * @file main.cpp
 * F203 Stage1 Host 入口：读 se_polyvec_gm.bin → 调 f203_stage1_encode → 写 mat_a_gm.bin。
 *
 * 张量布局：
 *   输入 se  [8,256]  int32，8192 字节
 *   输出 mat_a [16,256] int8，4096 字节（行 0..7 HI，8..15 LO）
 *
 * Launch：环境变量 LAUNCH_PROFILE=aiv=1|8（run.sh --aiv 设置），见 launch_profile.h。
 * CPU 仿真：ASCENDC_CPU_DEBUG + ICPU_RUN_KF；真机：ACL H2D/D2H + f203_stage1_encode_do。
 */
#include "data_utils.h"
#include "launch_profile.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
extern void f203_stage1_encode_do(uint32_t blockDim, void *stream, uint8_t *se, uint8_t *matA);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_stage1_encode_custom(GM_ADDR se, GM_ADDR matA);
#endif

namespace {
constexpr size_t kKPolys = 8;
constexpr size_t kN = 256;
constexpr size_t kRowsA = 16; // HI(8)+LO(8)
constexpr size_t kSeBytes = kKPolys * kN * sizeof(int32_t);
constexpr size_t kMatABytes = kRowsA * kN * sizeof(int8_t);
} // namespace

/**
 * Host main：读 se → 调 Stage1 encode → 写 mat_a；CPU/ACL 双路径。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // blockDim：1 → 单核串行 8 poly；8 → 8 核各 1 poly
    const launch_profile::Config launchCfg = launch_profile::Get(launch_profile::FromEnv());
    const uint32_t blockDim = launchCfg.blockDim;

#ifdef ASCENDC_CPU_DEBUG
    // CPU 仿真：GmAlloc 模拟 GM，无需 ACL
    uint8_t *se = static_cast<uint8_t *>(AscendC::GmAlloc(kSeBytes));
    uint8_t *matA = static_cast<uint8_t *>(AscendC::GmAlloc(kMatABytes));

    size_t seFileSize = kSeBytes;
    ReadFile("./input/se_polyvec_gm.bin", seFileSize, se, kSeBytes);
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    ICPU_RUN_KF(f203_stage1_encode_custom, blockDim, se, matA);
    WriteFile("./output/mat_a_gm.bin", matA, kMatABytes);

    AscendC::GmFree(se);
    AscendC::GmFree(matA);
#else
    // 真机：ACL 初始化 → Host 读 bin → H2D → kernel → D2H → 写 output
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *seHost;
    uint8_t *seDevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&seHost), kSeBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&seDevice), kSeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t seFileSize = kSeBytes;
    ReadFile("./input/se_polyvec_gm.bin", seFileSize, seHost, kSeBytes);
    CHECK_ACL(aclrtMemcpy(seDevice, kSeBytes, seHost, kSeBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    uint8_t *matAHost;
    uint8_t *matADevice;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&matAHost), kMatABytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&matADevice), kMatABytes, ACL_MEM_MALLOC_HUGE_FIRST));

    f203_stage1_encode_do(blockDim, stream, seDevice, matADevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(matAHost, kMatABytes, matADevice, kMatABytes, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/mat_a_gm.bin", matAHost, kMatABytes);

    CHECK_ACL(aclrtFree(seDevice));
    CHECK_ACL(aclrtFreeHost(seHost));
    CHECK_ACL(aclrtFree(matADevice));
    CHECK_ACL(aclrtFreeHost(matAHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
