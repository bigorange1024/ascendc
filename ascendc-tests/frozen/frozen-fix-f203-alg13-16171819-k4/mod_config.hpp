#ifndef F203_MOD_CONFIG_HPP
#define F203_MOD_CONFIG_HPP

/**
 * 全 testcase 统一取模方案（Stage3 合并 mod、+ê 后 final mod）。
 * Alg.11 basemul 内 Mlkem Barrett 固定，不随此宏变。
 *
 *   0 — 标量 int64 floor %（可能需 int64 中间值）
 *   1 — merged_kyber Barrett 向量约化（mu=314 k=20 + wrap_mod）
 *   2 — ntt_study Cast→float Div→Muls/Sub 向量约化（默认）
 *
 * 切换：改此宏后重编；变体 2 需 calc_f TBuf（3×halfLen float）。
 */
#ifndef F203_MOD_VARIANT
#define F203_MOD_VARIANT 2
#endif

#endif
