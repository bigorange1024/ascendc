/**
 * @file main.cpp
 * @brief Alg.14 pack 探针 host：m/u/v → mu_embed + c（单 launch，AIV_ONLY blockDim=1）。
 *
 * 流水线位置：FIPS 203 Alg.14 行 20（μ 展开）+ 行 22–24（Compress/ByteEncode→密文 c）。
 * 本探针不覆盖行 21（v←v+μ，由 compute 探针负责）；输入 u/v 已是时域系数。
 *
 * golden I/O（与 scripts/gen_data.py / verify_result.py 对齐）：
 *   输入：input/m.bin(32B)、u.bin(K×N int32)、v.bin(N int32)
 *   输出：output/mu_embed.bin(N int32)、c.bin(1568B = c₁‖c₂)
 *
 * CPU 孪生走 ICPU_RUN_KF；SIM/NPU 走 ACL H2D → launch → D2H。
 */
#include "data_utils.h"
#include "f203_encrypt_tail_layout.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_f203_encrypt_alg14_tail.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void f203_encrypt_alg14_tail(GM_ADDR mGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR muEmbedGm, GM_ADDR cGm);
#endif

/**
 * host 入口：装载 m/u/v，launch f203_encrypt_alg14_tail，落盘 mu_embed 与 c。
 * @return 0 成功；1–3 读入失败；4–5 写出失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // 字节数由 layout 宏锁定（ml_kem_1024 / k=4）
    constexpr size_t mBytes = F203_TAIL_MSG_BYTES;
    constexpr size_t uBytes = F203_TAIL_U_BYTES;
    constexpr size_t vBytes = F203_TAIL_V_BYTES;
    constexpr size_t muBytes = F203_TAIL_MU_BYTES;
    constexpr size_t cBytes = F203_TAIL_C_BYTES;
    constexpr uint32_t blockDim = 1U; // AIV_ONLY：仅 block 0 执行

#ifdef ASCENDC_CPU_DEBUG
    // --- CPU 孪生：GmAlloc + ICPU_RUN_KF ---
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    uint8_t *mGm = (uint8_t *)AscendC::GmAlloc(mBytes > 64 ? mBytes : 64);
    uint8_t *uGm = (uint8_t *)AscendC::GmAlloc(uBytes);
    uint8_t *vGm = (uint8_t *)AscendC::GmAlloc(vBytes);
    uint8_t *muGm = (uint8_t *)AscendC::GmAlloc(muBytes);
    uint8_t *cGm = (uint8_t *)AscendC::GmAlloc(cBytes);

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mGm, mBytes) || rs != mBytes) {
        return 1;
    }
    if (!ReadFile("./input/u.bin", rs, uGm, uBytes) || rs != uBytes) {
        return 2;
    }
    if (!ReadFile("./input/v.bin", rs, vGm, vBytes) || rs != vBytes) {
        return 3;
    }

    ICPU_RUN_KF(f203_encrypt_alg14_tail, blockDim, mGm, uGm, vGm, muGm, cGm);

    if (!WriteFile("./output/mu_embed.bin", muGm, muBytes)) {
        return 4;
    }
    if (!WriteFile("./output/c.bin", cGm, cBytes)) {
        return 5;
    }

    AscendC::GmFree((void *)mGm);
    AscendC::GmFree((void *)uGm);
    AscendC::GmFree((void *)vGm);
    AscendC::GmFree((void *)muGm);
    AscendC::GmFree((void *)cGm);
#else
    // --- SIM/NPU：ACL 分配 → H2D → launch → D2H → 落盘 ---
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *mHost = nullptr;
    uint8_t *uHost = nullptr;
    uint8_t *vHost = nullptr;
    uint8_t *muHost = nullptr;
    uint8_t *cHost = nullptr;
    uint8_t *mDev = nullptr;
    uint8_t *uDev = nullptr;
    uint8_t *vDev = nullptr;
    uint8_t *muDev = nullptr;
    uint8_t *cDev = nullptr;

    CHECK_ACL(aclrtMallocHost((void **)(&mHost), mBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&uHost), uBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&vHost), vBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&muHost), muBytes));
    CHECK_ACL(aclrtMallocHost((void **)(&cHost), cBytes));
    CHECK_ACL(aclrtMalloc((void **)&mDev, mBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&uDev, uBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&vDev, vBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&muDev, muBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc((void **)&cDev, cBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/m.bin", rs, mHost, mBytes) || rs != mBytes) {
        return 1;
    }
    if (!ReadFile("./input/u.bin", rs, uHost, uBytes) || rs != uBytes) {
        return 2;
    }
    if (!ReadFile("./input/v.bin", rs, vHost, vBytes) || rs != vBytes) {
        return 3;
    }

    CHECK_ACL(aclrtMemcpy(mDev, mBytes, mHost, mBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(uDev, uBytes, uHost, uBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(vDev, vBytes, vHost, vBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(f203_encrypt_alg14_tail)(blockDim, stream, mDev, uDev, vDev, muDev, cDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(muHost, muBytes, muDev, muBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(cHost, cBytes, cDev, cBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/mu_embed.bin", muHost, muBytes)) {
        return 4;
    }
    if (!WriteFile("./output/c.bin", cHost, cBytes)) {
        return 5;
    }

    CHECK_ACL(aclrtFree(mDev));
    CHECK_ACL(aclrtFree(uDev));
    CHECK_ACL(aclrtFree(vDev));
    CHECK_ACL(aclrtFree(muDev));
    CHECK_ACL(aclrtFree(cDev));
    CHECK_ACL(aclrtFreeHost(mHost));
    CHECK_ACL(aclrtFreeHost(uHost));
    CHECK_ACL(aclrtFreeHost(vHost));
    CHECK_ACL(aclrtFreeHost(muHost));
    CHECK_ACL(aclrtFreeHost(cHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
