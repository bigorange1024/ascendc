# ascendc-tests/ml-kem/ml-kem-768 — ML-KEM-768（k=3）探针

**参数组**：FIPS 203 **ML-KEM-768**（\(k=3\)）
**阶段**：**P2（W0–W3）全绿**（积木 + PKE/KEM device + CT）；对应 **P3（W4 + glue）** 已完成
**上级**：[../INDEX.md](../INDEX.md)
**参数卡**：[docs/specs/fips203-mlkem768-parameter-card.md](../../../docs/specs/fips203-mlkem768-parameter-card.md)
**P1 表**：[docs/specs/fips203-mlkem768-p1-gap-and-cases.md](../../../docs/specs/fips203-mlkem768-p1-gap-and-cases.md)
**对应 incubating**：[examples/incubating/ml-kem/ml-kem-768/](../../../examples/incubating/ml-kem/ml-kem-768/)
**分核锁定**：**T-B** polyvec6 + \(\hat A\) 独立 prep；**禁零垫**

| 规则 | 说明 |
|------|------|
| 命名 | `pass-fix-f203-…-k3`（绿后可改 `pass-`） |
| 状态 | 各目录 `STATUS.md` |
| 验收 | CPU + `SIM_DIRECT=1` sim；根无 stray dump |

---

## 探针表

| P | W | 目录 | ID | CPU | SIM |
|---|---|------|----|-----|-----|
| P2 | W0 | [pass-fix-f203-compress-decompress-du10-dv4-k3/](pass-fix-f203-compress-decompress-du10-dv4-k3/) | B1 | **PASS** | **PASS** |
| P2 | W0 | [pass-fix-f203-byteencode-decode-d-k3/](pass-fix-f203-byteencode-decode-d-k3/) | B2 | **PASS** | **PASS** |
| P2 | W0 | [pass-fix-f203-alg8-cbd-eta2-k3/](pass-fix-f203-alg8-cbd-eta2-k3/) | B3 | **PASS** | **PASS** |
| P2 | W1 | [pass-fix-f203-alg7-sample-ntt-k3/](pass-fix-f203-alg7-sample-ntt-k3/) | B4 | **PASS** | **PASS** |
| P2 | W1 | [pass-fix-f203-stage123-ntt-intt-polyvec6-k3/](pass-fix-f203-stage123-ntt-intt-polyvec6-k3/) | B5 | **PASS** | **PASS** |
| P2 | W1 | [pass-fix-f203-alg11-12-multiply-inner-k3/](pass-fix-f203-alg11-12-multiply-inner-k3/) | B6 | **PASS** | **PASS** |
| P2 | W2 | [pass-fix-f203-alg13-device-keygen-k3/](pass-fix-f203-alg13-device-keygen-k3/) | D13 | **PASS** | **PASS** |
| P2 | W2 | [pass-fix-f203-alg14-pke-encrypt-device-k3/](pass-fix-f203-alg14-pke-encrypt-device-k3/) | D14 | **PASS** | **PASS** |
| P2 | W2 | [pass-fix-f203-alg15-pke-decrypt-device-k3/](pass-fix-f203-alg15-pke-decrypt-device-k3/) | D15 | **PASS** | **PASS** |
| P2 | W3 | [pass-fix-f203-alg19-kem-keygen-device-k3/](pass-fix-f203-alg19-kem-keygen-device-k3/) | D19 | **PASS** | **PASS** |
| P2 | W3 | [pass-fix-f203-alg20-kem-encaps-device-k3/](pass-fix-f203-alg20-kem-encaps-device-k3/) | D20 | **PASS** | **PASS** |
| P2 | W3 | [pass-fix-f203-alg21-kem-decaps-device-k3/](pass-fix-f203-alg21-kem-decaps-device-k3/) | D21 | **PASS** | **PASS** |
| P2 | W3 | [pass-fix-f203-alg21-kem-decaps-device-ct-k3/](pass-fix-f203-alg21-kem-decaps-device-ct-k3/) | D21ct | **PASS** | **PASS** |
