/**
 * @file shake128_toy_entry.cpp
 * @brief 设备侧核函数入口（薄入口）：SHAKE128 toy 探针的 `__global__` 核。
 *
 * 流水线位置：`pass-shake128-ops-math-toy` 探针的设备核编译单元，被 `main.cpp`
 * 通过 `ICPU_RUN_KF`（CPU 孪生）或 `shake128_general_do`（SIM/NPU）拉起。
 * 核内**不做** GM 上 x/y 的搬运——消息与期望输出均以编译期常量（`auto_gen/toy_active_case.h`，
 * 由 `gen_data.py` 依据当前 `SHAKE128_CASE` 生成）内嵌到 UB 中自检，仅通过 GM 上的
 * tiling 结构体回写 1 个 PASS/FAIL 标志给 Host，减少 I/O 干扰，聚焦 SHAKE128 原语正确性。
 * 真正的 SHAKE128 + golden 对拍逻辑见 `shake128_toy_ub.hpp::RunActiveCaseUb`。
 */
#include "shake128_toy_ub.hpp"
#include "shake_general_tiling_data.h"

/**
 * SHAKE128 toy 核函数。
 * @param tiling GM 指针，指向 Host 构造的 `ShakeGeneralTilingData`；本核仅使用其
 *               `reserved2` 字段（此处以 `uint32_t` 数组第 8 个元素访问，对应结构体中
 *               该字段的偏移）作为 PASS(1)/FAIL(0) 回写通道，其余 tiling 字段本核不读取
 *               （batch/maxMsgLen/outLen/rate 等均已通过编译期常量固化在 UB 版实现中）。
 * 前置条件：仅 blockIdx==0 的核执行实际计算，其余核直接返回（本探针单核语义，
 * 避免多核重复写同一 GM 地址产生竞争）。
 */
extern "C" __global__ __aicore__ void shake128_general(GM_ADDR tiling)
{
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    /* 核内完成：UB 填充内嵌消息 → 调用共享 SHAKE128 核 → UB 上逐字节比对 golden，返回 1=PASS/0=FAIL */
    const uint32_t pass = Shake128Toy::RunActiveCaseUb();
    /* 将 tiling GM 缓冲区重解释为 uint32_t 数组，下标 8 对应 reserved2 字段偏移，
     * 用于把设备自检结果带回 Host（main.cpp 读回后落盘 output/device_pass.bin） */
    __gm__ uint32_t *tp = reinterpret_cast<__gm__ uint32_t *>(tiling);
    tp[8] = pass;
}

#ifndef __CCE_KT_TEST__
/* SIM/NPU 路径的核启动壳：CPU 孪生（__CCE_KT_TEST__ 宏定义时）由 main.cpp 走 ICPU_RUN_KF 直接调用核函数，
 * 不经过此壳；仅在真实设备/仿真编译时提供 <<<>>> 三重角括号语法的封装供 main.cpp 调用。 */
extern "C" void shake128_general_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tiling)
{
    shake128_general<<<blockDim, l2ctrl, stream>>>(tiling);
}
#endif
