# examples/incubating/ml-kem/ml-kem-512 — ML-KEM-512（k=2）预研

**参数组**：FIPS 203 **ML-KEM-512**（\(k=2\)）  
**阶段**：**P0–P3 完成**；W4 incubating 全绿；**glue（AscendC RT + liboqs-512）全绿**  
**上级**：[../INDEX.md](../INDEX.md)  
**参数卡**：[docs/specs/fips203-mlkem512-parameter-card.md](../../../../docs/specs/fips203-mlkem512-parameter-card.md)  
**P1 表**：[docs/specs/fips203-mlkem512-p1-gap-and-cases.md](../../../../docs/specs/fips203-mlkem512-p1-gap-and-cases.md)  
**探针树**：[ascendc-tests/ml-kem/ml-kem-512/](../../../../ascendc-tests/ml-kem/ml-kem-512/)

| 硬门禁 | 说明 |
|--------|------|
| 写码前 | 须用户指定活跃 `*-customspec.*`（本表路径） |
| 终点（本阶段） | **PKE×3 + KEM×3 + decaps-ct**；**不建** stable-512 |
| 分核 | **S-1** 单 cube；**T-B2** polyvec4；禁零垫 |
| 用语 | **缺项** / **补缺** |

---

## incubating exp（P3 / W4）— CPU + `SIM_DIRECT=1` sim 全绿

| P | W | 目录 | ID | customspec | CPU | SIM（tick 摘录） |
|---|---|------|----|------------|-----|------------------|
| P3 | W4a | `exp-fips203-mlkem-pke-keygen-k2/` | E13 | [tex](exp-fips203-mlkem-pke-keygen-k2/exp-fips203-mlkem-pke-keygen-k2-实现方案-customspec.tex) | PASS | **230036** |
| P3 | W4a | `exp-fips203-mlkem-pke-encrypt-k2/` | E14 | [tex](exp-fips203-mlkem-pke-encrypt-k2/exp-fips203-mlkem-pke-encrypt-k2-实现方案-customspec.tex) | PASS | **366129**（glue-c 后） |
| P3 | W4a | `exp-fips203-mlkem-pke-decrypt-k2/` | E15 | [tex](exp-fips203-mlkem-pke-decrypt-k2/exp-fips203-mlkem-pke-decrypt-k2-实现方案-customspec.tex) | PASS | **168783** |
| P3 | W4b | `exp-fips203-mlkem-kem-keygen-k2/` | E19 | [tex](exp-fips203-mlkem-kem-keygen-k2/exp-fips203-mlkem-kem-keygen-k2-实现方案-customspec.tex) | PASS | **319957** |
| P3 | W4b | `exp-fips203-mlkem-kem-encaps-k2/` | E20 | [tex](exp-fips203-mlkem-kem-encaps-k2/exp-fips203-mlkem-kem-encaps-k2-实现方案-customspec.tex) | PASS | **427927**（glue-c 后） |
| P3 | W4b | `exp-fips203-mlkem-kem-decaps-k2/` | E21 | [tex](exp-fips203-mlkem-kem-decaps-k2/exp-fips203-mlkem-kem-decaps-k2-实现方案-customspec.tex) | PASS（含 reject） | accept D+E **163062+406837**；reject 亦绿 |
| P3 | W4b | `exp-fips203-mlkem-kem-decaps-ct-k2/` | E21ct | [tex](exp-fips203-mlkem-kem-decaps-ct-k2/exp-fips203-mlkem-kem-decaps-ct-k2-实现方案-customspec.tex) | PASS（含 reject） | accept D+E **163062+405028**；reject 亦绿 |

**端到端 glue**：[`scripts/exp_kem512_liboqs_roundtrip.sh`](../../../../scripts/exp_kem512_liboqs_roundtrip.sh)

| 模式 | 结果 |
|------|------|
| AscendC-only CPU×1（含 reject） | **PASS** |
| AscendC-only `SIM_DIRECT=1` SIM×1（含 reject） | **PASS** |
| `USE_LIBOQS=1` CPU×1 | KeyGen/Encaps **c+K**/Decaps accept+reject **全 max=0** |
| `USE_LIBOQS=1` SIM×1 | 同上 **全 PASS**（2026-07-27 glue-c：`r←η1=3`） |
