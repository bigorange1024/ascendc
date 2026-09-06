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
