/*
 * Host：launch tiling → 设备 UB 自检 → reserved2 / device_pass.bin（无 x/y GM 搬运）。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void shake128_general_do(uint32_t coreDim, void *l2ctrl, void *stream, uint8_t *tiling);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void shake128_general(GM_ADDR tiling);
#endif

struct CaseMeta {
    uint32_t batch;
    uint32_t maxMsgLen;
    uint32_t outLen;
};

static bool ReadMeta(CaseMeta *meta)
{
    size_t sz = 0;
    uint32_t buf[3] = {0, 0, 0};
    if (!ReadFile("./input/meta.bin", sz, buf, sizeof(buf)) || sz != sizeof(buf)) {
        return false;
    }
    meta->batch = buf[0];
    meta->maxMsgLen = buf[1];
    meta->outLen = buf[2];
    return meta->batch > 0 && meta->maxMsgLen > 0 && meta->outLen > 0;
}

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    CaseMeta meta{};
    if (!ReadMeta(&meta)) {
        std::cerr << "[FAIL] read input/meta.bin\n";
        return 1;
    }

    const size_t tilingBytes = sizeof(ShakeGeneralTilingData);

    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, meta.batch, meta.maxMsgLen, meta.outLen, SHAKE128_RATE_BYTES);
    tilingHost.blockDim = 1U;
    tilingHost.reserved2 = 0U;

    std::cout << "[main] SHAKE128 batch=" << meta.batch << " maxMsgLen=" << meta.maxMsgLen
              << " outLen=" << meta.outLen << " blockDim=" << tilingHost.blockDim << " rate=" << tilingHost.rate
              << " (UB self-check, no GM x/y)\n";

#ifdef __CCE_KT_TEST__
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(tilingBytes));
    std::memcpy(tiling, &tilingHost, tilingBytes);

    ICPU_RUN_KF(shake128_general, tilingHost.blockDim, tiling);

    std::memcpy(&tilingHost, tiling, tilingBytes);
    const uint32_t pass = tilingHost.reserved2;
    if (!WriteFile("./output/device_pass.bin", &pass, sizeof(pass))) {
        return 4;
    }
    AscendC::GmFree(tiling);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *tilingHostBuf = nullptr;
    uint8_t *tilingDev = nullptr;

    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), tilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), tilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    std::memcpy(tilingHostBuf, &tilingHost, tilingBytes);
    CHECK_ACL(aclrtMemcpy(tilingDev, tilingBytes, tilingHostBuf, tilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    shake128_general_do(tilingHost.blockDim, nullptr, stream, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(tilingHostBuf, tilingBytes, tilingDev, tilingBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    std::memcpy(&tilingHost, tilingHostBuf, tilingBytes);

    const uint32_t pass = tilingHost.reserved2;
    if (!WriteFile("./output/device_pass.bin", &pass, sizeof(pass))) {
        return 4;
    }

    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif

    if (tilingHost.reserved2 != 1U) {
        std::cerr << "[FAIL] device UB self-check reserved2=" << tilingHost.reserved2 << "\n";
        return 5;
    }
    return 0;
}
