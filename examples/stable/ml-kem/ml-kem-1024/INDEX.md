# examples/stable/ml-kem/ml-kem-1024 — ML-KEM-1024 定型（stable-*）

**参数组**：FIPS 203 **ML-KEM-1024**（k=4）。  
**上级**：[../INDEX.md](../INDEX.md) · [../../INDEX.md](../../INDEX.md)  
**预研副本**：[`examples/incubating/ml-kem/ml-kem-1024/`](../../../incubating/ml-kem/ml-kem-1024/)  
**探针**：[`ascendc-tests/ml-kem/ml-kem-1024/`](../../../../ascendc-tests/ml-kem/ml-kem-1024/)

**前缀**：`stable-<简述>/`。须从活跃 `exp-*` **复制**晋级。

---

## 当前 stable

| 目录 | 简述 | 主版本 | 备注 |
|------|------|--------|------|
| [stable-fips203-mlkem-pke-keygen-k4/](stable-fips203-mlkem-pke-keygen-k4/) | FIPS 203 **Alg.13 PKE KeyGen** k=4（2 launch；prep 双 AIV 并行 Â）；[customspec](stable-fips203-mlkem-pke-keygen-k4/stable-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-pke-keygen-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-keygen-k4/) 晋级（2026-06-29）；**CPU ✓ / SIM ✓ / KAT ✓**；SIM **542393**（2026-06-29 验） |
| [stable-fips203-mlkem-pke-encrypt-k4/](stable-fips203-mlkem-pke-encrypt-k4/) | FIPS 203 **Alg.14 PKE Encrypt** k=4（ek+m+coins→**仅 c**）；[customspec](stable-fips203-mlkem-pke-encrypt-k4/stable-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-pke-encrypt-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/) 晋级（2026-07-09）；**SIM 主参考** tick **627590** · CPU 辅助 · KAT×10+1 / roundtrip×10+1 ✓；[交付口径](../../../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) · [registry](../../../../docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) |
| [stable-fips203-mlkem-pke-decrypt-k4/](stable-fips203-mlkem-pke-decrypt-k4/) | FIPS 203 **Alg.15 PKE Decrypt** k=4（dk+c→**仅 m**）；[customspec](stable-fips203-mlkem-pke-decrypt-k4/stable-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-pke-decrypt-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/) 晋级（2026-07-10）；**CPU ✓ / SIM ✓** tick **283290** · KAT×10+1 / roundtrip×10+1 ✓ |
| [stable-fips203-mlkem-kem-keygen-k4/](stable-fips203-mlkem-kem-keygen-k4/) | FIPS 203 **Alg.19 KEM KeyGen** k=4（2 launch；`seed_d`→`ek_kem`/`dk_kem`）；[customspec](stable-fips203-mlkem-kem-keygen-k4/stable-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-kem-keygen-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/) 晋级（2026-07-14 `#交付#`）；SIM tick **≈707k**；[registry](../../../../docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |
| [stable-fips203-mlkem-kem-encaps-k4/](stable-fips203-mlkem-kem-encaps-k4/) | FIPS 203 **Alg.20 KEM Encaps** k=4（SIM 2 / CPU 5；`ek`+`m`→`c`/`K`；设备 H/G）；[customspec](stable-fips203-mlkem-kem-encaps-k4/stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-fips203-mlkem-kem-encaps-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/) 晋级（2026-07-15 `#验收#`）；SIM tick **≈721k**；[registry](../../../../docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md) |
| [stable-fips203-mlkem-kem-decaps-k4/](stable-fips203-mlkem-kem-decaps-k4/) | FIPS 203 **Alg.21 KEM Decaps** k=4（**T19i SIM 3** / CPU 6；`dk`+`c`→**仅** `K`；设备 G/FO）；[customspec](stable-fips203-mlkem-kem-decaps-k4/stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-…`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4/) 晋级；**T19i `#修改#` PASS**（D**286851**+E**763769**；KAT 10+3 / roundtrip ✓）；三件套↔liboqs：[`scripts/stable_kem_liboqs_roundtrip.sh`](../../../../scripts/stable_kem_liboqs_roundtrip.sh)；[registry](../../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md) |
| [stable-fips203-mlkem-kem-decaps-ct-k4/](stable-fips203-mlkem-kem-decaps-ct-k4/) | FIPS 203 **Alg.21 KEM Decaps** k=4（CT 专题副本；`dk`+`c`→**仅** `K`；SIM `decaps_2session`；设备 FO）；[customspec](stable-fips203-mlkem-kem-decaps-ct-k4/stable-fips203-mlkem-kem-decaps-ct-k4-实现方案-customspec.pdf) | v1 | 自 [`exp-…-decaps-ct-k4`](../../../incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-ct-k4/) 晋级（2026-07-24 CT 专题）；SIM D**286829**+E**763658**；**非** `scripts/` 默认 |


---

## 维护

新增或取代版本 → 更新上表与 `examples/INDEX.md`。
