/**
 * ByteEncode₁₂-only host：dst_preset + t_hat_preset → ek/sk。
 */
#include "data_utils.h"
#include "tiling.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_byte_encode12_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void byte_encode12_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out, TilingData tiling);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t dstFileSize = tiling::dstFileBytes;
    size_t tHatFileSize = tiling::tHatFileBytes;
    size_t ekFileSize = byte_encode::polyVecBytes;
    size_t skFileSize = byte_encode::polyVecBytes;
    uint32_t blockDim = 1;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize < sizeof(TilingData)) {
        return 1;
    }
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024);
    uint8_t *t_hat = (uint8_t *)AscendC::GmAlloc(tHatFileSize > 1024 ? tHatFileSize : 1024);
    uint8_t *ek_out = (uint8_t *)AscendC::GmAlloc(ekFileSize > 1024 ? ekFileSize : 1024);
    uint8_t *sk_out = (uint8_t *)AscendC::GmAlloc(skFileSize > 1024 ? skFileSize : 1024);

    ok = ReadFile("./input/dst.bin", dstFileSize, dst, dstFileSize);
    if (!ok) {
        return 17;
    }
    ok = ReadFile("./input/t_hat.bin", tHatFileSize, t_hat, tHatFileSize);
    if (!ok) {
        return 18;
    }

    ICPU_RUN_KF(byte_encode12_custom, blockDim, dst, t_hat, ek_out, sk_out, *tiling);

    ok = WriteFile("./output/ek_polyvec.bin", ek_out, ekFileSize);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/sk_polyvec.bin", sk_out, skFileSize);
    if (!ok) {
        return 21;
    }

    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)t_hat);
    AscendC::GmFree((void *)ek_out);
    AscendC::GmFree((void *)sk_out);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *tHatHost, *ekHost, *skHost;
    uint8_t *dstDevice, *tHatDevice, *ekDevice, *skDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&tHatHost), tHatFileSize));
    CHECK_ACL(aclrtMalloc((void **)&tHatDevice, tHatFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&ekHost), ekFileSize));
    CHECK_ACL(aclrtMalloc((void **)&ekDevice, ekFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&skHost), skFileSize));
    CHECK_ACL(aclrtMalloc((void **)&skDevice, skFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    ok = ReadFile("./input/dst.bin", dstFileSize, dstHost, dstFileSize);
    if (!ok) {
        return 17;
    }
    ok = ReadFile("./input/t_hat.bin", tHatFileSize, tHatHost, tHatFileSize);
    if (!ok) {
        return 18;
    }
    CHECK_ACL(aclrtMemcpy(dstDevice, dstFileSize, dstHost, dstFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tHatDevice, tHatFileSize, tHatHost, tHatFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(byte_encode12_custom)(blockDim, stream, dstDevice, tHatDevice, ekDevice, skDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(ekHost, ekFileSize, ekDevice, ekFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(skHost, skFileSize, skDevice, skFileSize, ACL_MEMCPY_DEVICE_TO_HOST));

    ok = WriteFile("./output/ek_polyvec.bin", ekHost, ekFileSize);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/sk_polyvec.bin", skHost, skFileSize);
    if (!ok) {
        return 21;
    }

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(tHatDevice));
    CHECK_ACL(aclrtFreeHost(tHatHost));
    CHECK_ACL(aclrtFree(ekDevice));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(skDevice));
    CHECK_ACL(aclrtFreeHost(skHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
