/**
 * @file main.cpp
 * @brief sepolyvec8 NTT Host：se_polyvec + LUT → output.bin [8,256]。
 *
 * ## 流水线位置
 * exp-sepolyvec8-ntt-k8：8 poly 批 NTT 探针；读 input/se_polyvec_gm.bin、
 * mat_b_lut_gm.bin、tiling.bin；启动 MIX 核 sepolyvec8_ntt_custom；写 output/output.bin。
 *
 * ## 与 golden
 * scripts/verify_result.py 对拍；仅 I/O 等价，非设备实现规格。
 *
 * ## 分支
 * - ASCENDC_CPU_DEBUG：tikicpu 孪生
 * - 否则：ACL + aclrtLaunchKernel
 */
#include "data_utils.h"
#include "tiling.h"
#include <stdexcept>
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_sepolyvec8_ntt_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void sepolyvec8_ntt_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
#endif

/**
 * Host main：分配 GM/ACL，装入 LUT 到 ws+M0，启动 sepolyvec8_ntt_custom。
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kPolys * tiling::n * sizeof(int32_t);
    size_t dstFileSize = tiling::kPolys * tiling::n * sizeof(int32_t);
    size_t lutFileSize = tiling::n * tiling::n * 4;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    // CPU 孪生 MIX 模式
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize < sizeof(TilingData)) {
        return 1;
    }
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);

    ok = ReadFile("./input/se_polyvec_gm.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    // LUT 装入 workspace M0 起（四块 M0..M3 连续）
    ok = ReadFile("./input/mat_b_lut_gm.bin", lutFileSize, ws + tiling::M0, lutFileSize);
    if (!ok) {
        return 10;
    }
    ICPU_RUN_KF(sepolyvec8_ntt_custom, blockDim, dst, src, ws, *tiling);

    ok = WriteFile("./output/output.bin", dst, dstFileSize);
    if (!ok) {
        return 11;
    }
    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/se_polyvec_gm.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    // Host 侧同样把 LUT 放到 wsHost+M0，再 H2D 整块 ws
    ok = ReadFile("./input/mat_b_lut_gm.bin", lutFileSize, wsHost + tiling::M0, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(sepolyvec8_ntt_custom)(blockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/output.bin", dstHost, dstFileSize);
    if (!ok) {
        return 11;
    }

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
