#include "data_utils.h"
#include "tiling.h"

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern void multiply_ntts_custom_do(uint32_t coreDim, void *l2ctrl, void *stream, uint8_t *f, uint8_t *g,
                                    uint8_t *h);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void multiply_ntts_custom(GM_ADDR f, GM_ADDR g, GM_ADDR h);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const size_t polyBytes = static_cast<size_t>(alg11_tiling::kN) * sizeof(int32_t);
    const uint32_t blockDim = static_cast<uint32_t>(alg11_tiling::kBlockDim);

#ifdef __CCE_KT_TEST__
    uint8_t *f = (uint8_t *)AscendC::GmAlloc(polyBytes);
    uint8_t *g = (uint8_t *)AscendC::GmAlloc(polyBytes);
    uint8_t *h = (uint8_t *)AscendC::GmAlloc(polyBytes);

    size_t fSize = 0;
    size_t gSize = 0;
    ReadFile("./input/a.bin", fSize, f, polyBytes);
    ReadFile("./input/b.bin", gSize, g, polyBytes);

    ICPU_RUN_KF(multiply_ntts_custom, blockDim, f, g, h);

    WriteFile("./output/h.bin", h, polyBytes);

    AscendC::GmFree((void *)f);
    AscendC::GmFree((void *)g);
    AscendC::GmFree((void *)h);
#else
    CHECK_ACL(aclInit(nullptr));
    aclrtContext context;
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateContext(&context, deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *fHost = nullptr;
    uint8_t *gHost = nullptr;
    uint8_t *hHost = nullptr;
    uint8_t *fDevice = nullptr;
    uint8_t *gDevice = nullptr;
    uint8_t *hDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&fHost), polyBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&gHost), polyBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&hHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&fDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&gDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&hDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t fSize = 0;
    size_t gSize = 0;
    ReadFile("./input/a.bin", fSize, fHost, polyBytes);
    ReadFile("./input/b.bin", gSize, gHost, polyBytes);
    CHECK_ACL(aclrtMemcpy(fDevice, polyBytes, fHost, polyBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(gDevice, polyBytes, gHost, polyBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    multiply_ntts_custom_do(blockDim, nullptr, stream, fDevice, gDevice, hDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(hHost, polyBytes, hDevice, polyBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    WriteFile("./output/h.bin", hHost, polyBytes);

    CHECK_ACL(aclrtFree(fDevice));
    CHECK_ACL(aclrtFree(gDevice));
    CHECK_ACL(aclrtFree(hDevice));
    CHECK_ACL(aclrtFreeHost(fHost));
    CHECK_ACL(aclrtFreeHost(gHost));
    CHECK_ACL(aclrtFreeHost(hHost));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtDestroyContext(context));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
