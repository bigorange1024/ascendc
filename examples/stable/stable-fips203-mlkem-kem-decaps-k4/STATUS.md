# STATUS — stable-fips203-mlkem-kem-decaps-k4

**语义**：FIPS 203 **Alg.21 / Alg.18 `ML-KEM.Decaps()`**（ml_kem_1024 / k=4）— **stable 定型交付算子**。

| 项 | 值 |
|----|-----|
| **状态** | **定型交付**；**T19i `#修改#` PASS**（2026-07-20；SIM **3** launch） |
| **customspec** | [`stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md) |
| **Decrypt** | 本目录 `decrypt/` **vendored** |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored**；`l18_l19` 以 `kem/` 覆盖为准 |
| **KEM** | `kem/`：`G` 并入 Phase-E prep；SIM FO 并入 `l18_l19` 尾（T19i） |
| **Launch** | SIM **3** / CPU 6 |
| **I/O** | `dk_kem`+`c`+LUT → **仅** `K` |
| **SIM tick（T19i）** | **1050620**（D **286851** + E **763769**） |
| **预研副本** | [`exp-fips203-mlkem-kem-decaps-k4`](../../incubating/exp-fips203-mlkem-kem-decaps-k4/) |
| **行为基线** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) |
| **原理总结** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收

### 晋级复验（2026-07-20，SIM 4）

CPU / SIM / KAT 10+3 / roundtrip（含拒绝）— **PASS**（历史）。

### T19i `#修改#`（2026-07-20，SIM 3）

| 门禁 | 结果 |
|------|------|
| `bash run.sh -r cpu` | **PASS**（`K` max=0） |
| `SIM_DIRECT=1 bash run.sh -r sim` | **PASS**；D**286851**+E**763769**；根无 stray |
| `KEM_DECAPS_REJECT=1` cpu+sim | **PASS** |
| liboqs KAT CPU×10 + SIM×3 | **PASS** |
| roundtrip cpu+sim（agreement + reject） | **PASS** |

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh
bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r sim -v Ascend910B4
```
