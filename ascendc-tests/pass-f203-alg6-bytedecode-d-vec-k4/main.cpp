/**
 * ByteDecode_d host：input/encoded.bin → output/comp.bin
 */
#include "byte_decode_d_config.hpp"
#include "data_utils.h"
#include "f203_mlkem_params.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_byte_decode_d_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void byte_decode_d_custom(GM_ADDR encoded_in, GM_ADDR comp_out, int32_t coeff_n);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t inBytes = static_cast<size_t>(F203_BYTE_DECODE_POLY_BYTES);
    constexpr size_t compBytes = static_cast<size_t>(F203_MLKEM_N) * sizeof(int32_t);
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *encoded_in = (uint8_t *)AscendC::GmAlloc(inBytes > 1024 ? inBytes : 1024);
    uint8_t *comp_out = (uint8_t *)AscendC::GmAlloc(compBytes > 1024 ? compBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/encoded.bin", rs, encoded_in, inBytes) || rs != inBytes) {
        return 1;
    }

    ICPU_RUN_KF(byte_decode_d_custom, blockDim, encoded_in, comp_out, F203_MLKEM_N);

    if (!WriteFile("./output/comp.bin", comp_out, compBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)encoded_in);
    AscendC::GmFree((void *)comp_out);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *encodedHost = nullptr;
    uint8_t *compHost = nullptr;
    uint8_t *encodedDevice = nullptr;
    uint8_t *compDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&encodedHost), inBytes));
    CHECK_ACL(aclrtMalloc((void **)&encodedDevice, inBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&compHost), compBytes));
    CHECK_ACL(aclrtMalloc((void **)&compDevice, compBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/encoded.bin", rs, encodedHost, inBytes) || rs != inBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(encodedDevice, inBytes, encodedHost, inBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(byte_decode_d_custom)(blockDim, stream, encodedDevice, compDevice, F203_MLKEM_N);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(compHost, compBytes, compDevice, compBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/comp.bin", compHost, compBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(encodedDevice));
    CHECK_ACL(aclrtFreeHost(encodedHost));
    CHECK_ACL(aclrtFree(compDevice));
    CHECK_ACL(aclrtFreeHost(compHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
