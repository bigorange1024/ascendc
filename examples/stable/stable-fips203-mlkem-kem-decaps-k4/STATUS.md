# STATUS — stable-fips203-mlkem-kem-decaps-k4

**语义**：FIPS 203 **Alg.21 / Alg.18 `ML-KEM.Decaps()`**（ml_kem_1024 / k=4）— **stable 定型交付算子**（自 `exp-fips203-mlkem-kem-decaps-k4` 复制晋级，2026-07-20 `#交付#`）。

| 项 | 值 |
|----|-----|
| **状态** | **定型交付**（CPU + SIM + roundtrip + liboqs KAT 已验） |
| **customspec** | [`stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md) |
| **Decrypt** | 本目录 `decrypt/` **vendored** |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored** |
| **KEM** | `kem/`：`G` 并入 Phase-E prep；设备 FO（SIM 过渡 `fo_only`） |
| **Launch** | SIM **4** / CPU **6** |
| **I/O** | `dk_kem`+`c`+LUT → **仅** `K` |
| **SIM tick（stable 复验）** | **1032762**（D **286896** + E **745866**）；roundtrip accept **1032754**=D**286736**+E**746018** |
| **预研副本** | [`exp-fips203-mlkem-kem-decaps-k4`](../../incubating/exp-fips203-mlkem-kem-decaps-k4/) |
| **行为基线** | [`pass-probe-f203-alg21-kem-decaps-device-k4`](../../../ascendc-tests/pass-probe-f203-alg21-kem-decaps-device-k4/) |
| **原理总结** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收（晋级门禁 + stable 复验，2026-07-20）

| 门禁 | 结果 |
|------|------|
| incubating CPU / SIM / roundtrip / KAT | PASS（晋级前） |
| **stable** `bash run.sh -r cpu` | **PASS**（`K` max=0） |
| **stable** `bash run.sh -r sim` | **PASS** tick **1032762**；根目录无 stray dump |
| **stable** liboqs KAT CPU×10 + SIM×3 | **PASS**（CPU 首轮曾一轮 flake，复测 10/10；SIM 3/3） |
| **stable** roundtrip CPU+SIM（含拒绝） | **PASS**（agreement + reject） |

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash scripts/kem_keypair_stash_bootstrap.sh   # 若 stash 缺失
KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh
bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r sim -v Ascend910B4
```

## 非本目录职责

- SIM `fo_only` 收回 `l18_l19` 尾（4→3 launch）
- NPU 真机压测
