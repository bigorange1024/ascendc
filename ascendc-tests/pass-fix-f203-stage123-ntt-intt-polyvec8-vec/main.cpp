/**
 * @file main.cpp
 * @brief 8-poly polyvec 三段式 NTT/INTT host 驱动。
 */
#include "data_utils.h"
#include "tiling.h"

#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern volatile int g_stage123_k8_mix_pass;
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::srcFileBytes;
    size_t dstFileSize = tiling::dstFileBytes;
    size_t lutFileSize = tiling::lutEvenOddFileBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    size_t matCFileSize = tiling::matCFileBytes;
    size_t s0FileSize = tiling::s0FileBytes;
    TilingData tilingHost{};
    tilingHost.tileLength = static_cast<int32_t>(tiling::n);
    tilingHost.kPolys = static_cast<int32_t>(tiling::kK);
    tilingHost.mixPass = 3;

    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t tilingBuf[64] = {};
    size_t tilingRead = tilingSize;
    if (!ReadFile("./input/tiling.bin", tilingRead, tilingBuf, sizeof(tilingBuf)) ||
        tilingRead < sizeof(TilingData)) {
        return 1;
    }
    memcpy(&tilingHost, tilingBuf, sizeof(TilingData));
    g_stage123_k8_mix_pass = tilingHost.mixPass;

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    for (size_t i = 0; i < wsFileSize; ++i) {
        ws[i] = 0;
    }

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/lut_even_stacked.bin", lutFileSize, ws + tiling::LUT_EVEN_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut_odd_stacked.bin", lutFileSize, ws + tiling::LUT_ODD_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    if (tilingHost.mixPass == 1) {
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, ws + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    if (tilingHost.mixPass == 2) {
        ok = ReadFile("./input/mat_c_preset.bin", matCFileSize, ws + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 14;
        }
    }

    ICPU_RUN_KF(mmad_custom, blockDim, dst, src, ws, tilingHost);

    ok = WriteFile("./output/dst.bin", dst, dstFileSize);
    if (!ok) {
        return 2;
    }
    if (tilingHost.mixPass == 0 || tilingHost.mixPass == 3) {
        ok = WriteFile("./output/s0.bin", ws + tiling::S0, s0FileSize);
        if (!ok) {
            return 3;
        }
    }
    if (tilingHost.mixPass == 1 || tilingHost.mixPass == 3) {
        ok = WriteFile("./output/mat_c.bin", ws + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 4;
        }
    }

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    TilingData *tiling = nullptr;
    uint8_t *dstHost = nullptr;
    uint8_t *srcHost = nullptr;
    uint8_t *wsHost = nullptr;
    uint8_t *dstDevice = nullptr;
    uint8_t *srcDevice = nullptr;
    uint8_t *wsDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    size_t tilingRead = tilingSize;
    if (!ReadFile("./input/tiling.bin", tilingRead, tiling, tilingSize) || tilingRead < sizeof(TilingData)) {
        return 1;
    }

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    memset(wsHost, 0, wsFileSize);

    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/lut_even_stacked.bin", lutFileSize, wsHost, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(wsDevice + tiling::LUT_EVEN_STACKED, lutFileSize, wsHost, lutFileSize,
                           ACL_MEMCPY_HOST_TO_DEVICE));
    ok = ReadFile("./input/lut_odd_stacked.bin", lutFileSize, wsHost, lutFileSize);
    if (!ok) {
        return 10;
    }
    CHECK_ACL(aclrtMemcpy(wsDevice + tiling::LUT_ODD_STACKED, lutFileSize, wsHost, lutFileSize,
                         ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/dst.bin", dstHost, dstFileSize)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFreeHost(tiling));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
