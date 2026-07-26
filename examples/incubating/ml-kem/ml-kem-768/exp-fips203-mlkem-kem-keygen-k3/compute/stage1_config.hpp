/**
 * @file stage1_config.hpp
 * @brief F203 Stage1：int32 系数分裂为 hi/lo 各 6-bit limb（int8）的向量化档位。
 *
 * 流水线位置：被 ntt_vec.hpp / aiv_func.hpp（AivK6Split）在编译期读取 F203_STAGE1_SPLIT。
 *
 * 作用：控制 split_vec_* 走标量 / 整块向量 / 分 tile 向量；不改变 limb 数学语义。
 *
 * 不变量：lo = v - hi*64（Sub，非 And）；与 Kyber limb6 编码一致；Stage1 输出供 AIC MMAD。
 *
 * 与 golden 关系：Stage1 不单独作为验收主路径；全链路 dst.bin [6,256] 与 gen_data 一致。
 * mixPass=0 时可 dump s0 与 golden_s0 对照（调试）。
 *
 * CMake：F203_STAGE1_SPLIT（CMakeLists CACHE，cpu_lib/npu_lib 传入内核）。
 */
#ifndef F203_STAGE1_CONFIG_HPP
#define F203_STAGE1_CONFIG_HPP

/**
 * Stage1 int32→limb6 split（hi/lo int8）实现选型。
 *
 *   0 — 标量 GetValue/SetValue（回归对照）
 *   1 — 向量整块：ShiftRight+Muls+Sub+Cast，一次处理整 bank（本探针默认）
 *   2 — 向量分 tile（默认 32）：同上，按 tile 流水
 *
 * 切换：cmake -DF203_STAGE1_SPLIT=1|2 或 run.sh 环境变量；lo 用 Sub 不用 And。
 */
#ifndef F203_STAGE1_SPLIT
#define F203_STAGE1_SPLIT 1
#endif

#endif
