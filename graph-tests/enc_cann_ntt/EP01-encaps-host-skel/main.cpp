/**
 * EP01-encaps-host-skel · Encaps Host 壳（Alg.16/20 外形）
 *
 * Host：读 ek；读 m；算 H(ek)、G(m‖H(ek))→(K̄,r)；
 * 桩 Encrypt：写出固定长度假 c（由 r 派生填充，非真密文）；K=K̄。
 * 设备：仅 launch 一条 AIV 轻桩（enc_pack_stub）证明 run.sh cpu/SIM 壳可走；
 * 禁真 Encrypt 核、禁 -r npu、禁抄 examples/frozen/ER。
 */
#include "data_utils.h"
#include "host_hg_sha3.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_enc_pack_stub.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void enc_pack_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem, int32_t shift_bits);
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    constexpr size_t kEkBytes = 1568;
    constexpr size_t kMBytes = 32;
    constexpr size_t kCBytes = 1568;
    constexpr size_t kKBytes = 32;
    // 轻桩：对 256 个 int32 做 shift=0 恒等（仅证明 AIV launch 可走）
    constexpr int32_t kStubElems = 256;
    constexpr size_t kStubBytes = static_cast<size_t>(kStubElems) * sizeof(int32_t);
    uint32_t stubBlockDim = 1;
    bool ok;

    std::printf("[EP01] Encaps Host skel begin (H/G Host + stub Encrypt)\n");

#ifdef ASCENDC_CPU_DEBUG
    uint8_t *ek = (uint8_t *)AscendC::GmAlloc(kEkBytes);
    uint8_t *m = (uint8_t *)AscendC::GmAlloc(kMBytes);
    uint8_t *h = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *kBar = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *r = (uint8_t *)AscendC::GmAlloc(kKBytes);
    uint8_t *c = (uint8_t *)AscendC::GmAlloc(kCBytes);
    uint8_t *stubSrc = (uint8_t *)AscendC::GmAlloc(kStubBytes);
    uint8_t *stubDst = (uint8_t *)AscendC::GmAlloc(kStubBytes);

    size_t ekRead = kEkBytes;
    ok = ReadFile("./input/ek_kem.bin", ekRead, ek, kEkBytes);
    if (!ok) return 10;
    size_t mRead = kMBytes;
    ok = ReadFile("./input/m.bin", mRead, m, kMBytes);
    if (!ok) return 11;

    // -------- Host：H(ek)、G(m‖H) --------
    host_hg::H_Sha3_256(h, ek, kEkBytes);
    host_hg::G_Sha3_512(kBar, r, m, h);
    ok = WriteFile("./output/h_ek.bin", h, kKBytes);
    if (!ok) return 12;
    ok = WriteFile("./output/r.bin", r, kKBytes);
    if (!ok) return 13;
    ok = WriteFile("./output/K.bin", kBar, kKBytes);
    if (!ok) return 14;
    std::printf("[EP01] CPU Host H/G done (K=32B r=32B)\n");

    // -------- 桩 Encrypt：c[i] = r[i%32] ^ (i & 0xff)（长度正确即可）--------
    for (size_t i = 0; i < kCBytes; ++i) {
        c[i] = static_cast<uint8_t>(r[i % 32] ^ static_cast<uint8_t>(i & 0xff));
    }
    ok = WriteFile("./output/c.bin", c, kCBytes);
    if (!ok) return 15;
    std::printf("[EP01] CPU stub Encrypt done (c=%zu B)\n", kCBytes);

    // -------- 设备轻桩（恒等 ShiftRight 0）--------
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    std::memset(stubSrc, 0x11, kStubBytes);
    ICPU_RUN_KF(enc_pack_stub, stubBlockDim, stubDst, stubSrc, kStubElems, 0);
    ok = WriteFile("./output/dst_stub.bin", stubDst, kStubBytes);
    if (!ok) return 16;
    std::printf("[EP01] CPU AIV stub done\n");

    AscendC::GmFree((void *)ek);
    AscendC::GmFree((void *)m);
    AscendC::GmFree((void *)h);
    AscendC::GmFree((void *)kBar);
    AscendC::GmFree((void *)r);
    AscendC::GmFree((void *)c);
    AscendC::GmFree((void *)stubSrc);
    AscendC::GmFree((void *)stubDst);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekHost, *mHost, *hHost, *kBarHost, *rHost, *cHost, *stubSrcHost, *stubDstHost;
    uint8_t *stubSrcDev, *stubDstDev;
    CHECK_ACL(aclrtMallocHost((void **)(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&mHost), kMBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&hHost), kKBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&kBarHost), kKBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&rHost), kKBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&cHost), kCBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&stubSrcHost), kStubBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&stubDstHost), kStubBytes));
    CHECK_ACL(aclrtMalloc((void **)&stubSrcDev, kStubBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&stubDstDev, kStubBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t ekRead = kEkBytes;
    ok = ReadFile("./input/ek_kem.bin", ekRead, ekHost, kEkBytes);
    if (!ok) return 10;
    size_t mRead = kMBytes;
    ok = ReadFile("./input/m.bin", mRead, mHost, kMBytes);
    if (!ok) return 11;

    host_hg::H_Sha3_256(hHost, ekHost, kEkBytes);
    host_hg::G_Sha3_512(kBarHost, rHost, mHost, hHost);
    ok = WriteFile("./output/h_ek.bin", hHost, kKBytes);
    if (!ok) return 12;
    ok = WriteFile("./output/r.bin", rHost, kKBytes);
    if (!ok) return 13;
    ok = WriteFile("./output/K.bin", kBarHost, kKBytes);
    if (!ok) return 14;
    std::printf("[EP01] SIM Host H/G done\n");

    for (size_t i = 0; i < kCBytes; ++i) {
        cHost[i] = static_cast<uint8_t>(rHost[i % 32] ^ static_cast<uint8_t>(i & 0xff));
    }
    ok = WriteFile("./output/c.bin", cHost, kCBytes);
    if (!ok) return 15;
    std::printf("[EP01] SIM stub Encrypt done (c=%zu B)\n", kCBytes);

    std::memset(stubSrcHost, 0x11, kStubBytes);
    CHECK_ACL(aclrtMemcpy(stubSrcDev, kStubBytes, stubSrcHost, kStubBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(enc_pack_stub)(stubBlockDim, stream, stubDstDev, stubSrcDev, kStubElems, 0);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    CHECK_ACL(aclrtMemcpy(stubDstHost, kStubBytes, stubDstDev, kStubBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst_stub.bin", stubDstHost, kStubBytes);
    if (!ok) return 16;
    std::printf("[EP01] SIM AIV stub done\n");

    CHECK_ACL(aclrtFree(stubSrcDev));
    CHECK_ACL(aclrtFree(stubDstDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(hHost));
    CHECK_ACL(aclrtFreeHost(kBarHost));
    CHECK_ACL(aclrtFreeHost(rHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtFreeHost(stubSrcHost));
    CHECK_ACL(aclrtFreeHost(stubDstHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    std::printf("[EP01] Encaps Host skel finished OK (c=1568 K=32)\n");
    return 0;
}
