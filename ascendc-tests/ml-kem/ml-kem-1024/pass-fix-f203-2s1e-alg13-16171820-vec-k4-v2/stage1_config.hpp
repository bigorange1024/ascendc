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
 * 本文件仅宏选型，实现见 ntt_vec.hpp::split_vec_*。
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
