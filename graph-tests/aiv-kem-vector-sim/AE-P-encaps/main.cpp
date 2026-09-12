/**
 * @file main.cpp
 * @brief AE-P host：读 ek|m + NTT 表 → aiv_encaps 单 launch → c.bin + K.bin。
 *
 * workspace（y/e1/e2/ahat/that/yhat）仅 GmAlloc，由设备 FO/CBD/SampleNTT/Decode 写入。
 * 禁 -r npu。
 */
#include "data_utils.h"
#include <cstring>

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_aiv_encaps.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void aiv_encaps(GM_ADDR ekGm, GM_ADDR kGm, GM_ADDR yGm, GM_ADDR e1Gm, GM_ADDR e2Gm,
                           GM_ADDR aHatGm, GM_ADDR tHatGm, GM_ADDR mGm, GM_ADDR rootsGm,
                           GM_ADDR mapGm, GM_ADDR invRootsGm, GM_ADDR invMapGm, GM_ADDR gammaGm,
                           GM_ADDR yHatGm, GM_ADDR cGm);
#endif

namespace {
constexpr size_t K = 4;
constexpr size_t N = 256;
constexpr size_t POLY = N * sizeof(int32_t);
constexpr size_t Y_BYTES = K * POLY;
constexpr size_t E1_BYTES = K * POLY;
constexpr size_t E2_BYTES = POLY;
constexpr size_t AHAT_BYTES = K * K * POLY;
constexpr size_t THAT_BYTES = K * POLY;
constexpr size_t M_BYTES = 32;
constexpr size_t EK_BYTES = 1568;
constexpr size_t K_BYTES = 32;
constexpr size_t ROOT_BYTES = 1024 * sizeof(int32_t);
constexpr size_t MAP_BYTES = 1792 * sizeof(uint32_t);
constexpr size_t GAMMA_BYTES = 128 * sizeof(int32_t);
constexpr size_t YHAT_BYTES = K * POLY;
constexpr size_t C_BYTES = 1568;
}  // namespace

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    auto alloc = [](size_t n) -> uint8_t * {
        size_t sz = n > 1024 ? n : 1024;
        uint8_t *p = (uint8_t *)AscendC::GmAlloc(sz);
        std::memset(p, 0, sz);
        return p;
    };
    uint8_t *ek = alloc(EK_BYTES);
    uint8_t *kOut = alloc(K_BYTES);
    uint8_t *m = alloc(M_BYTES);
    uint8_t *roots = alloc(ROOT_BYTES);
    uint8_t *map = alloc(MAP_BYTES);
    uint8_t *invRoots = alloc(ROOT_BYTES);
    uint8_t *invMap = alloc(MAP_BYTES);
    uint8_t *gamma = alloc(GAMMA_BYTES);
    uint8_t *y = alloc(Y_BYTES);
    uint8_t *e1 = alloc(E1_BYTES);
    uint8_t *e2 = alloc(E2_BYTES);
    uint8_t *ahat = alloc(AHAT_BYTES);
    uint8_t *that = alloc(THAT_BYTES);
    uint8_t *yhat = alloc(YHAT_BYTES);
    uint8_t *c = alloc(C_BYTES);

    size_t rs = 0;
    if (!ReadFile("./input/ek_kem.bin", rs, ek, EK_BYTES) || rs != EK_BYTES) return 1;
    if (!ReadFile("./input/m.bin", rs, m, M_BYTES) || rs != M_BYTES) return 2;
    if (!ReadFile("./input/roots.bin", rs, roots, ROOT_BYTES) || rs != ROOT_BYTES) return 3;
    if (!ReadFile("./input/indices.bin", rs, map, MAP_BYTES) || rs != MAP_BYTES) return 4;
    if (!ReadFile("./input/inv_roots.bin", rs, invRoots, ROOT_BYTES) || rs != ROOT_BYTES) return 5;
    if (!ReadFile("./input/inv_indices.bin", rs, invMap, MAP_BYTES) || rs != MAP_BYTES) return 6;
    if (!ReadFile("./input/gammas.bin", rs, gamma, GAMMA_BYTES) || rs != GAMMA_BYTES) return 7;

    ICPU_RUN_KF(aiv_encaps, blockDim, ek, kOut, y, e1, e2, ahat, that, m, roots, map, invRoots,
                invMap, gamma, yhat, c);

    if (!WriteFile("./output/c.bin", c, C_BYTES)) return 8;
    if (!WriteFile("./output/K.bin", kOut, K_BYTES)) return 9;

    AscendC::GmFree((void *)ek);
    AscendC::GmFree((void *)kOut);
    AscendC::GmFree((void *)m);
    AscendC::GmFree((void *)roots);
    AscendC::GmFree((void *)map);
    AscendC::GmFree((void *)invRoots);
    AscendC::GmFree((void *)invMap);
    AscendC::GmFree((void *)gamma);
    AscendC::GmFree((void *)y);
    AscendC::GmFree((void *)e1);
    AscendC::GmFree((void *)e2);
    AscendC::GmFree((void *)ahat);
    AscendC::GmFree((void *)that);
    AscendC::GmFree((void *)yhat);
    AscendC::GmFree((void *)c);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    auto hostAlloc = [](size_t n, uint8_t **p) {
        CHECK_ACL(aclrtMallocHost((void **)p, n));
        std::memset(*p, 0, n);
    };
    auto devAlloc = [](size_t n, uint8_t **p) {
        CHECK_ACL(aclrtMalloc((void **)p, n, ACL_MEM_MALLOC_HUGE_FIRST));
        CHECK_ACL(aclrtMemset(*p, n, 0, n));
    };

    uint8_t *hek = nullptr, *hk = nullptr, *hm = nullptr;
    uint8_t *hroots = nullptr, *hmap = nullptr, *hinvR = nullptr, *hinvM = nullptr;
    uint8_t *hgamma = nullptr, *hc = nullptr;
    uint8_t *dek = nullptr, *dk = nullptr, *dm = nullptr;
    uint8_t *droots = nullptr, *dmap = nullptr, *dinvR = nullptr, *dinvM = nullptr;
    uint8_t *dgamma = nullptr, *dy = nullptr, *de1 = nullptr, *de2 = nullptr;
    uint8_t *dahat = nullptr, *dthat = nullptr, *dyhat = nullptr, *dc = nullptr;

    hostAlloc(EK_BYTES, &hek);
    hostAlloc(K_BYTES, &hk);
    hostAlloc(M_BYTES, &hm);
    hostAlloc(ROOT_BYTES, &hroots);
    hostAlloc(MAP_BYTES, &hmap);
    hostAlloc(ROOT_BYTES, &hinvR);
    hostAlloc(MAP_BYTES, &hinvM);
    hostAlloc(GAMMA_BYTES, &hgamma);
    hostAlloc(C_BYTES, &hc);

    devAlloc(EK_BYTES, &dek);
    devAlloc(K_BYTES, &dk);
    devAlloc(M_BYTES, &dm);
    devAlloc(ROOT_BYTES, &droots);
    devAlloc(MAP_BYTES, &dmap);
    devAlloc(ROOT_BYTES, &dinvR);
    devAlloc(MAP_BYTES, &dinvM);
    devAlloc(GAMMA_BYTES, &dgamma);
    devAlloc(Y_BYTES, &dy);
    devAlloc(E1_BYTES, &de1);
    devAlloc(E2_BYTES, &de2);
    devAlloc(AHAT_BYTES, &dahat);
    devAlloc(THAT_BYTES, &dthat);
    devAlloc(YHAT_BYTES, &dyhat);
    devAlloc(C_BYTES, &dc);

    size_t rs = 0;
    if (!ReadFile("./input/ek_kem.bin", rs, hek, EK_BYTES) || rs != EK_BYTES) return 1;
    if (!ReadFile("./input/m.bin", rs, hm, M_BYTES) || rs != M_BYTES) return 2;
    if (!ReadFile("./input/roots.bin", rs, hroots, ROOT_BYTES) || rs != ROOT_BYTES) return 3;
    if (!ReadFile("./input/indices.bin", rs, hmap, MAP_BYTES) || rs != MAP_BYTES) return 4;
    if (!ReadFile("./input/inv_roots.bin", rs, hinvR, ROOT_BYTES) || rs != ROOT_BYTES) return 5;
    if (!ReadFile("./input/inv_indices.bin", rs, hinvM, MAP_BYTES) || rs != MAP_BYTES) return 6;
    if (!ReadFile("./input/gammas.bin", rs, hgamma, GAMMA_BYTES) || rs != GAMMA_BYTES) return 7;

    CHECK_ACL(aclrtMemcpy(dek, EK_BYTES, hek, EK_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dm, M_BYTES, hm, M_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(droots, ROOT_BYTES, hroots, ROOT_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dmap, MAP_BYTES, hmap, MAP_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dinvR, ROOT_BYTES, hinvR, ROOT_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dinvM, MAP_BYTES, hinvM, MAP_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(dgamma, GAMMA_BYTES, hgamma, GAMMA_BYTES, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(aiv_encaps)
    (blockDim, stream, dek, dk, dy, de1, de2, dahat, dthat, dm, droots, dmap, dinvR, dinvM, dgamma,
     dyhat, dc);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(hc, C_BYTES, dc, C_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(hk, K_BYTES, dk, K_BYTES, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/c.bin", hc, C_BYTES)) return 8;
    if (!WriteFile("./output/K.bin", hk, K_BYTES)) return 9;

    CHECK_ACL(aclrtFree(dek));
    CHECK_ACL(aclrtFree(dk));
    CHECK_ACL(aclrtFree(dm));
    CHECK_ACL(aclrtFree(droots));
    CHECK_ACL(aclrtFree(dmap));
    CHECK_ACL(aclrtFree(dinvR));
    CHECK_ACL(aclrtFree(dinvM));
    CHECK_ACL(aclrtFree(dgamma));
    CHECK_ACL(aclrtFree(dy));
    CHECK_ACL(aclrtFree(de1));
    CHECK_ACL(aclrtFree(de2));
    CHECK_ACL(aclrtFree(dahat));
    CHECK_ACL(aclrtFree(dthat));
    CHECK_ACL(aclrtFree(dyhat));
    CHECK_ACL(aclrtFree(dc));
    CHECK_ACL(aclrtFreeHost(hek));
    CHECK_ACL(aclrtFreeHost(hk));
    CHECK_ACL(aclrtFreeHost(hm));
    CHECK_ACL(aclrtFreeHost(hroots));
    CHECK_ACL(aclrtFreeHost(hmap));
    CHECK_ACL(aclrtFreeHost(hinvR));
    CHECK_ACL(aclrtFreeHost(hinvM));
    CHECK_ACL(aclrtFreeHost(hgamma));
    CHECK_ACL(aclrtFreeHost(hc));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
