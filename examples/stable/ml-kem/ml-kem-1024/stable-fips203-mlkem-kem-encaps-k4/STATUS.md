# STATUS — stable-fips203-mlkem-kem-encaps-k4

**语义**：FIPS 203 **Alg.20 / Alg.17 `ML-KEM.Encaps()`**（ml_kem_1024 / k=4）— **stable 定型交付算子**（自 `exp-fips203-mlkem-kem-encaps-k4` 复制晋级，2026-07-15 `#验收#`）。

| 项 | 值 |
|----|-----|
| **状态** | **定型交付**（CPU + SIM + liboqs KAT 已验） |
| **customspec** | [`stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.pdf) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md) |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored** |
| **KEM 头** | `kem/`：`m` GM → 设备 `H`/`G` → `K`‖`r` |
| **Launch** | SIM **2** / CPU **5** |
| **I/O** | `ek_kem`+`m`+LUT → **仅** `c`+`K` |
| **SIM tick** | **721119**（stable 交付验收）；对标 device **721010** |
| **预研副本** | [`exp-fips203-mlkem-kem-encaps-k4`](../../incubating/exp-fips203-mlkem-kem-encaps-k4/) |
| **行为基线** | [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/) |
| **原理总结** | [`docs/notes/F203-KEM-Alg20-Encaps设备全链技术总结.md`](../../../docs/notes/F203-KEM-Alg20-Encaps设备全链技术总结.md) |

## 验收（晋级门禁 + stable 复验）

| 门禁 | 结果 |
|------|------|
| incubating CPU / SIM / 随机 `m`×3 | PASS（晋级前） |
| incubating liboqs KAT CPU×10 + SIM×3 | PASS（晋级前） |
| **stable** `bash run.sh -r cpu` | **PASS**（`c`/`K` max=0） |
| **stable** `bash run.sh -r sim` | **PASS** tick **721119**；根目录无 stray dump |
| **stable** liboqs KAT CPU×10 + SIM×3 | **PASS**（复跑；首轮 SIM 曾遇一次 exit 139 flake，复测全绿） |

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # 默认 SIM_DIRECT=1；WSL/Cloud 勿再手写
bash scripts/kem_keypair_stash_bootstrap.sh
bash scripts/liboqs_kem_encaps_batch.sh   # 默认 ENCAPS_DIR=本目录
```

## 非本目录职责

- Decaps（T19b/c）；NPU 真机压测
- Encrypt Â / SHA3 后端大改（另开战役）
