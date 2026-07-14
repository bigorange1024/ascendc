// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/kyber_limb6.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `kyber_limb6.hpp` 为该子模块组件。 / Component: kyber_limb6.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 Stage1 limb6 拆分原语。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/kyber_limb6.hpp
 */
/**
 * @file kyber_limb6.hpp
 * @brief Kyber/ML-KEM limb6 编码常量与 PIPE 屏障宏（Stage1 分裂 / Stage3 Horner 移位）。
 *
 * 用途：kKyberLimbBits=6、kKyberMergeShift{1,2,3}；KYBER_PIPE_ALL 在向量 basemul 遗留路径中同步 V 管道。
 *
 * 调用方：ntt_vec.hpp、mod_variants.hpp、hat_vec.hpp、aiv_func.hpp。
 *
 * 不变量：mask=0x3f；合并 shift 6/12/18 与平面 mat_c 四 limb 行语义一致。
 *
 * Golden：间接经 dst.bin；无独立 limb golden。
 *
 * CMake：无。
 *
 * 背景（2026-07-13 KEM KeyGen landmine）：曾在 ASCENDC_CPU_DEBUG 下把本宏置空，
 * 导致 Fuse/Tail 本核「半新旧」；customspec 强制 **禁止空操作**，CPU/SIM 一律
 * PipeBarrier<PIPE_ALL>。
 */
#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

#include "kernel_operator.h"

constexpr int32_t kKyberLimbBits = 6;
constexpr int32_t kKyberLimbMask = 0x3f;
constexpr int32_t kKyberMergeShift1 = 6;
constexpr int32_t kKyberMergeShift2 = 12;
constexpr int32_t kKyberMergeShift3 = 18;

/** 本核全管道屏障；CPU/SIM/NPU 同定义（禁止 ASCENDC_CPU_DEBUG 空操作）。 */
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

#endif
