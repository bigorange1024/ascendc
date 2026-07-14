/*
 * @file main.cpp
 * @brief Host 主程序：pass-shake256-ascendc-toy 探针入口。
 *
 * 流水线位置：读取 gen_data.py 产出的 `input/meta.bin`（batch/maxMsgLen/outLen），
 * 构造 tiling（rate=SHAKE256_RATE_BYTES）并下发核函数 `shake256_general`（CPU 孪生走
 * ICPU_RUN_KF，SIM/NPU 走 acl 下发），核内完成 SHAKE256 计算与内嵌 golden 自检，
 * Host 只回收 1 个 PASS/FAIL 标志（tiling.reserved2）落盘为 `output/device_pass.bin`。
 * 与 SHAKE128 探针 main.cpp 结构完全对称（仅 rate/核函数名不同）。真正的
 * Host↔Python golden 逐字节对拍在 `verify_result.py` 中完成。
 */
#include "data_utils.h"
#include "shake_general_tiling_data.h"
#include "tiling_host.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

#ifndef __CCE_KT_TEST__
#include "acl/acl.h"
extern "C" void shake256_general_do(uint32_t coreDim, void *l2ctrl, void *stream, uint8_t *tiling);
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void shake256_general(GM_ADDR tiling);
#endif

/** 当前用例的规模元信息：batch=消息条数，maxMsgLen=单条消息最大字节数，outLen=每条期望输出字节数。 */
struct CaseMeta {
    uint32_t batch;
    uint32_t maxMsgLen;
    uint32_t outLen;
};

/**
 * 读取 gen_data.py 写出的 `input/meta.bin`（3 个小端 uint32_t：batch/maxMsgLen/outLen）。
 * @param meta [out] 解析结果
 * @return 读取成功且三个字段均 > 0 时返回 true，否则 false
 */
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

/**
 * 主流程：读 meta → 构造 tiling → 下发核函数（CPU 孪生 / SIM-NPU 二选一编译分支）→
 * 落盘 device_pass.bin → 校验 reserved2。
 * @return 0 表示 Host 侧流程与设备自检均成功；非 0 为具体失败阶段的错误码
 */
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

    /* 构造 tiling：rate 固定为 SHAKE256（136B）；blockDim 固定 1；reserved2 先清零 */
    ShakeGeneralTilingData tilingHost{};
    FillShakeTiling(&tilingHost, meta.batch, meta.maxMsgLen, meta.outLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = 1U;
    tilingHost.reserved2 = 0U;

    std::cout << "[main] SHAKE256 batch=" << meta.batch << " maxMsgLen=" << meta.maxMsgLen
              << " outLen=" << meta.outLen << " blockDim=" << tilingHost.blockDim << " rate=" << tilingHost.rate
              << " (UB self-check, no GM x/y)\n";

#ifdef __CCE_KT_TEST__
    /* CPU 孪生：本核为 AIV 向量路径；不切 AIV_MODE 时 AIC/AIV 竞态写 reserved2。 */
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    constexpr uint32_t kCpuLaunchBlockDim = 1U;
    /* CPU 孪生路径：tiling 分配在「GM 模拟」堆上，ICPU_RUN_KF 直接调用核函数，
     * 执行完毕后从同一块内存读回 reserved2。 */
    uint8_t *tiling = static_cast<uint8_t *>(AscendC::GmAlloc(tilingBytes));
    std::memcpy(tiling, &tilingHost, tilingBytes);

    ICPU_RUN_KF(shake256_general, kCpuLaunchBlockDim, tiling);

    std::memcpy(&tilingHost, tiling, tilingBytes);
    const uint32_t pass = tilingHost.reserved2;
    if (!WriteFile("./output/device_pass.bin", &pass, sizeof(pass))) {
        return 4;
    }
    AscendC::GmFree(tiling);
#else
    /* SIM/NPU 路径：acl 初始化 → 建流 → Host/Device 分配 tiling 缓冲 → 拷入 → 下发核函数 →
     * 同步 → 拷回，取回 reserved2。 */
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

    shake256_general_do(tilingHost.blockDim, nullptr, stream, tilingDev);
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

    /* reserved2 由核函数写回：1=UB 内 SHAKE256 输出与内嵌 golden 逐字节一致，0=不一致 */
    if (tilingHost.reserved2 != 1U) {
        std::cerr << "[FAIL] device UB self-check reserved2=" << tilingHost.reserved2 << "\n";
        return 5;
    }
    return 0;
}
