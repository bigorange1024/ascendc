/**
 * @file decompress_d_config.hpp
 * @brief ML-KEM-512 B1 Decompress_d 子探针的编译期开关总入口。
 *
 * 本文件在流水线中的位置：位于 f203_decompress_d_params.hpp、decompress_d_vec.hpp 的
 * 最底层依赖，只负责把 -D 编译宏（`F203_DECOMPRESS_D`、`DECOMPRESS_D_VEC`）归一化为带
 * 默认值的宏，并对非法 `F203_DECOMPRESS_D` 取值做编译期报错；不含任何计算逻辑。
 * 对齐规范：FIPS 203 §4.2.1 Decompress_d（Eq 4.8），探针验收 d∈{4,5,10,11}（见 IMPLEMENTATION_PLAN.md）。
 * 与 golden 的关系：`F203_DECOMPRESS_D`/`DECOMPRESS_D_VEC` 由 run.sh 透传给 host/device 编译与
 * scripts/gen_data.py（环境变量 `F203_DECOMPRESS_D`），三方须取值一致才能保证 kernel 输出与
 * decompress_d_ref.c 生成的 golden_poly.bin 对拍成立。
 */
#ifndef DECOMPRESS_D_CONFIG_HPP
#define DECOMPRESS_D_CONFIG_HPP

// F203_DECOMPRESS_D：解压位宽 d；未在编译命令行指定时默认取 4。
#ifndef F203_DECOMPRESS_D
#define F203_DECOMPRESS_D 4
#endif

// 0=标量 fallback | 1=默认向量 per-lane（Decrypt 链）；见 docs/notes/F203-Compress-Decompress-向量实现指南.md
// 控制 decompress_d_vec.hpp 中 poly_decompress_local 的实现路径：默认 1（向量线性运算），
// 0 时退化为逐系数标量 fallback，仅用于对照验证。
#ifndef DECOMPRESS_D_VEC
#define DECOMPRESS_D_VEC 1
#endif

// 仅接受 FIPS 203 Encrypt/Decrypt 用到的四个解压位宽；其余取值在编译期直接报错。
#if F203_DECOMPRESS_D != 4 && F203_DECOMPRESS_D != 5 && F203_DECOMPRESS_D != 10 && F203_DECOMPRESS_D != 11
#error "F203_DECOMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
