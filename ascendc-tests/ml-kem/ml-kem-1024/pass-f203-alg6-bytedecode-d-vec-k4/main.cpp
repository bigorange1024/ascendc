/**
 * @file main.cpp
 * @brief ByteDecode_d 探针 host 侧驱动：input/encoded.bin → output/comp.bin。
 *        在流水线中的位置：本文件是 CPU 孪生（ASCENDC_CPU_DEBUG）与 NPU/SIM（ACL）两种
 *        运行模式下的唯一入口，负责读入 golden 输入、搬运到 device、launch
 *        byte_decode_d_custom kernel（实现见 byte_decode_d_vec.hpp）、取回结果并写出。
 *        与 golden 的关系：input/encoded.bin 与 output/golden_comp.bin 均由
 *        scripts/gen_data.py（round-trip 经上游 ByteEncode_d ref 生成）产出；本文件产出的
 *        output/comp.bin 由 scripts/verify_result.py 与 golden 逐元素对拍，验证
 *        FIPS 203 Alg.6 语义正确性。
 */
#include "byte_decode_d_config.hpp"
#include "data_utils.h"
#include "f203_mlkem_params.h"

#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_byte_decode_d_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void byte_decode_d_custom(GM_ADDR encoded_in, GM_ADDR comp_out, int32_t coeff_n);
#endif

/**
 * @brief 程序入口：按编译宏区分 CPU 孪生调试路径与真实 ACL（NPU/SIM）路径，
 *        两条路径逻辑对称，均为「读输入 → 搬运 → launch kernel → 搬运 → 写输出」。
 *        无前置条件（单 blockDim=1，无需多核协同）。
 * @param argc/argv 未使用（本探针无命令行参数，路径与 d 值均由环境变量/宏控制）
 * @return 0 成功；1 读输入文件失败；2 写输出文件失败
 */
int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* inBytes：encoded.bin 的字节数，由 byte_decode_d_config.hpp 按当前 d 值派生；
     * compBytes：comp.bin 的字节数 = N 个 int32 系数（ByteDecode_d 还原结果）。 */
    constexpr size_t inBytes = static_cast<size_t>(F203_BYTE_DECODE_POLY_BYTES);
    constexpr size_t compBytes = static_cast<size_t>(F203_MLKEM_N) * sizeof(int32_t);
    uint32_t blockDim = 1;

#ifdef ASCENDC_CPU_DEBUG
    /* CPU 孪生调试路径：用 GmAlloc 模拟 GM 内存（host 进程内直接分配，无需真实拷贝），
     * ICPU_RUN_KF 在 host 进程内以 CPU 指令模拟 AI Core 执行 kernel。 */
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    /* 分配 GM 缓冲区：与 1024 取 max 是为规避 GmAlloc 对极小尺寸的实现限制，
     * 实际读写仍严格按 inBytes/compBytes 界定，不影响算法语义。 */
    uint8_t *encoded_in = (uint8_t *)AscendC::GmAlloc(inBytes > 1024 ? inBytes : 1024);
    uint8_t *comp_out = (uint8_t *)AscendC::GmAlloc(compBytes > 1024 ? compBytes : 1024);

    size_t rs = 0;
    if (!ReadFile("./input/encoded.bin", rs, encoded_in, inBytes) || rs != inBytes) {
        return 1;
    }

    /* encoded_in：GM 语义的输入比特流 uint8[inBytes]；comp_out：GM 语义的输出系数数组 int32[N]；
     * F203_MLKEM_N=256 作为 coeff_n 传入 kernel，供其内部按 n 决定 DataCopy 长度与循环组数。 */
    ICPU_RUN_KF(byte_decode_d_custom, blockDim, encoded_in, comp_out, F203_MLKEM_N);

    if (!WriteFile("./output/comp.bin", comp_out, compBytes)) {
        return 2;
    }

    AscendC::GmFree((void *)encoded_in);
    AscendC::GmFree((void *)comp_out);
#else
    /* 真实 ACL 路径：需显式初始化 runtime、创建 stream，host/device 各自分配缓冲区
     * 并通过 aclrtMemcpy 显式搬运（与 CPU 孪生路径的 GmAlloc 隐式共享内存不同）。 */
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *encodedHost = nullptr;
    uint8_t *compHost = nullptr;
    uint8_t *encodedDevice = nullptr;
    uint8_t *compDevice = nullptr;

    /* Host 侧用 aclrtMallocHost 分配可 DMA 的锁页内存；Device 侧用 aclrtMalloc 分配 GM 显存。 */
    CHECK_ACL(aclrtMallocHost((void **)(&encodedHost), inBytes));
    CHECK_ACL(aclrtMalloc((void **)&encodedDevice, inBytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&compHost), compBytes));
    CHECK_ACL(aclrtMalloc((void **)&compDevice, compBytes, ACL_MEM_MALLOC_HUGE_FIRST));

    size_t rs = 0;
    if (!ReadFile("./input/encoded.bin", rs, encodedHost, inBytes) || rs != inBytes) {
        return 1;
    }
    /* Host → Device：把读入的比特流搬到显存，供 kernel 的 GlobalTensor 读取。 */
    CHECK_ACL(aclrtMemcpy(encodedDevice, inBytes, encodedHost, inBytes, ACL_MEMCPY_HOST_TO_DEVICE));

    /* encodedDevice：GM 语义的输入比特流 uint8[inBytes]；compDevice：GM 语义的输出系数数组；
     * blockDim=1 表示仅用 1 个 AI Core（单 poly 计算量小，无需多核切分）。 */
    ACLRT_LAUNCH_KERNEL(byte_decode_d_custom)(blockDim, stream, encodedDevice, compDevice, F203_MLKEM_N);
    CHECK_ACL(aclrtSynchronizeStream(stream));

    /* Device → Host：把 kernel 写入 GM 的还原系数取回，再落盘到 output/comp.bin。 */
    CHECK_ACL(aclrtMemcpy(compHost, compBytes, compDevice, compBytes, ACL_MEMCPY_DEVICE_TO_HOST));
    if (!WriteFile("./output/comp.bin", compHost, compBytes)) {
        return 2;
    }

    CHECK_ACL(aclrtFree(encodedDevice));
    CHECK_ACL(aclrtFreeHost(encodedHost));
    CHECK_ACL(aclrtFree(compDevice));
    CHECK_ACL(aclrtFreeHost(compHost));
    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
