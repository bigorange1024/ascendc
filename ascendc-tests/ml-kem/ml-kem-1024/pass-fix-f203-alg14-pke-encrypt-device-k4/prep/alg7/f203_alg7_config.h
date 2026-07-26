// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/alg7/f203_alg7_config.h
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_config.h` 为该子模块组件。 / Component: f203_alg7_config.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends prep 子模块头文件 + CANN AscendC；entry 由 f203_keygen_prep_entry 聚合。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。



// @encrypt_probe_note 本文件亦用于 pass-fix-f203-alg14-lines3-15-encrypt-prep-k4 / pke-encrypt-device-k4 的 Alg.14 行 3–15 prep；Encrypt 生产 I/O 为 ek_pke+coins → a_hat+re（见该探针 STATUS），上方 @production_io 若写 keygen ek/dk 为 vendored 来源标签，勿与 Encrypt 验收混淆。
/**
 * @file f203_alg7_config.h
 * @brief Alg.7 SampleNTT 探针编译期开关（CMake CACHE / run.sh -D 注入）。
 *
 * 流水线位置：被 entry、d12_vec、rej_filter 等包含；不定义算法几何（见 f203_alg7_layout.h）。
 *
 * 与 golden 关系：切换 REJ_IMPL / DUMP_XOF / D12_GATHER 会改变设备路径或中间 dump，
 * 但语义须与 scripts/gen_data.py 标量参考一致；验收默认 F203_ALG7_REJ_IMPL=1。
 */
#pragma once

/**
 * rej 全链实现选择（标量路径仅作回归 golden 对照）：
 *   0 — F203_ALG7_REJ_SCALAR：标量 GetValue 顺序 rej（最慢、最易对拍）
 *   1 — F203_ALG7_REJ_VEC_MINS：Mins 向量剔除 d≥q + Gather 交错 + 标量 compact（**生产默认**）
 *   2 — F203_ALG7_REJ_VEC_MASK：Compares(LT)+Select 剔除 + Gather + 标量 compact（实验对照）
 *
 * 背景：剔除段禁止 for+GetValue；compact 段生产仍用标量（向量 compact SIM 未通过，见 rej_compact.hpp）。
 */
#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif

#define F203_ALG7_REJ_SCALAR 0
#define F203_ALG7_REJ_VEC_MINS 1
#define F203_ALG7_REJ_VEC_MASK 2

/**
 * XOF 原始字节 GM 落盘开关：
 *   1 — 链末 DumpXofUbToGm，写出 output/xof.bin 供 golden 对拍 SHAKE 续流
 *   0 — 生产默认跳过，节省 GM 写带宽
 */
#ifndef F203_ALG7_DUMP_XOF
#define F203_ALG7_DUMP_XOF 0
#endif

/**
 * XOF→c0/c1/c2 解交织实现：
 *   1 — Gather+ROM 向量路径（实验，多占 UB）
 *   0 — 标量 GetValue 解交织（生产默认，Phase2 tick 更优）
 */
#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif
