/**
 * @file shake256_toy_entry.cpp
 * @brief 设备侧核函数入口（薄入口）：SHAKE256 toy 探针的 `__global__` 核。
 *
 * 流水线位置：`pass-shake256-ascendc-toy` 探针的设备核编译单元，被 `main.cpp`
 * 通过 `ICPU_RUN_KF`（CPU 孪生）或 `shake256_general_do`（SIM/NPU）拉起。
 * 与 SHAKE128 toy 探针同构：核内**不做** GM 上 x/y 的搬运，消息与期望输出均以
 * 编译期常量（`auto_gen/toy_active_case.h`，由 `gen_data.py` 依据当前
 * `SHAKE256_CASE` 生成）内嵌到 UB 中自检，仅通过 GM 上的 tiling 结构体回写
 * 1 个 PASS/FAIL 标志给 Host。SHAKE256 是 ML-KEM PRF/H/G/J 等函数的规范轨基础
 * 原语，实际计算逻辑见 `shake256_toy_ub.hpp::RunActiveCaseUb`。
 */
#include "shake256_toy_ub.hpp"
#include "shake_general_tiling_data.h"

/**
 * SHAKE256 toy 核函数。
 * @param tiling GM 指针，指向 Host 构造的 `ShakeGeneralTilingData`；本核仅使用其
 *               `reserved2` 字段（以 `uint32_t` 数组下标 8 访问）回写 PASS(1)/FAIL(0)。
 * 前置条件：仅 blockIdx==0 的核执行实际计算，其余核直接返回（单核语义）。
 */
extern "C" __global__ __aicore__ void shake256_general(GM_ADDR tiling)
{
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    /* UB 填充内嵌消息 → 调用共享 SHAKE256 核 → UB 上逐字节比对 golden，返回 1=PASS/0=FAIL */
    const uint32_t pass = Shake256Toy::RunActiveCaseUb();
    /* 将 tiling GM 缓冲区重解释为 uint32_t 数组，下标 8 对应 reserved2 字段偏移 */
    __gm__ uint32_t *tp = reinterpret_cast<__gm__ uint32_t *>(tiling);
    tp[8] = pass;
}

#ifndef __CCE_KT_TEST__
/* SIM/NPU 路径的核启动壳；CPU 孪生由 main.cpp 走 ICPU_RUN_KF 直接调用核函数，不经过此壳。 */
extern "C" void shake256_general_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *tiling)
{
    shake256_general<<<blockDim, l2ctrl, stream>>>(tiling);
}
#endif
