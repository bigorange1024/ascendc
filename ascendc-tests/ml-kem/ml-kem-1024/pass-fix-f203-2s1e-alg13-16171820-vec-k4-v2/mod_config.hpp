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
 * 本文件仅宏选型，实现见 mod_variants.hpp。
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
