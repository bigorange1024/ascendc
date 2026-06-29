/**
 * Tag5T poly-batch Stage1+2（平面 mat_c）：src [8,256] → mat_c_planar [64,128]。
 */
#include "data_utils.h"
#include "tiling.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR mat_c_planar, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern volatile int g_planar_mix_pass;
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kPolys * tiling::n * sizeof(int32_t);
    size_t matCPlanarFileSize = tiling::matCPlanarRows * tiling::halfN * sizeof(int32_t);
    size_t lutEvenFileSize = tiling::lutEvenOddFileBytes;
    size_t lutOddFileSize = tiling::lutEvenOddFileBytes;
    size_t s0FileSize = tiling::mRows * tiling::n;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize < sizeof(TilingData)) {
        return 1;
    }
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *mat_c = (uint8_t *)AscendC::GmAlloc(matCPlanarFileSize > 1024 ? matCPlanarFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/lut_even_stacked.bin", lutEvenFileSize, ws + tiling::LUT_EVEN_STACKED, lutEvenFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut_odd_stacked.bin", lutOddFileSize, ws + tiling::LUT_ODD_STACKED, lutOddFileSize);
    if (!ok) {
        return 11;
    }
    if (tiling->mixPass == 2) {
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, ws + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    g_planar_mix_pass = tiling->mixPass;
    ICPU_RUN_KF(mmad_custom, blockDim, mat_c, src, ws, *tiling);

    ok = WriteFile("./output/mat_c_planar.bin", mat_c, matCPlanarFileSize);
    if (!ok) {
        return 14;
    }
    ok = WriteFile("./output/s0.bin", ws + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }
    AscendC::GmFree((void *)mat_c);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *matCHost, *srcHost, *wsHost;
    uint8_t *matCDevice, *srcDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&matCHost), matCPlanarFileSize));
    CHECK_ACL(aclrtMalloc((void **)&matCDevice, matCPlanarFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/lut_even_stacked.bin", lutEvenFileSize, wsHost + tiling::LUT_EVEN_STACKED, lutEvenFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut_odd_stacked.bin", lutOddFileSize, wsHost + tiling::LUT_ODD_STACKED, lutOddFileSize);
    if (!ok) {
        return 11;
    }
    if (tiling->mixPass == 2) {
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, wsHost + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, matCDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(matCHost, matCPlanarFileSize, matCDevice, matCPlanarFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/mat_c_planar.bin", matCHost, matCPlanarFileSize);
    if (!ok) {
        return 14;
    }
    CHECK_ACL(aclrtMemcpy(wsHost, wsFileSize, wsDevice, wsFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/s0.bin", wsHost + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }

    CHECK_ACL(aclrtFree(matCDevice));
    CHECK_ACL(aclrtFreeHost(matCHost));
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
