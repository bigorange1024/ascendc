/**
 * @file main.cpp
 * @brief RB-K05-kem-tail host：读 ek/dk_pke/seed_d → launch kg_kem_tail_custom → 写 h/z/dk_kem。
 *
 * 本文件在流水线中的位置：仅 Host 壳（H2D / launch / mid-sync / D2H）。
 * Host **不**预喂最终 H(ek)/z/dk_kem（输出缓冲清零后由设备写出）；golden 仅 verify 对拍。
 *
 * 上游：优先 RB-K04 产物（gen_data 拷贝）；本刀不改动 P01–P04 语义。
 */
#include "data_utils.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_kg_kem_tail_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void kg_kem_tail_custom(GM_ADDR ekGm, GM_ADDR dkPkeGm, GM_ADDR seedDGm, GM_ADDR hGm,
                                   GM_ADDR zGm, GM_ADDR dkKemGm);
#endif

namespace {
constexpr size_t kEkBytes = 1568;
constexpr size_t kDkPkeBytes = 1536;
constexpr size_t kHashBytes = 32;
constexpr size_t kDkKemBytes = 3168;
constexpr size_t kSeedPad = 32;
#ifdef ASCENDC_CPU_DEBUG
/** tikicpulib GmAlloc 最小粒度（仅 CPU 孪生路径使用；NPU/SIM 走 aclrtMalloc） */
constexpr size_t kMinAlloc = 1024;
#endif
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *ekBuf = (uint8_t *)AscendC::GmAlloc(kEkBytes > kMinAlloc ? kEkBytes : kMinAlloc);
    uint8_t *dkPkeBuf =
        (uint8_t *)AscendC::GmAlloc(kDkPkeBytes > kMinAlloc ? kDkPkeBytes : kMinAlloc);
    uint8_t *seedBuf = (uint8_t *)AscendC::GmAlloc(kSeedPad > kMinAlloc ? kSeedPad : kMinAlloc);
    uint8_t *hBuf = (uint8_t *)AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc);
    uint8_t *zBuf = (uint8_t *)AscendC::GmAlloc(kHashBytes > kMinAlloc ? kHashBytes : kMinAlloc);
    uint8_t *dkKemBuf =
        (uint8_t *)AscendC::GmAlloc(kDkKemBytes > kMinAlloc ? kDkKemBytes : kMinAlloc);

    size_t rs = 0;
    if (!ReadFile("./input/ek_pke.bin", rs, ekBuf, kEkBytes) || rs != kEkBytes) {
        return 1;
    }
    if (!ReadFile("./input/dk_pke.bin", rs, dkPkeBuf, kDkPkeBytes) || rs != kDkPkeBytes) {
        return 1;
    }
    if (!ReadFile("./input/seed_d.bin", rs, seedBuf, kSeedPad) || rs != kSeedPad) {
        return 1;
    }
    // Host 不预喂输出：清零后交给设备 UB+DataCopy 写出。
    for (size_t i = 0; i < kHashBytes; ++i) {
        hBuf[i] = 0;
        zBuf[i] = 0;
    }
    for (size_t i = 0; i < kDkKemBytes; ++i) {
        dkKemBuf[i] = 0;
    }

    ICPU_RUN_KF(kg_kem_tail_custom, blockDim, ekBuf, dkPkeBuf, seedBuf, hBuf, zBuf, dkKemBuf);

    if (!WriteFile("./output/h.bin", hBuf, kHashBytes)) {
        return 2;
    }
    if (!WriteFile("./output/z.bin", zBuf, kHashBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_kem.bin", dkKemBuf, kDkKemBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)ekBuf);
    AscendC::GmFree((void *)dkPkeBuf);
    AscendC::GmFree((void *)seedBuf);
    AscendC::GmFree((void *)hBuf);
    AscendC::GmFree((void *)zBuf);
    AscendC::GmFree((void *)dkKemBuf);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekHost = nullptr;
    uint8_t *dkPkeHost = nullptr;
    uint8_t *seedHost = nullptr;
    uint8_t *hHost = nullptr;
    uint8_t *zHost = nullptr;
    uint8_t *dkKemHost = nullptr;
    uint8_t *ekDev = nullptr;
    uint8_t *dkPkeDev = nullptr;
    uint8_t *seedDev = nullptr;
    uint8_t *hDev = nullptr;
    uint8_t *zDev = nullptr;
    uint8_t *dkKemDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&dkPkeHost), kDkPkeBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&seedHost), kSeedPad));
    CHECK_ACL(aclrtMallocHost((void **)(&hHost), kHashBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&zHost), kHashBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&dkKemHost), kDkKemBytes));
    CHECK_ACL(aclrtMalloc((void **)&ekDev, kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dkPkeDev, kDkPkeBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&seedDev, kSeedPad, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&hDev, kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&zDev, kHashBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&dkKemDev, kDkKemBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/ek_pke.bin", rs, ekHost, kEkBytes) || rs != kEkBytes) {
        return 1;
    }
    if (!ReadFile("./input/dk_pke.bin", rs, dkPkeHost, kDkPkeBytes) || rs != kDkPkeBytes) {
        return 1;
    }
    if (!ReadFile("./input/seed_d.bin", rs, seedHost, kSeedPad) || rs != kSeedPad) {
        return 1;
    }
    for (size_t i = 0; i < kHashBytes; ++i) {
        hHost[i] = 0;
        zHost[i] = 0;
    }
    for (size_t i = 0; i < kDkKemBytes; ++i) {
        dkKemHost[i] = 0;
    }

    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkPkeDev, kDkPkeBytes, dkPkeHost, kDkPkeBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(seedDev, kSeedPad, seedHost, kSeedPad, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(hDev, kHashBytes, hHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(zDev, kHashBytes, zHost, kHashBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dkKemDev, kDkKemBytes, dkKemHost, kDkKemBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(kg_kem_tail_custom)
    (blockDim, stream, ekDev, dkPkeDev, seedDev, hDev, zDev, dkKemDev);
    // Host mid-sync：stream 同步后 D2H，供 verify 可见。
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(hHost, kHashBytes, hDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(zHost, kHashBytes, zDev, kHashBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(dkKemHost, kDkKemBytes, dkKemDev, kDkKemBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/h.bin", hHost, kHashBytes)) {
        return 2;
    }
    if (!WriteFile("./output/z.bin", zHost, kHashBytes)) {
        return 2;
    }
    if (!WriteFile("./output/dk_kem.bin", dkKemHost, kDkKemBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(dkPkeDev));
    CHECK_ACL(aclrtFree(seedDev));
    CHECK_ACL(aclrtFree(hDev));
    CHECK_ACL(aclrtFree(zDev));
    CHECK_ACL(aclrtFree(dkKemDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFreeHost(dkPkeHost));
    CHECK_ACL(aclrtFreeHost(seedHost));
    CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFreeHost(zHost));
    CHECK_ACL(aclrtFreeHost(dkKemHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
