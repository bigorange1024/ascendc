// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/stage1_config.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `stage1_config.hpp` 为该子模块组件。 / Component: stage1_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file stage1_config.hpp
 * @brief F203 Stage1：int32 系数分裂为 hi/lo 各 6-bit limb（int8）的向量化档位。
 *
 * 用途：F203_STAGE1_SPLIT 控制 ntt_vec.hpp 中 split_vec_* 标量 / 整块向量 / 分 tile 向量。
 *
 * 调用方：`aiv_func.hpp`::AivSplitPolyBatch（经 ntt_vec.hpp）。
 *
 * 不变量：lo = v - hi*64（Sub，非 And）；与 Kyber limb6 编码一致；Stage1 输出供 AIC MMAD。
 *
 * Golden：Stage1 不单独 golden；全链路 dst.bin [12,256] 与 gen_data 一致。
 *
 * CMake：F203_STAGE1_SPLIT（CMakeLists CACHE，cpu_lib/npu_lib 传入内核）。
 */
#ifndef F203_STAGE1_CONFIG_HPP
#define F203_STAGE1_CONFIG_HPP

/**
 * Stage1 int32→limb6 split（hi/lo int8）实现选型。
 *
 *   0 — 标量 GetValue/SetValue（回归对照）
 *   1 — 向量整块：ShiftRight+Muls+Sub+Cast，一次处理整 bank
 *   2 — 向量分 tile（默认 32）：同上，按 tile 流水
 *
 * 切换：cmake -DF203_STAGE1_SPLIT=1|2 或 run.sh 环境变量；lo 用 Sub 不用 And。
 */
#ifndef F203_STAGE1_SPLIT
#define F203_STAGE1_SPLIT 1
#endif

#endif
