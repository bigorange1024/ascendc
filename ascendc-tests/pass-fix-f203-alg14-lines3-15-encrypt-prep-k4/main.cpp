/**
 * @file main.cpp
 * @brief Host：ek_pke + coins → 设备 Alg.14 prep（Â + r/e₁/e₂）→ a_hat.bin + re.bin。
 *
 * 流水线位置（FIPS 203 Alg.14 Encrypt，行 3–15 prep）：
 *   - 输入：input/ek_pke.bin（1568B，尾 32B 为 ρ）、input/coins.bin（32B）
 *   - 设备核 f203_encrypt_prep：ρ→SampleNTT→a_hat[16,256]；coins→PRF+CBD→re[9,256]
 *   - 输出：output/a_hat.bin、output/re.bin（与 golden_a_hat / golden_re 对拍）
 *
 * 与 golden 关系：本文件只做 Host I/O 与 launch；期望值由 scripts/gen_data.py 生成，
 * scripts/verify_result.py 做 max_abs_diff=0 黑盒对拍。禁止把 golden 源码当作设备实现规格。
 *
 * 验收：CPU（__CCE_KT_TEST__）+ SIM/NPU（ACL）双模式；探针
 * pass-fix-f203-alg14-lines3-15-encrypt-prep-k4（2026-07-07 晋级 pass-）。
 */
#include "data_utils.h"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_layout.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void f203_encrypt_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                     uint8_t *coins_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *re_gm,
                                     uint8_t *tiling_gm);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void f203_encrypt_prep(GM_ADDR ek_gm, GM_ADDR coins_gm, GM_ADDR a_hat_gm,
                                                        GM_ADDR prf_out_gm, GM_ADDR re_gm, GM_ADDR tiling_gm);
#endif

namespace {
using namespace F203EncryptPrep;

/** 与 f203_encrypt_prep_layout.h / CMake 中 F203_AHAT16_BLOCK_DIM 一致（默认 2）。 */
constexpr uint32_t kBlockDim = kPrepBlockDim;
/** ShakeGeneralTilingData 字节数；host 填好后整块拷到 tiling GM。 */
constexpr size_t kTilingBytes = sizeof(ShakeGeneralTilingData);

/**
 * 填充 PRF 用 SHAKE256 tiling。
 *
 * 背景：设备侧 f203_se_vector_prf.hpp 以 PRF_MSG_STRIDE=64 做 UB 行布局（8B 对齐），
 * 有效消息长仍为 33（coins[32]‖nonce）。maxMsgLen 必须与 stride 一致，否则 SIM pem_lsu 告警。
 * batch=8：nonce 0–7 一批；nonce 8（e₂）由设备侧单次补跑（见 f203_encrypt_re_prf.hpp）。
 */
void FillPrfTiling(ShakeGeneralTilingData *t)
{
    // maxMsgLen 须与 f203_se_vector_prf.hpp PRF_MSG_STRIDE(64) 一致，非有效消息长 33
    FillShakeTiling(t, 8U, 64U, kPrfBytesPerPoly, SHAKE256_RATE_BYTES);
    t->blockDim = 1U;
}
}  // namespace

/**
 * Host 入口：读 fixtures 安装后的 input → launch prep 核 → 写 output。
 *
 * 分支：
 *   - __CCE_KT_TEST__：CPU 孪生（GmAlloc + ICPU_RUN_KF）
 *   - 否则：ACL Host/Device 缓冲 + f203_encrypt_prep_do（SIM/NPU）
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t rs = 0;

    // Host 侧 tiling：batch=8 SHAKE256，供设备 PRF 前 8 nonce 使用
    ShakeGeneralTilingData tilingHost{};
    FillPrfTiling(&tilingHost);

    std::cout << "[main] f203_encrypt_prep blockDim=" << kBlockDim << " ek=" << kEkBytes << " coins=" << kCoinsSize
              << " a_hat=" << kAHatBytes << " re=" << kReBytes << "\n";

#ifdef __CCE_KT_TEST__
    // ---------- CPU 孪生路径：全部缓冲在 Host 侧 GmAlloc ----------
    uint8_t *ekGm = static_cast<uint8_t *>(AscendC::GmAlloc(kEkBytes));
    uint8_t *coinsGm = static_cast<uint8_t *>(AscendC::GmAlloc(kCoinsSize));
    uint8_t *aHatGm = static_cast<uint8_t *>(AscendC::GmAlloc(kAHatBytes));
    uint8_t *prfGm = static_cast<uint8_t *>(AscendC::GmAlloc(kPrfBytes));
    uint8_t *reGm = static_cast<uint8_t *>(AscendC::GmAlloc(kReBytes));
    uint8_t *tilingGm = static_cast<uint8_t *>(AscendC::GmAlloc(kTilingBytes));

    // 读 ek_pke / coins；尺寸必须与 layout 常量一致
    ReadFile("./input/ek_pke.bin", rs, ekGm, kEkBytes);
    if (rs != kEkBytes) {
        std::cerr << "[FAIL] ek_pke.bin size\n";
        return 1;
    }
    ReadFile("./input/coins.bin", rs, coinsGm, kCoinsSize);
    if (rs != kCoinsSize) {
        std::cerr << "[FAIL] coins.bin size\n";
        return 1;
    }
    std::memcpy(tilingGm, &tilingHost, kTilingBytes);

    // blockDim=2：双 AIV 分片 Â；CPU 调试下 entry 内可能串行跑两次分片
    ICPU_RUN_KF(f203_encrypt_prep, kBlockDim, ekGm, coinsGm, aHatGm, prfGm, reGm, tilingGm);

    // 仅落盘最终产物；prf_out 为中间态，不对拍、不写盘
    if (!WriteFile("./output/a_hat.bin", aHatGm, kAHatBytes) || !WriteFile("./output/re.bin", reGm, kReBytes)) {
        return 2;
    }

    AscendC::GmFree(ekGm);
    AscendC::GmFree(coinsGm);
    AscendC::GmFree(aHatGm);
    AscendC::GmFree(prfGm);
    AscendC::GmFree(reGm);
    AscendC::GmFree(tilingGm);
#else
    // ---------- ACL / SIM / NPU 路径：Host 缓冲 + Device GM ----------
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *ekHost = nullptr;
    uint8_t *coinsHost = nullptr;
    uint8_t *aHatHost = nullptr;
    uint8_t *reHost = nullptr;
    uint8_t *tilingHostBuf = nullptr;
    uint8_t *ekDev = nullptr;
    uint8_t *coinsDev = nullptr;
    uint8_t *aHatDev = nullptr;
    uint8_t *prfDev = nullptr;
    uint8_t *reDev = nullptr;
    uint8_t *tilingDev = nullptr;

    // Host pinned + Device GM：输入 ek/coins/tiling；输出 a_hat/re；prf 仅设备中间
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekHost), kEkBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&coinsHost), kCoinsSize));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&aHatHost), kAHatBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&reHost), kReBytes));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&tilingHostBuf), kTilingBytes));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekDev), kEkBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&coinsDev), kCoinsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&aHatDev), kAHatBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&prfDev), kPrfBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&reDev), kReBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&tilingDev), kTilingBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    ReadFile("./input/ek_pke.bin", rs, ekHost, kEkBytes);
    if (rs != kEkBytes) {
        std::cerr << "[FAIL] ek_pke.bin size\n";
        return 1;
    }
    ReadFile("./input/coins.bin", rs, coinsHost, kCoinsSize);
    if (rs != kCoinsSize) {
        std::cerr << "[FAIL] coins.bin size\n";
        return 1;
    }
    std::memcpy(tilingHostBuf, &tilingHost, kTilingBytes);

    // H2D：仅输入与 tiling；a_hat/re/prf 由核写入
    CHECK_ACL(aclrtMemcpy(ekDev, kEkBytes, ekHost, kEkBytes, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(coinsDev, kCoinsSize, coinsHost, kCoinsSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(tilingDev, kTilingBytes, tilingHostBuf, kTilingBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    f203_encrypt_prep_do(kBlockDim, nullptr, stream, ekDev, coinsDev, aHatDev, prfDev, reDev, tilingDev);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    // D2H：只取最终 a_hat / re
    CHECK_ACL(aclrtMemcpy(aHatHost, kAHatBytes, aHatDev, kAHatBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(reHost, kReBytes, reDev, kReBytes, ACL_MEMCPY_DEVICE_TO_HOST));

    if (!WriteFile("./output/a_hat.bin", aHatHost, kAHatBytes) || !WriteFile("./output/re.bin", reHost, kReBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(ekDev));
    CHECK_ACL(aclrtFree(coinsDev));
    CHECK_ACL(aclrtFree(aHatDev));
    CHECK_ACL(aclrtFree(prfDev));
    CHECK_ACL(aclrtFree(reDev));
    CHECK_ACL(aclrtFree(tilingDev));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFreeHost(coinsHost));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFreeHost(reHost));
    CHECK_ACL(aclrtFreeHost(tilingHostBuf));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
