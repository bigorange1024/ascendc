// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/stage3_config.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `stage3_config.hpp` 为该子模块组件。 / Component: stage3_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


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
