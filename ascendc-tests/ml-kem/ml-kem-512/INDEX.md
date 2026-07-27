# ascendc-tests/ml-kem/ml-kem-512 — ML-KEM-512（k=2）探针

**参数组**：FIPS 203 **ML-KEM-512**（\(k=2\)，\(\eta_1=3,\eta_2=2\)，\(d_u=10,d_v=4\)）  
**阶段**：**P0–P2 完成**；**P2 W0–W3 全绿**（B1–B6，D13–D15，D19–D21ct）；incubating W4+glue 见 [`examples/.../ml-kem-512`](../../../examples/incubating/ml-kem/ml-kem-512/INDEX.md)
**上级**：[../INDEX.md](../INDEX.md)  
**参数卡**：[docs/specs/fips203-mlkem512-parameter-card.md](../../../docs/specs/fips203-mlkem512-parameter-card.md)  
**P1 表**：[docs/specs/fips203-mlkem512-p1-gap-and-cases.md](../../../docs/specs/fips203-mlkem512-p1-gap-and-cases.md)  
**计划**：[docs/research/MLKEM-512-从0到exp完整实现计划.md](../../../docs/research/MLKEM-512-从0到exp完整实现计划.md)  
**对应 incubating**：[examples/incubating/ml-kem/ml-kem-512/](../../../examples/incubating/ml-kem/ml-kem-512/)  
**分核锁定**：**S-1** 单 cube；**T-B2** polyvec4；**禁零垫**

| 规则 | 说明 |
|------|------|
| 命名 | `pass-fix-f203-…-k2` |
| 状态 | 各目录 `STATUS.md` |
| 验收 | CPU + `SIM_DIRECT=1` sim；根无 stray dump |
| 用语 | **缺项** / **补缺**（勿另造同义词） |

---

## 探针表（P1 锁定；实现进度）

| P | W | 目录 | ID | CPU | SIM |
|---|---|------|----|-----|-----|
| P2 | W0 | `pass-fix-f203-compress-decompress-du10-dv4-k2/` | B1 | PASS | PASS（c4/c10=3156/3442；dec4/dec10=3236/3317） |
| P2 | W0 | `pass-fix-f203-byteencode-decode-d-k2/` | B2 | PASS | PASS（enc4 5407 / dec4 9340 / enc10 6629 / dec10 6561 / enc12 17613） |
| P2 | W0 | `pass-fix-f203-alg8-cbd-eta2-k2/` | B3a | PASS | PASS（11377） |
| P2 | W0 | `pass-fix-f203-alg8-cbd-eta3-k2/` | B3b | PASS | PASS（13566） |
| P2 | W1 | `pass-fix-f203-alg7-sample-ntt-k2/` | B4 | PASS（含 2×2 matrix） | PASS（80235） |
| P2 | W1 | `pass-fix-f203-stage123-ntt-intt-polyvec4-k2/` | B5 | PASS（ntt+intt） | PASS（NTT 22921 / INTT 22836） |
| P2 | W1 | `pass-fix-f203-alg11-12-multiply-inner-k2/` | B6 | PASS | PASS（multiply 9290 / inner 12603） |
| P2 | W2 | [`pass-fix-f203-alg13-device-keygen-k2/`](pass-fix-f203-alg13-device-keygen-k2/) | D13 | **PASS** | **PASS（230102）** |
| P2 | W2 | [`pass-fix-f203-alg14-pke-encrypt-device-k2/`](pass-fix-f203-alg14-pke-encrypt-device-k2/) | D14 | **PASS** | **PASS（365995，glue-c 后）** |
| P2 | W2 | [`pass-fix-f203-alg15-pke-decrypt-device-k2/`](pass-fix-f203-alg15-pke-decrypt-device-k2/) | D15 | **PASS** | **PASS（168975）** |
| P2 | W3 | [`pass-fix-f203-alg19-kem-keygen-device-k2/`](pass-fix-f203-alg19-kem-keygen-device-k2/) | D19 | **PASS** | **PASS（320247）** |
| P2 | W3 | [`pass-fix-f203-alg20-kem-encaps-device-k2/`](pass-fix-f203-alg20-kem-encaps-device-k2/) | D20 | **PASS** | **PASS**（旧 tick 394978；glue-c 后待重登） |
| P2 | W3 | [`pass-fix-f203-alg21-kem-decaps-device-k2/`](pass-fix-f203-alg21-kem-decaps-device-k2/) | D21 | **PASS**（accept/reject） | **PASS**（accept **571206** / reject **570547**） |
| P2 | W3 | [`pass-fix-f203-alg21-kem-decaps-device-ct-k2/`](pass-fix-f203-alg21-kem-decaps-device-ct-k2/) | D21ct | **PASS**（accept/reject） | **PASS**（accept **570707** / reject **571369**） |
