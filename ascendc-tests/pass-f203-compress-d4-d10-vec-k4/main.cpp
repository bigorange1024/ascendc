/**
 * Compress_d host：input/poly.bin → output/comp.bin
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

    constexpr size_t polyBytes = static_cast<size_t>(F203_MLKEM_N) * sizeof(int32_t);
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *poly_in = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);
    uint8_t *comp_out = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/poly.bin", rs, poly_in, polyBytes) || rs != polyBytes) {
        return 1;
    }

    ICPU_RUN_KF(compress_d_custom, blockDim, poly_in, comp_out, F203_MLKEM_N);

    if (!WriteFile("./output/comp.bin", comp_out, polyBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)poly_in);
    AscendC::GmFree((void *)comp_out);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *polyHost = nullptr;
    uint8_t *compHost = nullptr;
    uint8_t *polyDevice = nullptr;
    uint8_t *compDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&polyHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&polyDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&compHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&compDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/poly.bin", rs, polyHost, polyBytes) || rs != polyBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(polyDevice, polyBytes, polyHost, polyBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(compress_d_custom)(blockDim, stream, polyDevice, compDevice, F203_MLKEM_N);
    CHECK_ACL(aclrtSynchronizeStream(stream));

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
