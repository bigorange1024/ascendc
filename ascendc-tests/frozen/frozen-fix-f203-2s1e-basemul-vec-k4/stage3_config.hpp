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
