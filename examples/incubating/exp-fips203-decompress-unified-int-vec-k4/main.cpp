/**
 * @file main.cpp
 * @brief 统一整数 Decompress_d exp 探针 host：input/comp.bin → kernel → output/poly.bin。
 *
 * 流水线：gen_data.py 写 input + golden → 本 main launch AIV kernel → verify_result.py 对拍。
 * 规格：exp-fips203-decompress-unified-int-vec-k4-实现方案-customspec.pdf
 */
#include "data_utils.h"
#include "f203_mlkem_params.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_decompress_unified_int_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void decompress_unified_int_custom(GM_ADDR comp_in, GM_ADDR poly_out, int32_t coeff_n);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t polyBytes = static_cast<size_t>(F203_MLKEM_N) * sizeof(int32_t);
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *comp_in = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);
    uint8_t *poly_out = (uint8_t *)AscendC::GmAlloc(polyBytes > 1024 ? polyBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/comp.bin", rs, comp_in, polyBytes) || rs != polyBytes) {
        return 1;
    }

    ICPU_RUN_KF(decompress_unified_int_custom, blockDim, comp_in, poly_out, F203_MLKEM_N);

    if (!WriteFile("./output/poly.bin", poly_out, polyBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)comp_in);
    AscendC::GmFree((void *)poly_out);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *compHost = nullptr;
    uint8_t *polyHost = nullptr;
    uint8_t *compDevice = nullptr;
    uint8_t *polyDevice = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&compHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&compDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&polyHost), polyBytes));
    CHECK_ACL(aclrtMalloc((void **)&polyDevice, polyBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/comp.bin", rs, compHost, polyBytes) || rs != polyBytes) {
        return 1;
    }
    CHECK_ACL(aclrtMemcpy(compDevice, polyBytes, compHost, polyBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(decompress_unified_int_custom)(blockDim, stream, compDevice, polyDevice, F203_MLKEM_N);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(polyHost, polyBytes, polyDevice, polyBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/poly.bin", polyHost, polyBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(compDevice));
    CHECK_ACL(aclrtFreeHost(compHost));
    CHECK_ACL(aclrtFree(polyDevice));
    CHECK_ACL(aclrtFreeHost(polyHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
