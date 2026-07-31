/**
 * @file main.cpp
 * @brief pass-toy-mix-s123-byteencode-k2 的 host 入口：装填 GM、单趟 launch、落盘中间结果供 verify。
 *
 * GM 参数布局：
 *   out  — 内核写出的最终 int8[4096]
 *   src  — 输入 int32[2048]（golden 用全 0；S1 填数不依赖 src 非零）
 *   ws   — workspace：LUT 由 host 预填；S0/MAT_C 由 kernel 写入
 *
 * mixPass 分阶段：
 *   2 — 跳过 S1，需 input/s0_preset.bin
 *   3 — 跳过 S1+S2，需 input/mat_c_preset.bin
 *
 * CPU 路径：ICPU_RUN_KF + g_toy_mix_pass；SIM/NPU：ACLRT_LAUNCH_KERNEL。
 */
#include "data_utils.h"
#include "tiling.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#include <cstdlib>
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling);
extern volatile int g_toy_mix_pass;
#endif

/**
 * 主流程：读取 tiling/src/lut（及可选的 s0_preset/mat_c_preset）→ 下发 MIX kernel
 * （CPU 孪生 / SIM-NPU 二选一编译分支）→ 落盘最终输出与中间结果（out/s0/mat_c）。
 * @return 0=成功；9/10/13/15=对应输入文件读取失败；12/14/16=对应输出文件写入失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::kSrcTotal * sizeof(int32_t);
    size_t outFileSize = tiling::kOutTotal;
    size_t lutFileSize = tiling::kLutBytes;
    size_t s0FileSize = tiling::kS0Bytes;
    size_t matCFileSize = tiling::kMatCBytes;
    uint32_t blockDim = 1; /**< 单 block MIX：内含 1 AIC + 2 AIV */
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    /* CPU 孪生路径：MIX_MODE 告知 tikicpu 本核为 Cube+Vector 混合核；GM 缓冲
     * 用 AscendC::GmAlloc 模拟设备内存，读取各输入文件后直接 ICPU_RUN_KF 调用
     * 核函数，完成后从同一块「GM」内存落盘（无需显式 Device→Host 拷贝）。 */
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize < sizeof(TilingData)) {
        return 1;
    }
    TilingData *tiling = (TilingData *)tiling_data;

    uint8_t *out = (uint8_t *)AscendC::GmAlloc(outFileSize > 1024 ? outFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    // 右矩阵 B = I₆₄，仅 S2 使用；host 在 launch 前写入 ws+LUT。
    ok = ReadFile("./input/lut.bin", lutFileSize, ws + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    if (tiling->mixPass == 2) {
        /* mixPass=2（仅跑 S2）时 S1 被跳过，需要预置左矩阵 A（S0）供 Cube 直接读取 */
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, ws + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 3) {
        /* mixPass=3（仅跑 S3+encode）时 S1+S2 被跳过，需要预置 Cube 输出 C（MAT_C）供 S3 直接读取 */
        ok = ReadFile("./input/mat_c_preset.bin", matCFileSize, ws + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 15;
        }
    }
    /* CPU 孪生下 tiling 结构体在核内可能读取不可靠，改用全局变量传递 mixPass（见 mmad_custom.cpp 头部说明） */
    g_toy_mix_pass = tiling->mixPass;
    ICPU_RUN_KF(mmad_custom, blockDim, out, src, ws, *tiling);

    ok = WriteFile("./output/out.bin", out, outFileSize);
    if (!ok) {
        return 14;
    }
    // 中间结果落盘，便于 mixPass 分阶段与 verify 对照 golden_s0 / golden_mat_c。
    ok = WriteFile("./output/s0.bin", ws + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }
    ok = WriteFile("./output/mat_c.bin", ws + tiling::MAT_C, matCFileSize);
    if (!ok) {
        return 16;
    }
    AscendC::GmFree((void *)out);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    /* SIM/NPU 路径：acl 初始化 → 建流 → 分配 Host/Device 双份缓冲（out/src/ws）
     * → 读输入文件到 Host 缓冲 → Host→Device 拷入 → ACLRT_LAUNCH_KERNEL 下发
     * → 同步 → Device→Host 拷回 → 落盘。tiling 结构体本身走 Host 侧内存直接传参
     * （ACLRT_LAUNCH_KERNEL 按值传递 TilingData，无需显式拷到 Device）。
     * 设备号：读 ASCEND_DEVICE_ID；缺省 1（借入多卡约定）。SIM 由 run.sh 强制 export=0。 */
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 1;
    if (const char *envDev = std::getenv("ASCEND_DEVICE_ID")) {
        deviceId = static_cast<int32_t>(std::atoi(envDev));
    }
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *outHost, *srcHost, *wsHost;
    uint8_t *outDevice, *srcDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&outHost), outFileSize));
    CHECK_ACL(aclrtMalloc((void **)&outDevice, outFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/lut.bin", lutFileSize, wsHost + tiling::LUT, lutFileSize);
    if (!ok) {
        return 10;
    }
    if (tiling->mixPass == 2) {
        /* 同 CPU 路径：mixPass=2 跳过 S1，预置左矩阵 A 到 Host 侧 ws 缓冲，随后统一拷入 Device */
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, wsHost + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 3) {
        /* 同 CPU 路径：mixPass=3 跳过 S1+S2，预置 Cube 输出 C 到 Host 侧 ws 缓冲 */
        ok = ReadFile("./input/mat_c_preset.bin", matCFileSize, wsHost + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 15;
        }
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, outDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(outHost, outFileSize, outDevice, outFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/out.bin", outHost, outFileSize);
    if (!ok) {
        return 14;
    }
    CHECK_ACL(aclrtMemcpy(wsHost, wsFileSize, wsDevice, wsFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/s0.bin", wsHost + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }
    ok = WriteFile("./output/mat_c.bin", wsHost + tiling::MAT_C, matCFileSize);
    if (!ok) {
        return 16;
    }

    CHECK_ACL(aclrtFree(outDevice));
    CHECK_ACL(aclrtFreeHost(outHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
