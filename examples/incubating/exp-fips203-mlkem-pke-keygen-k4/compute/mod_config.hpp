// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/mod_config.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `mod_config.hpp` 为该子模块组件。 / Component: mod_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 模约化变体配置。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/mod_config.hpp
 */
/**
 * @file mod_config.hpp
 * @brief 行 18 final mod（Σ basemul + ê 之后）设备变体编号；与 Stage3 mod 解耦。
 *
 * 用途：F203_MOD_VARIANT 选择标量 int64 / Barrett 向量 / Cast+Div 向量（mod_variants.hpp 实现）。
 *
 * 调用方：mod_variants.hpp → `2s1e_post_ntt_ub.hpp` 行 18 写 t_hat 前 MOD_Q_I32。
 *
 * 不变量：编号与 hat_inner_product_ref.h::HatModVariant 一致；golden 固定 HAT_MOD_SCALAR_I64=0。
 *
 * Golden：gen_data.py 编译 hat_inner_product_ref.c 时 HAT_GOLDEN_MOD_VARIANT=0，与设备变体无关仍须对拍。
 *
 * CMake：当前仅头文件默认；run.sh 注释提及 F203_MOD_VARIANT 环境覆盖（改宏后重编）。
 */
#ifndef F203_MOD_CONFIG_HPP
#define F203_MOD_CONFIG_HPP

/**
 * 行 18 final mod（Σ basemul + ê 之后）与 Stage3 解耦。
 * 0=标量 int64 %（调试）；1=Barrett 向量（默认交付）；2=Cast+Div 向量。
 * C ref golden 固定 HAT_MOD_SCALAR_I64，与设备变体无关。
 */
#ifndef F203_MOD_VARIANT
#define F203_MOD_VARIANT 1
#endif

#endif
