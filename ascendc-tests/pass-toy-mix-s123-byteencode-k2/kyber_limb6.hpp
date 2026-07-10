#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief 本探针仅复用 KYBER_PIPE_ALL 宏；不含真 limb6 拆 limb 逻辑（见 aiv_func.hpp 内玩具 limb）。
 *
 * 流水线位置：被 aic_func.hpp / aiv_func.hpp 在 AIC↔AIV CrossCore 同步点前后
 * include 调用，用于保证跨核（AIC 与 AIV）之间通过 SET/WAIT flag 传递的
 * 「完成」语义在真实硬件流水线上是可靠的（即 flag 置位前，之前发出的所有
 * 访存/计算指令均已完成落地）。本文件名沿用自「真 limb6」相关命名，但本探针
 * 是简化玩具（见 TOY_MIX_S123.md「与真 NTT / keygen 的对应」表），并未包含
 * 真正的 limb6 拆分算法。
 *
 * KYBER_PIPE_ALL：在 device 侧于 CrossCore 前后插入 PIPE_ALL barrier；
 * CPU debug 下为空操作，避免 tikicpu 与真机 barrier 行为差异拖慢单测。
 */

#include "kernel_operator.h"

/* ASCENDC_CPU_DEBUG 由 CPU 孪生编译环境定义；该模式下无需真实流水线屏障
 * （CPU 孪生按顺序执行，天然满足屏障语义），定义为空操作以避免不必要开销。
 * 非 CPU 调试（SIM/NPU）下插入 PIPE_ALL 屏障，等待所有流水线阶段清空。 */
#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
