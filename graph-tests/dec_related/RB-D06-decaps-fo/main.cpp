/**
 * @file main.cpp
 * @brief RB-D06-decaps-fo host：读 c/c'/K'/z → launch decaps_fo_custom → 写 K。
 *
 * 本文件在流水线中的位置：仅 Host 壳（H2D / launch / mid-sync / D2H）。
 * Host **不**预喂最终 K（输出缓冲清零后由设备 UB+DataCopy 写出）；
 * 合法/拒绝两路径由 env FO_PATH=legit|reject 选择 input 子目录（见 gen_data / run.sh）。
 *
 * z 输入文件契约：dk_kem[3136:3168) 切片语义（gen 从权威 KeyGen 的 dk 切出）。
 */
#include "data_utils.h"

#include <cstdlib>
#include <cstring>
#include <string>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_decaps_fo_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void decaps_fo_custom(GM_ADDR cGm, GM_ADDR cPrimeGm, GM_ADDR kPrimeGm, GM_ADDR zGm,
                                 GM_ADDR kOutGm);
#endif

namespace {
constexpr size_t kCt = 1568;
constexpr size_t kHalf = 32;
/** tikicpulib GmAlloc 最小粒度习惯值 */
constexpr size_t kMinAlloc = 2048;

/**
 * 解析 FO_PATH：legit|reject；缺省 legit。
 * @return 子目录名字符串
 */
const char *ResolveFoPath()
{
    const char *p = std::getenv("FO_PATH");
    if (p == nullptr || p[0] == '\0') {
        return "legit";
    }
    if (std::strcmp(p, "legit") == 0 || std::strcmp(p, "reject") == 0) {
        return p;
    }
    ERROR_LOG("FO_PATH must be legit|reject, got %s", p);
    return nullptr;
}
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;
    const char *foPath = ResolveFoPath();
    if (foPath == nullptr) {
        return 3;
    }
    // input/<legit|reject>/{c,c_prime,k_prime,z}.bin → output/k_<path>.bin
    const std::string inDir = std::string("./input/") + foPath + "/";
    const std::string outK = std::string("./output/k_") + foPath + ".bin";

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *cBuf = (uint8_t *)AscendC::GmAlloc(kCt > kMinAlloc ? kCt : kMinAlloc);
    uint8_t *cpBuf = (uint8_t *)AscendC::GmAlloc(kCt > kMinAlloc ? kCt : kMinAlloc);
    uint8_t *kpBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);
    uint8_t *zBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);
    uint8_t *kBuf = (uint8_t *)AscendC::GmAlloc(kHalf > kMinAlloc ? kHalf : kMinAlloc);

    size_t rs = 0;
    if (!ReadFile(inDir + "c.bin", rs, cBuf, kCt) || rs != kCt) {
        return 1;
    }
    if (!ReadFile(inDir + "c_prime.bin", rs, cpBuf, kCt) || rs != kCt) {
        return 1;
    }
    if (!ReadFile(inDir + "k_prime.bin", rs, kpBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile(inDir + "z.bin", rs, zBuf, kHalf) || rs != kHalf) {
        return 1;
    }
    // Host 不预喂 K：清零后交给设备写出。
    for (size_t i = 0; i < kHalf; ++i) {
        kBuf[i] = 0;
    }

    ICPU_RUN_KF(decaps_fo_custom, blockDim, cBuf, cpBuf, kpBuf, zBuf, kBuf);

    if (!WriteFile(outK, kBuf, kHalf)) {
        return 2;
    }

    AscendC::GmFree((void *)cBuf);
    AscendC::GmFree((void *)cpBuf);
    AscendC::GmFree((void *)kpBuf);
    AscendC::GmFree((void *)zBuf);
    AscendC::GmFree((void *)kBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *cHost = nullptr;
    uint8_t *cpHost = nullptr;
    uint8_t *kpHost = nullptr;
    uint8_t *zHost = nullptr;
    uint8_t *kHost = nullptr;
    uint8_t *cDev = nullptr;
    uint8_t *cpDev = nullptr;
    uint8_t *kpDev = nullptr;
    uint8_t *zDev = nullptr;
    uint8_t *kDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCt));
    CHECK_ACL(aclrtMallocHost((void **)(&cpHost), kCt));
    CHECK_ACL(aclrtMallocHost((void **)(&kpHost), kHalf));
    CHECK_ACL(aclrtMallocHost((void **)(&zHost), kHalf));
    CHECK_ACL(aclrtMallocHost((void **)(&kHost), kHalf));
    CHECK_ACL(aclrtMalloc((void **)&cDev, kCt, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cpDev, kCt, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&kpDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&zDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&kDev, kHalf, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile(inDir + "c.bin", rs, cHost, kCt) || rs != kCt) {
        return 1;
    }
    if (!ReadFile(inDir + "c_prime.bin", rs, cpHost, kCt) || rs != kCt) {
        return 1;
    }
    if (!ReadFile(inDir + "k_prime.bin", rs, kpHost, kHalf) || rs != kHalf) {
        return 1;
    }
    if (!ReadFile(inDir + "z.bin", rs, zHost, kHalf) || rs != kHalf) {
        return 1;
    }
    for (size_t i = 0; i < kHalf; ++i) {
        kHost[i] = 0;
    }

    CHECK_ACL(aclrtMemcpy(cDev, kCt, cHost, kCt, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(cpDev, kCt, cpHost, kCt, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(kpDev, kHalf, kpHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHalf, zHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(kDev, kHalf, kHost, kHalf, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(decaps_fo_custom)(blockDim, stream, cDev, cpDev, kpDev, zDev, kDev);
    // Host mid-sync：stream 同步后 D2H。
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(kHost, kHalf, kDev, kHalf, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile(outK, kHost, kHalf)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFree(cpDev));
    CHECK_ACL(aclrtFree(kpDev));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFree(kDev));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFreeHost(cpHost));
    CHECK_ACL(aclrtFreeHost(kpHost));
    CHECK_ACL(aclrtFreeHost(zHost));
    CHECK_ACL(aclrtFreeHost(kHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
