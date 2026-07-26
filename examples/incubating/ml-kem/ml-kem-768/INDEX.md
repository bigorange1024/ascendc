# examples/incubating/ml-kem/ml-kem-768 — ML-KEM-768（k=3）预研

**参数组**：FIPS 203 **ML-KEM-768**（\(k=3\)）  
**阶段**：**W4 进行中** — E13/E14 已完成 customspec + incubating 实现；下一刀 E15
**上级**：[../INDEX.md](../INDEX.md)  
**参数卡**：[docs/specs/fips203-mlkem768-parameter-card.md](../../../../docs/specs/fips203-mlkem768-parameter-card.md)  
**P1 表**：[docs/specs/fips203-mlkem768-p1-gap-and-cases.md](../../../../docs/specs/fips203-mlkem768-p1-gap-and-cases.md)  
**探针树**：[ascendc-tests/ml-kem/ml-kem-768/](../../../../ascendc-tests/ml-kem/ml-kem-768/)

| 硬门禁 | 说明 |
|--------|------|
| 写码前 | 须用户指定活跃 `*-customspec.*`（`$规格$` → ascendc-impl-spec） |
| 终点（用户锁定） | **PKE×3 + KEM×3 + decaps-ct**；本阶段 **不做** stable |
| 分核 | **T-B** polyvec6；禁零垫 |

---

## 规划 exp（壳）

| 波次 | 目录 | ID | customspec | CPU | SIM |
|------|------|----|------------|-----|-----|
| W4a | [exp-fips203-mlkem-pke-keygen-k3/](exp-fips203-mlkem-pke-keygen-k3/) | E13 | **有** | **PASS** | **PASS**（tick **373429**） |
| W4a | [exp-fips203-mlkem-pke-encrypt-k3/](exp-fips203-mlkem-pke-encrypt-k3/) | E14 | **有** | **PASS** | **PASS**（tick **507633**） |
| W4a | [exp-fips203-mlkem-pke-decrypt-k3/](exp-fips203-mlkem-pke-decrypt-k3/) | E15 | 缺 | 壳 | 壳 |
| W4b | [exp-fips203-mlkem-kem-keygen-k3/](exp-fips203-mlkem-kem-keygen-k3/) | E19 | 缺 | 壳 | 壳 |
| W4b | [exp-fips203-mlkem-kem-encaps-k3/](exp-fips203-mlkem-kem-encaps-k3/) | E20 | 缺 | 壳 | 壳 |
| W4b | [exp-fips203-mlkem-kem-decaps-k3/](exp-fips203-mlkem-kem-decaps-k3/) | E21 | 缺 | 壳 | 壳 |
| W4b | [exp-fips203-mlkem-kem-decaps-ct-k3/](exp-fips203-mlkem-kem-decaps-ct-k3/) | E21ct | 缺 | 壳 | 壳 |
