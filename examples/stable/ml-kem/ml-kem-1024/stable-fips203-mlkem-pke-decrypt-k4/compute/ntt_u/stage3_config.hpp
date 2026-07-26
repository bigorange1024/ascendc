/**
 * @file stage3_config.hpp
 * @brief F203 Stage3.1：RouteA 平面四行 Horner 合并后的 mod q 方案选型。
 *
 * 用途：F203_STAGE3_MOD 绑定 ntt_vec.hpp 末尾 combine_limb6_routea_mod_vec 实现（Barrett / int64 / float Div）。
 *
 * 调用方：`aiv_func.hpp`::AivMergePlanarPoly（经 ntt_vec.hpp + stage3_mod_variants.hpp）。
 *
 * 不变量：Stage3 内禁止 Gather；golden 固定 stage31_mod 数学（方案 0 Barrett 与本探针默认一致）。
 *
 * Golden：output/dst.bin vs golden_dst；mixPass=3 可隔离 Stage3。
 *
 * CMake：F203_STAGE3_MOD（头文件默认 0；改后重编；方案 2 需 aiv_func calc_f TBuf）。
 */
#ifndef F203_STAGE3_CONFIG_HPP
#define F203_STAGE3_CONFIG_HPP

/**
 * Stage3.1 取模方案（仅影响 RouteA 合并后的 mod q，golden 固定为 stage31_mod）。
 *
 *   0 — Barrett 三步 Horner（每步 barrett_reduce，mu=314 k=20）
 *   1 — Horner raw + 标量 Stage31ModI64（ntt_study / exp-mlkem 拓扑）
 *   2 — Horner raw + Cast→float Div→int32 Muls/Sub（当前默认，ONNX 拓扑）
 *
 * 切换：改此宏后重编；方案 2 需在 aiv_func.hpp 保留 calc_f TBuf（已用 #if _guard）。
 */
#ifndef F203_STAGE3_MOD
#define F203_STAGE3_MOD 0
#endif

#endif
