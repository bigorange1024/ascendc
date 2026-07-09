# examples/stable — 定型算子

**前缀**：`stable-<简述>/` 或 `stable-<简述>-vN`。

**规则**（见 Rule）：首版须从 `exp-*` **复制**晋级；修订须**新版本目录**，旧版标 **已取代**。

---

## 当前 stable

| 目录 | 简述 | 主版本 | 备注 |
|------|------|--------|------|
| [stable-fips203-mlkem-pke-keygen-k4/](stable-fips203-mlkem-pke-keygen-k4/) | FIPS 203 **Alg.13 PKE KeyGen** k=4（2 launch；prep 双 AIV 并行 Â）；[customspec](stable-fips203-mlkem-pke-keygen-k4/stable-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-pke-keygen-k4`](../incubating/exp-fips203-mlkem-pke-keygen-k4/) 晋级（2026-06-29）；**CPU ✓ / SIM ✓ / KAT ✓**；SIM **542393**（2026-06-29 验） |
| [stable-fips203-mlkem-pke-encrypt-k4/](stable-fips203-mlkem-pke-encrypt-k4/) | FIPS 203 **Alg.14 PKE Encrypt** k=4（ek+m+coins→**仅 c**）；[customspec](stable-fips203-mlkem-pke-encrypt-k4/stable-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-pke-encrypt-k4`](../incubating/exp-fips203-mlkem-pke-encrypt-k4/) 晋级（2026-07-09）；**SIM 主参考** tick **627590** · CPU 辅助 · KAT×10+1 / roundtrip×10+1 ✓；[交付口径](../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) · [registry](../../docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) |

---

## 维护

新增或取代版本 → 更新上表与 `examples/INDEX.md`。
