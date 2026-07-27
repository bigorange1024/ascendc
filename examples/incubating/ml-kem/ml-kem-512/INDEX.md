# examples/incubating/ml-kem/ml-kem-512 — ML-KEM-512（k=2）预研

**参数组**：FIPS 203 **ML-KEM-512**（\(k=2\)）  
**阶段**：**P0 目录壳已建**；P3（W4）待 customspec + 实现  
**上级**：[../INDEX.md](../INDEX.md)  
**参数卡**：[docs/specs/fips203-mlkem512-parameter-card.md](../../../../docs/specs/fips203-mlkem512-parameter-card.md)  
**P1 表**：[docs/specs/fips203-mlkem512-p1-gap-and-cases.md](../../../../docs/specs/fips203-mlkem512-p1-gap-and-cases.md)  
**探针树**：[ascendc-tests/ml-kem/ml-kem-512/](../../../../ascendc-tests/ml-kem/ml-kem-512/)

| 硬门禁 | 说明 |
|--------|------|
| 写码前 | 须用户指定活跃 `*-customspec.*`（`$规格$` → ascendc-impl-spec） |
| 终点（本阶段） | **PKE×3 + KEM×3 + decaps-ct**；**不建** stable-512 |
| 分核 | **S-1** 单 cube；**T-B2** polyvec4；禁零垫 |
| 用语 | **缺项** / **补缺** |

---

## incubating exp（P1 锁定；待建）

| P | W | 目录 | ID | customspec | CPU | SIM |
|---|---|------|----|------------|-----|-----|
| P3 | W4a | `exp-fips203-mlkem-pke-keygen-k2/` | E13 | 待建 | 待建 | 待建 |
| P3 | W4a | `exp-fips203-mlkem-pke-encrypt-k2/` | E14 | 待建 | 待建 | 待建 |
| P3 | W4a | `exp-fips203-mlkem-pke-decrypt-k2/` | E15 | 待建 | 待建 | 待建 |
| P3 | W4b | `exp-fips203-mlkem-kem-keygen-k2/` | E19 | 待建 | 待建 | 待建 |
| P3 | W4b | `exp-fips203-mlkem-kem-encaps-k2/` | E20 | 待建 | 待建 | 待建 |
| P3 | W4b | `exp-fips203-mlkem-kem-decaps-k2/` | E21 | 待建 | 待建 | 待建 |
| P3 | W4b | `exp-fips203-mlkem-kem-decaps-ct-k2/` | E21ct | 待建 | 待建 | 待建 |

**端到端（P3 glue）**：`scripts/exp_kem512_*_roundtrip.sh`（待建；至少 AscendC-only；本阶段 liboqs-512 交叉必达）。
