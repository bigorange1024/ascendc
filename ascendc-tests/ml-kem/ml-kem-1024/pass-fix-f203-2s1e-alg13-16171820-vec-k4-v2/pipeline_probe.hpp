/**
 * @file pipeline_probe.hpp
 * @brief 开发期流水线中间态探测：CPU printf 抽样系数（不改 GM、不破坏 UB 不变量）。
 *
 * 用途：
 *   - F203_PIPELINE_PROBE=1 且 ASCENDC_CPU_DEBUG：PIPELINE_PROBE_UB_SAMPLE / GM_SAMPLE 宏生效；
 *   - 设备/SIM：不用 printf；改用 mixPass 分段 + output 下 bin + scripts/probe_stage_verify.py。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp` 关键阶段前后（可选插入宏）。
 *
 * 不变量：默认 F203_PIPELINE_PROBE=0；测 tick/SIM 发布对比须关闭；见 docs/notes/F203-2s1e-NTT内积UB融合技术总结.md §8。
 *
 * Golden：probe 不改变 I/O；分段 checkpoint 仍走 verify_result.py mixPass 4/5。
 *
 * CMake：F203_PIPELINE_PROBE（CMakeLists CACHE，cpu_lib/npu_lib 传入）。
 */
#pragma once

#include "integration_config.hpp"
#include "kernel_operator.h"

namespace pipeline_probe {

#if defined(ASCENDC_CPU_DEBUG) && F203_PIPELINE_PROBE >= 1

/**
 * CPU 调试：从 UB LocalTensor 抽样打印 n 个 int32。
 * @param tag 阶段标签；@param t UB 张量；@param off 元素偏移；@param n 抽样个数
 * 前置：仅 ASCENDC_CPU_DEBUG 且 F203_PIPELINE_PROBE≥1；不写 GM。
 */
__aicore__ inline void print_ub_i32_sample(const char *tag, const AscendC::LocalTensor<int32_t> &t, uint32_t off,
                                           int32_t n)
{
    printf("[probe] %s off=%u:", tag, off);
    for (int32_t i = 0; i < n; ++i) {
        printf(" %d", t.GetValue(off + static_cast<uint32_t>(i)));
    }
    printf("\n");
}

/**
 * CPU 调试：从 GM GlobalTensor 抽样打印 n 个 int32（只读）。
 */
__aicore__ inline void print_gm_i32_sample(const char *tag, const AscendC::GlobalTensor<int32_t> &gm, uint32_t off,
                                           int32_t n)
{
    printf("[probe] %s gm_off=%u:", tag, off);
    for (int32_t i = 0; i < n; ++i) {
        printf(" %d", gm.GetValue(off + static_cast<uint32_t>(i)));
    }
    printf("\n");
}

#define PIPELINE_PROBE_UB_SAMPLE(TAG, TENSOR, OFF, N) pipeline_probe::print_ub_i32_sample((TAG), (TENSOR), (OFF), (N))
#define PIPELINE_PROBE_GM_SAMPLE(TAG, GM, OFF, N) pipeline_probe::print_gm_i32_sample((TAG), (GM), (OFF), (N))

#else

#define PIPELINE_PROBE_UB_SAMPLE(TAG, TENSOR, OFF, N) ((void)0)
#define PIPELINE_PROBE_GM_SAMPLE(TAG, GM, OFF, N) ((void)0)

#endif

} // namespace pipeline_probe
