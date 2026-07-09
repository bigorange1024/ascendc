/**
 * @file stage1_config.hpp
 * @brief NTT/INTT Stage1 编译开关（split 实现变体）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt compute。
 * 默认生产全量；调试改宏须显式覆盖。
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
