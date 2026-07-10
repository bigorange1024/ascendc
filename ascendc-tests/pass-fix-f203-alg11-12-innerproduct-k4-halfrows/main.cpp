/**
 * @file main.cpp
 * @brief Host 入口：Alg.13 行 18 半行内积探针（双 AIV，每核写 P_OUT/2 行 t̂，无 ê）。
 *
 * 流水线位置：读 input/a_hat.bin、s_hat.bin → 调 halfrows kernel → 写 output/t_hat.bin，
 * 由 scripts/verify_result.py 与 golden_t_hat.bin 对拍。
 *
 * GM 仍为全量 4×4 a_hat / s_hat；blockDim=2，与单核内积探针 I/O 兼容。
 * __CCE_KT_TEST__：CPU 孪生；否则 ACL 设备路径。
 */
#include "data_utils.h"
#include "innerproduct_tiling.h"

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern void hat_innerproduct_halfrows_custom_do(uint32_t coreDim, void *l2ctrl, void *stream, uint8_t *aHat,
                                                uint8_t *sHat, uint8_t *tHat);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void hat_innerproduct_halfrows_custom(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat);
#endif

/**
 * Host main：按 tiling 分配缓冲，加载 a_hat/s_hat，以 blockDim=2 启动半行内积，写出 t_hat。
 * @param argc 未使用
 * @param argv 未使用
 * @return 恒为 0
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // 全量 GM 字节数；blockDim=2（每 AIV 负责 kPPerAiv 行）
    const size_t aHatBytes = static_cast<size_t>(innerproduct_tiling::kAHatBytes);
    const size_t sHatBytes = static_cast<size_t>(innerproduct_tiling::kSHatBytes);
    const size_t tHatBytes = static_cast<size_t>(innerproduct_tiling::kTHatBytes);
    const uint32_t blockDim = static_cast<uint32_t>(innerproduct_tiling::kBlockDim);

#ifdef __CCE_KT_TEST__
    // ---------- CPU 孪生路径 ----------
    uint8_t *aHat = (uint8_t *)AscendC::GmAlloc(aHatBytes);
    uint8_t *sHat = (uint8_t *)AscendC::GmAlloc(sHatBytes);
    uint8_t *tHat = (uint8_t *)AscendC::GmAlloc(tHatBytes);

    size_t sz = 0;
    ReadFile("./input/a_hat.bin", sz, aHat, aHatBytes);
    ReadFile("./input/s_hat.bin", sz, sHat, sHatBytes);

    ICPU_RUN_KF(hat_innerproduct_halfrows_custom, blockDim, aHat, sHat, tHat);

    WriteFile("./output/t_hat.bin", tHat, tHatBytes);

    AscendC::GmFree((void *)aHat);
    AscendC::GmFree((void *)sHat);
    AscendC::GmFree((void *)tHat);
#else
    // ---------- ACL 设备路径 ----------
    CHECK_ACL(aclInit(nullptr));
    aclrtContext context;
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateContext(&context, deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *aHost = nullptr;
    uint8_t *sHost = nullptr;
    uint8_t *tHost = nullptr;
    uint8_t *aDev = nullptr;
    uint8_t *sDev = nullptr;
    uint8_t *tDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&aHost), aHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&sHost), sHatBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&tHost), tHatBytes));
    CHECK_ACL(aclrtMalloc((void **)&aDev, aHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&sDev, sHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&tDev, tHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t sz = 0;
    ReadFile("./input/a_hat.bin", sz, aHost, aHatBytes);
    ReadFile("./input/s_hat.bin", sz, sHost, sHatBytes);
    CHECK_ACL(aclrtMemcpy(aDev, aHatBytes, aHost, aHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(sDev, sHatBytes, sHost, sHatBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    hat_innerproduct_halfrows_custom_do(blockDim, nullptr, stream, aDev, sDev, tDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(tHost, tHatBytes, tDev, tHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/t_hat.bin", tHost, tHatBytes);

    CHECK_ACL(aclrtFree(aDev));
    CHECK_ACL(aclrtFree(sDev));
    CHECK_ACL(aclrtFree(tDev));
    CHECK_ACL(aclrtFreeHost(aHost));
    CHECK_ACL(aclrtFreeHost(sHost));
    CHECK_ACL(aclrtFreeHost(tHost));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtDestroyContext(context));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
